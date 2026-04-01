import crypto from 'node:crypto';
import fs from 'node:fs';
import Fastify from 'fastify';
import multipart from '@fastify/multipart';
import { z } from 'zod';
import type {
  ChatMessage,
  ChatMessageListResponse,
  AckDirectMessageRequest,
  ChatMessageRole,
  ContactListResponse,
  ContactRequest,
  ContactRequestListResponse,
  CreateDirectAttachmentMessageResponse,
  CompanionSession,
  ConversationListResponse,
  CreateContactRequest,
  CreateDirectConversationRequest,
  CreateDirectMessageRequest,
  CreateMessageRequest,
  CreateMessageResponse,
  CreateSessionRequest,
  DeviceLoginRequest,
  DeviceLoginResponse,
  DirectMessageAttachmentEncryption,
  DirectMessageListResponse,
  PetState,
  TransportKeyResponse,
  UpdateTransportKeyRequest
} from '@desktop-companion/shared';
import { socialStore } from './lib/socialStore.js';
import Database from 'better-sqlite3';

const db = new Database('data.db');
db.exec(`
  CREATE TABLE IF NOT EXISTS pet_states (
    userId TEXT PRIMARY KEY,
    mood TEXT,
    energy REAL,
    intimacy REAL,
    level INTEGER,
    skin TEXT,
    statusText TEXT,
    jingjie INTEGER DEFAULT 0,
    xiuwei REAL DEFAULT 0
  )
`);

const prGetPet = db.prepare('SELECT * FROM pet_states WHERE userId = ?');
const prInsertPet = db.prepare(`
  INSERT INTO pet_states (userId, mood, energy, intimacy, level, skin, statusText, jingjie, xiuwei)
  VALUES (@userId, @mood, @energy, @intimacy, @level, @skin, @statusText, @jingjie, @xiuwei)
`);
const prUpdatePet = db.prepare(`
  UPDATE pet_states SET
    mood = @mood, energy = @energy, intimacy = @intimacy, level = @level,
    skin = @skin, statusText = @statusText, jingjie = @jingjie, xiuwei = @xiuwei
  WHERE userId = @userId
`);
const prDeletePet = db.prepare('DELETE FROM pet_states WHERE userId = ?');
// For rankings, we join with socialStore or just return userIds if simpler. (Here we'll just return userIds and stats for the top 10).
const prGetRankings = db.prepare('SELECT userId, level, jingjie, xiuwei FROM pet_states ORDER BY xiuwei DESC LIMIT 10');


const app = Fastify({ logger: true });

const ACCESS_TOKEN_TTL_MS = 1000 * 60 * 60 * 12;
const SESSION_TTL_MS = 1000 * 60 * 60 * 24;
const MAX_AUTH_SESSIONS = 200;
const MAX_COMPANION_SESSIONS = 200;
const MAX_MESSAGES_PER_SESSION = 400;

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
  role: z.enum(['user', 'assistant', 'system']).default('user'),
  content: z.string().min(1).max(4000),
  stream: z.boolean().default(false)
});

const createContactRequestSchema = z.object({
  target: z.string().min(1),
  remark: z.string().max(40).optional()
});

const createDirectConversationSchema = z.object({
  participantUserId: z.string().min(1)
});

const createDirectMessageSchema = z.object({
  kind: z.literal('text').default('text'),
  content: z.string().min(1).max(2000),
  requiresAck: z.boolean().optional().default(false),
  assistantMeta: z
    .object({
      source: z.literal('openclaw'),
      action: z.enum(['polish', 'suggest-reply']),
      model: z.string().max(100).optional()
    })
    .optional()
});

const createDirectAttachmentManifestSchema = z.object({
  kind: z.enum(['file', 'skill']),
  content: z.string().max(2000).optional().default(''),
  requiresAck: z.boolean().optional().default(true),
  sha256: z.string().min(16).max(128).optional(),
  targetDir: z.string().max(255).optional(),
  encryption: z
    .object({
      algorithm: z.literal('X25519-HKDF-AESGCM'),
      salt: z.string().min(8).max(512),
      nonce: z.string().min(8).max(512),
      senderPublicKey: z.string().min(16).max(512)
    })
    .optional()
});

const updateTransportKeySchema = z.object({
  publicKey: z.string().min(16).max(512)
});

const ackDirectMessageSchema = z.object({
  note: z.string().max(512).optional(),
  installState: z
    .object({
      status: z.enum(['pending', 'installed', 'failed']),
      targetDir: z.string().max(255).optional(),
      note: z.string().max(512).optional()
    })
    .optional()
});

type AuthSession = {
  accessToken: string;
  refreshToken: string;
  userId: string;
  deviceId: string;
  expiresAt: number;
};

