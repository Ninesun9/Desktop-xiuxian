import crypto from 'node:crypto';
import Fastify from 'fastify';
import { z } from 'zod';
import { WebSocket, WebSocketServer } from 'ws';
import type {
  ChatMessage,
  ChatMessageListResponse,
  CompanionSession,
  CreateMessageRequest,
  CreateMessageResponse,
  CreateSessionRequest,
  DeviceLoginRequest,
  DeviceLoginResponse,
  OpenClawStatus,
  PetState,
  SessionMessageCompletedPayload,
  SessionMessageDeltaPayload,
  SessionMessageStartedPayload,
  WsEnvelope
} from '@desktop-companion/shared';
import { createOpenClawAdapter, streamOpenClawResponse } from './modules/openclaw';

const app = Fastify({ logger: true });

const ACCESS_TOKEN_TTL_MS = 1000 * 60 * 60 * 12;
const SESSION_TTL_MS = 1000 * 60 * 60 * 24;
const MAX_AUTH_SESSIONS = 200;
const MAX_COMPANION_SESSIONS = 200;
const MAX_MESSAGES_PER_SESSION = 200;

const deviceLoginSchema = z.object({
  deviceId: z.string().min(1),
  deviceName: z.string().min(1),
  platform: z.enum(['windows', 'macos', 'linux'])
});

const patchPetStateSchema = z.object({
  mood: z.enum(['idle', 'happy', 'busy', 'sleepy', 'alert']).optional(),
  statusText: z.string().max(255).optional()
});

const createSessionSchema = z.object({
  provider: z.literal('openclaw'),
  title: z.string().max(100).optional()
});

const createMessageSchema = z.object({
  content: z.string().min(1).max(4000),
  stream: z.boolean().default(true)
});

type AuthSession = {
  accessToken: string;
  refreshToken: string;
  userId: string;
  deviceId: string;
  expiresAt: number;
};

type WsClientMeta = {
  userId: string;
};

const authSessions = new Map<string, AuthSession>();

const createDefaultPetState = (): PetState => ({
  id: 'pet_001',
  mood: 'idle',
  energy: 83,
  intimacy: 42,
  level: 3,
  skin: 'default',
  statusText: '准备开工'
});

const petStates = new Map<string, PetState>();
const sessions = new Map<string, CompanionSession>();
const messagesBySession = new Map<string, ChatMessage[]>();

const wss = new WebSocketServer({ noServer: true });
const wsClients = new Map<WebSocket, WsClientMeta>();
const openClawAdapter = createOpenClawAdapter();

declare module 'fastify' {
  interface FastifyRequest {
    authSession?: AuthSession;
  }
}

const createOpaqueToken = (prefix: string) => `${prefix}_${crypto.randomBytes(24).toString('hex')}`;

const hashToUserId = (deviceId: string) => `user_${crypto.createHash('sha256').update(deviceId).digest('hex').slice(0, 16)}`;

const pruneAuthSessions = () => {
  const now = Date.now();
  for (const [token, authSession] of authSessions.entries()) {
    if (authSession.expiresAt <= now) {
      authSessions.delete(token);
    }
  }

  while (authSessions.size > MAX_AUTH_SESSIONS) {
    const firstEntry = authSessions.keys().next();
    if (firstEntry.done) {
      break;
    }
    authSessions.delete(firstEntry.value);
  }
};

const pruneCompanionSessions = () => {
  const now = Date.now();
  for (const [sessionId, session] of sessions.entries()) {
    const lastTouched = Date.parse(session.updatedAt);
    if (Number.isNaN(lastTouched) || now - lastTouched <= SESSION_TTL_MS) {
      continue;
    }
    sessions.delete(sessionId);
    messagesBySession.delete(sessionId);
  }

  while (sessions.size > MAX_COMPANION_SESSIONS) {
    const firstEntry = sessions.keys().next();
    if (firstEntry.done) {
      break;
    }
    messagesBySession.delete(firstEntry.value);
    sessions.delete(firstEntry.value);
  }
};

const getPetStateForUser = (userId: string) => {
  const existing = petStates.get(userId);
  if (existing) {
    return existing;
  }
  const nextState = createDefaultPetState();
  petStates.set(userId, nextState);
  return nextState;
};

const authenticateAccessToken = (token?: string | null) => {
  if (!token) {
    return null;
  }

  pruneAuthSessions();
  const authSession = authSessions.get(token);
  if (!authSession) {
    return null;
  }

  if (authSession.expiresAt <= Date.now()) {
    authSessions.delete(token);
    return null;
  }

  return authSession;
};

