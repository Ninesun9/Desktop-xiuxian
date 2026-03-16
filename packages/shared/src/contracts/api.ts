export type DeviceLoginRequest = {
  deviceId: string;
  deviceName: string;
  platform: 'windows' | 'macos' | 'linux';
};

export type DeviceLoginResponse = {
  accessToken: string;
  refreshToken: string;
  user: {
    id: string;
    name: string;
  };
};

export type PetState = {
  id: string;
  mood: 'idle' | 'happy' | 'busy' | 'sleepy' | 'alert';
  energy: number;
  intimacy: number;
  level: number;
  skin: string;
  statusText?: string;
};

export type OpenClawStatus = {
  provider: 'openclaw';
  connected: boolean;
  capabilities: Array<'chat' | 'stream' | 'tools' | 'memory' | 'presence' | 'task-execution'>;
};

export type CompanionSession = {
  id: string;
  userId: string;
  provider: 'openclaw';
  title?: string;
  summary?: string;
  createdAt: string;
  updatedAt: string;
};

export type CreateSessionRequest = {
  provider: 'openclaw';
  title?: string;
};

export type ChatMessage = {
  id: string;
  sessionId: string;
  role: 'user' | 'assistant' | 'system';
  content: string;
  createdAt: string;
};

export type ChatMessageListResponse = {
  items: ChatMessage[];
};

export type CreateMessageRequest = {
  content: string;
  stream: boolean;
};

export type CreateMessageResponse = {
  accepted: true;
  requestId: string;
  messageId: string;
};

export type WsEventName =
  | 'session.message.started'
  | 'session.message.delta'
  | 'session.message.completed'
  | 'companion.presence.changed'
  | 'openclaw.status.changed';

export type WsEnvelope<TPayload = Record<string, unknown>> = {
  event: WsEventName;
  traceId: string;
  sessionId?: string;
  timestamp: string;
  payload: TPayload;
};

export type SessionMessageStartedPayload = {
  messageId: string;
  role: 'assistant';
};

export type SessionMessageDeltaPayload = {
  messageId: string;
  delta: string;
};

export type SessionMessageCompletedPayload = {
  messageId: string;
  content: string;
};