const authSessions = new Map<string, AuthSession>();
const sessions = new Map<string, CompanionSession>();
const messagesBySession = new Map<string, ChatMessage[]>();

declare module 'fastify' {
  interface FastifyRequest {
    authSession?: AuthSession;
  }
}

app.addHook('onRequest', async (request, reply) => {
  reply.header('Access-Control-Allow-Origin', '*');
  reply.header('Access-Control-Allow-Headers', 'Content-Type, Authorization');
  reply.header('Access-Control-Allow-Methods', 'GET,POST,PATCH,OPTIONS');

  if (request.method === 'OPTIONS') {
    return reply.code(204).send();
  }
});

await app.register(multipart, {
  limits: {
    fileSize: 25 * 1024 * 1024,
    files: 1
  }
});

const createDefaultPetState = (): PetState => ({
  id: 'pet_001',
  mood: 'idle',
  energy: 83,
  intimacy: 42,
  level: 3,
  skin: 'default',
  statusText: '准备开工',
  jingjie: 0,
  xiuwei: 0
});

const createOpaqueToken = (prefix: string) => `${prefix}_${crypto.randomBytes(24).toString('hex')}`;

const hashToUserId = (deviceId: string) =>
  `user_${crypto.createHash('sha256').update(deviceId).digest('hex').slice(0, 16)}`;

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

const getPetStateForUser = (userId: string): PetState => {
  const row = prGetPet.get(userId) as any;
  if (row) {
    return {
      id: row.userId,
      mood: row.mood,
      energy: row.energy,
      intimacy: row.intimacy,
      level: row.level,
      skin: row.skin,
      statusText: row.statusText,
      jingjie: row.jingjie ?? 0,
      xiuwei: row.xiuwei ?? 0
    };
  }

  const nextState: PetState = {
    ...createDefaultPetState(),
    id: userId
  };
  
  prInsertPet.run({
    userId,
    mood: nextState.mood,
    energy: nextState.energy,
    intimacy: nextState.intimacy,
    level: nextState.level,
    skin: nextState.skin,
    statusText: nextState.statusText,
    jingjie: nextState.jingjie,
    xiuwei: nextState.xiuwei
  });
  
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
  socialStore.touchUserProfile(authSession.userId);
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

  socialStore.upsertUserProfile(userId, payload.deviceName);

  return reply.send(response);
});

app.get('/api/v1/social/contacts', async (request): Promise<ContactListResponse> => socialStore.listContacts(request.authSession!.userId));

app.get('/api/v1/social/contact-requests', async (request): Promise<ContactRequestListResponse> =>
  socialStore.listPendingContactRequests(request.authSession!.userId)
);

app.get('/api/v1/social/transport-key', async (request): Promise<TransportKeyResponse> =>
  socialStore.getOwnTransportKey(request.authSession!.userId)
);

app.post('/api/v1/social/transport-key', async (request, reply) => {
  try {
    const payload = updateTransportKeySchema.parse(request.body) as UpdateTransportKeyRequest;
    const result = socialStore.updateTransportPublicKey(request.authSession!.userId, payload.publicKey);
    return reply.code(202).send(result);
  } catch (error) {
    const message = error instanceof Error ? error.message : '更新传输公钥失败';
    return reply.code(400).send({ code: 'TRANSPORT_KEY_UPDATE_FAILED', message });
  }
});

app.post('/api/v1/social/contact-requests', async (request, reply) => {
  try {
    const payload = createContactRequestSchema.parse(request.body) as CreateContactRequest;
    const nextRequest = socialStore.createContactRequest(request.authSession!.userId, payload);
    return reply.code(201).send(nextRequest);
  } catch (error) {
    const message = error instanceof Error ? error.message : '发送通讯录申请失败';
    const code = /已/.test(message) ? 409 : /不存在|名字/.test(message) ? 404 : 400;
    return reply.code(code).send({ code: 'CONTACT_REQUEST_FAILED', message });
  }
});

app.post('/api/v1/social/contact-requests/:requestId/accept', async (request, reply) => {
  const params = request.params as { requestId: string };
  const current = socialStore.respondContactRequest(params.requestId, request.authSession!.userId, 'accept');
  if (!current) {
    return reply.code(404).send({ code: 'REQUEST_NOT_FOUND', message: '联系人申请不存在' });
  }
  return reply.send(current);
});

app.post('/api/v1/social/contact-requests/:requestId/reject', async (request, reply) => {
  const params = request.params as { requestId: string };
  const current = socialStore.respondContactRequest(params.requestId, request.authSession!.userId, 'reject');
  if (!current) {
    return reply.code(404).send({ code: 'REQUEST_NOT_FOUND', message: '联系人申请不存在' });
  }
  return reply.send(current);
});