const readBearerToken = (authorizationHeader?: string) => {
  if (!authorizationHeader) {
    return null;
  }

  const [scheme, token] = authorizationHeader.split(' ');
  if (scheme?.toLowerCase() !== 'bearer' || !token) {
    return null;
  }

  return token;
};

const broadcastEvent = <TPayload>(envelope: WsEnvelope<TPayload>, userId?: string) => {
  const payload = JSON.stringify(envelope);
  for (const client of wss.clients) {
    if (client.readyState === WebSocket.OPEN) {
      const meta = wsClients.get(client);
      if (userId && meta?.userId !== userId) {
        continue;
      }
      client.send(payload);
    }
  }
};

const buildFallbackAssistantMessage = (input: string) => `OpenClaw 当前不可用。我先记录这条请求：${input}。你可以稍后重试，或者检查 OpenClaw gateway 配置。`;

const resolveOpenClawStatus = async () => openClawAdapter.getStatus();

const scheduleAssistantStream = async (sessionId: string, userId: string, requestContent: string) => {
  const messageId = crypto.randomUUID();
  const traceId = crypto.randomUUID();
  const createdAt = new Date().toISOString();

  const startedEvent: WsEnvelope<SessionMessageStartedPayload> = {
    event: 'session.message.started',
    traceId,
    sessionId,
    timestamp: createdAt,
    payload: {
      messageId,
      role: 'assistant'
    }
  };
  broadcastEvent(startedEvent, userId);

  let assembled = '';

  try {
    const history = (messagesBySession.get(sessionId) ?? []).map(({ role, content }) => ({ role, content }));
    for await (const chunk of streamOpenClawResponse(openClawAdapter, {
      sessionId,
      userId,
      traceId,
      message: requestContent,
      history
    })) {
      assembled += chunk;
      const deltaEvent: WsEnvelope<SessionMessageDeltaPayload> = {
        event: 'session.message.delta',
        traceId,
        sessionId,
        timestamp: new Date().toISOString(),
        payload: {
          messageId,
          delta: chunk
        }
      };
      broadcastEvent(deltaEvent, userId);
    }
  } catch (error) {
    app.log.error(error);
    const fallback = buildFallbackAssistantMessage(requestContent);
    assembled = fallback;
    const deltaEvent: WsEnvelope<SessionMessageDeltaPayload> = {
      event: 'session.message.delta',
      traceId,
      sessionId,
      timestamp: new Date().toISOString(),
      payload: {
        messageId,
        delta: fallback
      }
    };
    broadcastEvent(deltaEvent, userId);
  }

  const assistantMessage: ChatMessage = {
    id: messageId,
    sessionId,
    role: 'assistant',
    content: assembled,
    createdAt: new Date().toISOString()
  };
  const items = messagesBySession.get(sessionId) ?? [];
  items.push(assistantMessage);
  if (items.length > MAX_MESSAGES_PER_SESSION) {
    items.splice(0, items.length - MAX_MESSAGES_PER_SESSION);
  }
  messagesBySession.set(sessionId, items);

  const session = sessions.get(sessionId);
  if (session) {
    session.updatedAt = assistantMessage.createdAt;
    session.summary = assembled;
    sessions.set(sessionId, session);
  }

  const completedEvent: WsEnvelope<SessionMessageCompletedPayload> = {
    event: 'session.message.completed',
    traceId,
    sessionId,
    timestamp: assistantMessage.createdAt,
    payload: {
      messageId,
      content: assembled
    }
  };
  broadcastEvent(completedEvent, userId);
};

app.addHook('preHandler', async (request, reply) => {
  const pathname = request.url.split('?')[0];
  if (pathname === '/health' || pathname === '/api/v1/auth/device-login') {
    return;
  }

  pruneCompanionSessions();

  const token = readBearerToken(request.headers.authorization);
  const authSession = authenticateAccessToken(token);
  if (!authSession) {
    return reply.code(401).send({ code: 'UNAUTHORIZED', message: '未授权' });
  }

  request.authSession = authSession;
});

app.get('/health', async () => ({ ok: true }));