app.get('/api/v1/social/conversations', async (request): Promise<ConversationListResponse> =>
  socialStore.listConversations(request.authSession!.userId)
);

app.post('/api/v1/social/conversations', async (request, reply) => {
  try {
    const payload = createDirectConversationSchema.parse(request.body) as CreateDirectConversationRequest;
    if (!socialStore.areContacts(request.authSession!.userId, payload.participantUserId)) {
      return reply.code(403).send({ code: 'CONTACT_REQUIRED', message: '需要先互加通讯录才能私聊' });
    }
    const conversation = socialStore.getOrCreateConversation(request.authSession!.userId, payload.participantUserId);
    return reply.code(201).send(conversation);
  } catch (error) {
    const message = error instanceof Error ? error.message : '创建私聊会话失败';
    return reply.code(/不存在/.test(message) ? 404 : 403).send({ code: 'CONVERSATION_CREATE_FAILED', message });
  }
});

app.get('/api/v1/social/conversations/:conversationId/messages', async (request, reply): Promise<DirectMessageListResponse | ReturnType<typeof reply.code>> => {
  const params = request.params as { conversationId: string };
  const conversation = socialStore.getConversationForUser(params.conversationId, request.authSession!.userId);
  if (!conversation) {
    return reply.code(404).send({ code: 'CONVERSATION_NOT_FOUND', message: '私聊会话不存在' });
  }

  return socialStore.listMessages(params.conversationId, request.authSession!.userId);
});

app.post('/api/v1/social/conversations/:conversationId/read', async (request, reply) => {
  const params = request.params as { conversationId: string };
  const conversation = socialStore.getConversationForUser(params.conversationId, request.authSession!.userId);
  if (!conversation) {
    return reply.code(404).send({ code: 'CONVERSATION_NOT_FOUND', message: '私聊会话不存在' });
  }

  socialStore.markConversationRead(params.conversationId, request.authSession!.userId);
  return reply.send({ accepted: true });
});

app.post('/api/v1/social/conversations/:conversationId/messages', async (request, reply) => {
  const params = request.params as { conversationId: string };
  const conversation = socialStore.getConversationForUser(params.conversationId, request.authSession!.userId);
  if (!conversation) {
    return reply.code(404).send({ code: 'CONVERSATION_NOT_FOUND', message: '私聊会话不存在' });
  }

  try {
    const payload = createDirectMessageSchema.parse(request.body) as CreateDirectMessageRequest;
    const nextMessage = socialStore.createTextMessage(
      params.conversationId,
      request.authSession!.userId,
      payload.content,
      payload.assistantMeta,
      payload.requiresAck
    );
    return reply.code(201).send(nextMessage);
  } catch (error) {
    const message = error instanceof Error ? error.message : '发送私聊失败';
    return reply.code(400).send({ code: 'MESSAGE_CREATE_FAILED', message });
  }
});

app.post('/api/v1/social/messages/:messageId/ack', async (request, reply) => {
  const params = request.params as { messageId: string };

  try {
    const payload = ackDirectMessageSchema.parse(request.body ?? {}) as AckDirectMessageRequest;
    const message = socialStore.acknowledgeMessage(params.messageId, request.authSession!.userId, payload);
    if (!message) {
      return reply.code(404).send({ code: 'MESSAGE_NOT_FOUND', message: '消息不存在或无权确认' });
    }
    return reply.send(message);
  } catch (error) {
    const message = error instanceof Error ? error.message : '确认回执失败';
    return reply.code(400).send({ code: 'MESSAGE_ACK_FAILED', message });
  }
});

app.post('/api/v1/social/conversations/:conversationId/attachments', async (request, reply) => {
  const params = request.params as { conversationId: string };
  const conversation = socialStore.getConversationForUser(params.conversationId, request.authSession!.userId);
  if (!conversation) {
    return reply.code(404).send({ code: 'CONVERSATION_NOT_FOUND', message: '私聊会话不存在' });
  }

  const parts = request.parts();
  let manifestJson = '';
  let uploadBuffer = Buffer.from([]);
  let uploadName = '';
  let contentType = 'application/octet-stream';

  for await (const part of parts) {
    if (part.type === 'file') {
      uploadName = part.filename;
      contentType = part.mimetype;
      uploadBuffer = Buffer.from(await part.toBuffer());
      continue;
    }

    if (part.fieldname === 'manifest') {
      manifestJson = String(part.value ?? '');
    }
  }

  if (!uploadName || uploadBuffer.byteLength === 0) {
    return reply.code(400).send({ code: 'ATTACHMENT_REQUIRED', message: '请上传一个文件' });
  }

  let payload: {
    kind: 'file' | 'skill';
    content: string;
    requiresAck: boolean;
    sha256?: string;
    targetDir?: string;
    encryption?: DirectMessageAttachmentEncryption;
  };

  try {
    payload = createDirectAttachmentManifestSchema.parse(JSON.parse(manifestJson || '{}'));
  } catch (error) {
    const message = error instanceof Error ? error.message : '附件 manifest 无效';
    return reply.code(400).send({ code: 'ATTACHMENT_MANIFEST_INVALID', message });
  }

  if (payload.kind === 'skill' && !uploadName.toLowerCase().endsWith('.zip')) {
    return reply.code(400).send({ code: 'SKILL_ZIP_REQUIRED', message: 'skill 传递目前只接受 zip 包' });
  }

  try {
    const nextMessage = socialStore.createAttachmentMessage({
      conversationId: params.conversationId,
      senderUserId: request.authSession!.userId,
      kind: payload.kind,
      content: payload.content,
      originalName: uploadName,
      contentType,
      bytes: uploadBuffer,
      sha256: payload.sha256,
      requiresAck: payload.requiresAck,
      encryption: payload.encryption,
      installHint: payload.kind === 'skill'
        ? {
            unpackArchive: true,
            targetDir: payload.targetDir || `/root/.openclaw/skills/${uploadName.replace(/\.zip$/i, '')}`
          }
        : undefined
    });
    return reply.code(201).send(nextMessage satisfies CreateDirectAttachmentMessageResponse);
  } catch (error) {
    const message = error instanceof Error ? error.message : '上传附件失败';
    return reply.code(400).send({ code: 'ATTACHMENT_CREATE_FAILED', message });
  }
});

app.get('/api/v1/social/messages/:messageId/attachments/:attachmentId', async (request, reply) => {
  const params = request.params as { messageId: string; attachmentId: string };
  const conversationId = request.query && typeof request.query === 'object' && 'conversationId' in request.query
    ? String((request.query as Record<string, unknown>).conversationId)
    : null;
  if (!conversationId) {
    return reply.code(400).send({ code: 'CONVERSATION_REQUIRED', message: '缺少 conversationId' });
  }

  const attachment = socialStore.getAttachmentForUser(conversationId, params.messageId, params.attachmentId, request.authSession!.userId);
  if (!attachment) {
    return reply.code(404).send({ code: 'ATTACHMENT_NOT_FOUND', message: '附件不存在' });
  }

  const attachmentName = String(attachment.name);
  const attachmentContentType = String(attachment.content_type);
  const attachmentPath = String(attachment.stored_path);

  reply.header('Content-Type', attachmentContentType);
  reply.header('Content-Disposition', `attachment; filename="${encodeURIComponent(attachmentName)}"`);
  return reply.send(fs.createReadStream(attachmentPath));
});

app.get('/api/v1/pet/state', async (request) => getPetStateForUser(request.authSession!.userId));

app.patch('/api/v1/pet/state', async (request) => {
  const payload = patchPetStateSchema.parse(request.body);
  const currentState = getPetStateForUser(request.authSession!.userId);
  const nextState = {
    ...currentState,
    ...payload
  };
  
  prUpdatePet.run({
    userId: request.authSession!.userId,
    mood: nextState.mood,
    energy: nextState.energy,
    intimacy: nextState.intimacy,
    level: nextState.level,
    skin: nextState.skin,
    statusText: nextState.statusText,
    jingjie: nextState.jingjie,
    xiuwei: nextState.xiuwei
  });
  
  return nextState;
});

app.get('/api/v1/pet/rankings', async () => {
  const rows = prGetRankings.all();
  return { items: rows };
});

app.delete('/api/v1/pet/state', async (request, reply) => {
  prDeletePet.run(request.authSession!.userId);
  return reply.code(204).send();
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
  const role = (payload.role ?? 'user') as ChatMessageRole;
  const message: ChatMessage = {
    id: crypto.randomUUID(),
    sessionId: params.sessionId,
    role,
    content: payload.content,
    createdAt
  };

  const items = messagesBySession.get(params.sessionId) ?? [];
  items.push(message);
  if (items.length > MAX_MESSAGES_PER_SESSION) {
    items.splice(0, items.length - MAX_MESSAGES_PER_SESSION);
  }
  messagesBySession.set(params.sessionId, items);

  session.updatedAt = createdAt;
  if (role === 'assistant') {
    session.summary = payload.content;
  }
  sessions.set(params.sessionId, session);

  const response: CreateMessageResponse = {
    accepted: true,
    requestId: crypto.randomUUID(),
    messageId: message.id
  };

  return reply.code(202).send(response);
});

const start = async () => {
  const port = Number(process.env.PORT ?? 3000);
  const host = process.env.HOST ?? '0.0.0.0';
  await app.listen({ port, host });
};

start().catch((error) => {
  app.log.error(error);
  process.exit(1);
});