app.post('/api/v1/auth/device-login', async (request, reply) => {
  const payload = deviceLoginSchema.parse(request.body) as DeviceLoginRequest;

  pruneAuthSessions();

  const userId = hashToUserId(payload.deviceId);
  const accessToken = createOpaqueToken('access');
  const refreshToken = createOpaqueToken('refresh');
  const authSession: AuthSession = {
    accessToken,
    refreshToken,
    userId,
    deviceId: payload.deviceId,
    expiresAt: Date.now() + ACCESS_TOKEN_TTL_MS
  };

  authSessions.set(accessToken, authSession);

  const response: DeviceLoginResponse = {
    accessToken,
    refreshToken,
    user: {
      id: userId,
      name: payload.deviceName
    }
  };

  return reply.send(response);
});

app.get('/api/v1/pet/state', async (request) => getPetStateForUser(request.authSession!.userId));

app.patch('/api/v1/pet/state', async (request) => {
  const payload = patchPetStateSchema.parse(request.body);
  const currentState = getPetStateForUser(request.authSession!.userId);
  const nextState = {
    ...currentState,
    ...payload
  };
  petStates.set(request.authSession!.userId, nextState);
  return nextState;
});

app.post('/api/v1/sessions', async (request, reply) => {
  const payload = createSessionSchema.parse(request.body) as CreateSessionRequest;
  const now = new Date().toISOString();
  const session: CompanionSession = {
    id: crypto.randomUUID(),
    userId: request.authSession!.userId,
    provider: payload.provider,
    title: payload.title,
    createdAt: now,
    updatedAt: now
  };

  sessions.set(session.id, session);
  messagesBySession.set(session.id, []);

  return reply.code(201).send(session);
});

app.get('/api/v1/sessions/:sessionId/messages', async (request, reply) => {
  const params = request.params as { sessionId: string };
  const session = sessions.get(params.sessionId);
  if (!session || session.userId !== request.authSession!.userId) {
    return reply.code(404).send({ code: 'SESSION_NOT_FOUND', message: '会话不存在' });
  }

  const response: ChatMessageListResponse = {
    items: messagesBySession.get(params.sessionId) ?? []
  };

  return response;
});

app.post('/api/v1/sessions/:sessionId/messages', async (request, reply) => {
  const params = request.params as { sessionId: string };
  const session = sessions.get(params.sessionId);
  if (!session || session.userId !== request.authSession!.userId) {
    return reply.code(404).send({ code: 'SESSION_NOT_FOUND', message: '会话不存在' });
  }

  const payload = createMessageSchema.parse(request.body) as CreateMessageRequest;
  const createdAt = new Date().toISOString();
  const userMessage: ChatMessage = {
    id: crypto.randomUUID(),
    sessionId: params.sessionId,
    role: 'user',
    content: payload.content,
    createdAt
  };

  const items = messagesBySession.get(params.sessionId) ?? [];
  items.push(userMessage);
  if (items.length > MAX_MESSAGES_PER_SESSION) {
    items.splice(0, items.length - MAX_MESSAGES_PER_SESSION);
  }
  messagesBySession.set(params.sessionId, items);

  session.updatedAt = createdAt;
  sessions.set(params.sessionId, session);

  void scheduleAssistantStream(params.sessionId, request.authSession!.userId, payload.content);

  const response: CreateMessageResponse = {
    accepted: true,
    requestId: crypto.randomUUID(),
    messageId: userMessage.id
  };

  return reply.code(202).send(response);
});

app.get('/api/v1/openclaw/status', async () => resolveOpenClawStatus());

const start = async () => {
  const port = Number(process.env.PORT ?? 3000);
  const host = process.env.HOST ?? '0.0.0.0';
  app.server.on('upgrade', (request, socket, head) => {
    const pathname = new URL(request.url ?? '/', `http://${request.headers.host}`).pathname;
    if (pathname !== '/api/v1/ws') {
      socket.destroy();
      return;
    }

    const url = new URL(request.url ?? '/', `http://${request.headers.host}`);
    const authSession = authenticateAccessToken(url.searchParams.get('accessToken'));
    if (!authSession) {
      socket.destroy();
      return;
    }

    wss.handleUpgrade(request, socket, head, (ws: WebSocket) => {
      wsClients.set(ws, { userId: authSession.userId });
      ws.on('close', () => {
        wsClients.delete(ws);
      });
      wss.emit('connection', ws, request);
      void resolveOpenClawStatus().then((status) => {
        ws.send(
          JSON.stringify({
            event: 'openclaw.status.changed',
            traceId: crypto.randomUUID(),
            timestamp: new Date().toISOString(),
            payload: status
          } satisfies WsEnvelope<OpenClawStatus>)
        );
      });
    });
  });
  await app.listen({ port, host });
};

start().catch((error) => {
  app.log.error(error);
  process.exit(1);
});
