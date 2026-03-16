import { useEffect, useState } from 'react';
import type {
  ChatMessage,
  CompanionSession,
  CreateMessageResponse,
  DeviceLoginResponse,
  CreateSessionRequest,
  OpenClawStatus,
  PetState,
  SessionMessageCompletedPayload,
  SessionMessageDeltaPayload,
  SessionMessageStartedPayload,
  WsEnvelope
} from '@desktop-companion/shared';
import { ChatPanel } from './app/features/chat/ChatPanel';
import { PetStatusCard } from './app/features/pet/PetStatusCard';

const apiBase = import.meta.env.VITE_API_BASE_URL ?? 'http://localhost:3000';

const getPlatform = () => {
  const platform = navigator.platform.toLowerCase();
  if (platform.includes('mac')) {
    return 'macos' as const;
  }
  if (platform.includes('linux')) {
    return 'linux' as const;
  }
  return 'windows' as const;
};

const getDeviceId = () => {
  const stored = window.localStorage.getItem('desktop-companion-device-id');
  if (stored) {
    return stored;
  }

  const nextId = crypto.randomUUID();
  window.localStorage.setItem('desktop-companion-device-id', nextId);
  return nextId;
};

export default function App() {
  const [petState, setPetState] = useState<PetState | null>(null);
  const [openclawStatus, setOpenclawStatus] = useState<OpenClawStatus | null>(null);
  const [session, setSession] = useState<CompanionSession | null>(null);
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [draft, setDraft] = useState('帮我整理今天的工作和休息安排');
  const [isSending, setIsSending] = useState(false);
  const [accessToken, setAccessToken] = useState<string | null>(null);

  const authHeaders = accessToken
    ? {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${accessToken}`
      }
    : null;

  useEffect(() => {
    const init = async () => {
      const loginResponse = await fetch(`${apiBase}/api/v1/auth/device-login`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          deviceId: getDeviceId(),
          deviceName: 'desktop-companion',
          platform: getPlatform()
        })
      }).then((response) => response.json() as Promise<DeviceLoginResponse>);

      setAccessToken(loginResponse.accessToken);

      const nextHeaders = {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${loginResponse.accessToken}`
      };

      const [pet, provider, nextSession] = await Promise.all([
        fetch(`${apiBase}/api/v1/pet/state`, {
          headers: nextHeaders
        }).then((response) => response.json() as Promise<PetState>),
        fetch(`${apiBase}/api/v1/openclaw/status`, {
          headers: nextHeaders
        }).then((response) => response.json() as Promise<OpenClawStatus>),
        fetch(`${apiBase}/api/v1/sessions`, {
          method: 'POST',
          headers: nextHeaders,
          body: JSON.stringify({ provider: 'openclaw', title: '桌面伴侣对话' } satisfies CreateSessionRequest)
        }).then((response) => response.json() as Promise<CompanionSession>)
      ]);

      setPetState(pet);
      setOpenclawStatus(provider);
      setSession(nextSession);
    };

    void init();
  }, []);

  useEffect(() => {
    if (!session || !accessToken) {
      return;
    }

    const wsBase = new URL('/api/v1/ws', apiBase.replace(/^http/, 'ws'));
    wsBase.searchParams.set('accessToken', accessToken);
    const socket = new WebSocket(wsBase.toString());

    socket.onmessage = (event) => {
      const envelope = JSON.parse(event.data) as WsEnvelope<
        OpenClawStatus | SessionMessageStartedPayload | SessionMessageDeltaPayload | SessionMessageCompletedPayload
      >;

      if (envelope.event === 'openclaw.status.changed') {
        setOpenclawStatus(envelope.payload as OpenClawStatus);
        return;
      }

      if (envelope.sessionId !== session.id) {
        return;
      }

      if (envelope.event === 'session.message.started') {
        const payload = envelope.payload as SessionMessageStartedPayload;
        setIsSending(true);
        setMessages((current) => [
          ...current,
          {
            id: payload.messageId,
            sessionId: session.id,
            role: 'assistant',
            content: '',
            createdAt: envelope.timestamp
          }
        ]);
        return;
      }

      if (envelope.event === 'session.message.delta') {
        const payload = envelope.payload as SessionMessageDeltaPayload;
        setMessages((current) =>
          current.map((message) =>
            message.id === payload.messageId
              ? { ...message, content: `${message.content}${payload.delta}` }
              : message
          )
        );
        return;
      }

      if (envelope.event === 'session.message.completed') {
        const payload = envelope.payload as SessionMessageCompletedPayload;
        setIsSending(false);
        setMessages((current) =>
          current.map((message) =>
            message.id === payload.messageId ? { ...message, content: payload.content } : message
          )
        );
      }
    };

    return () => {
      socket.close();
    };
  }, [accessToken, session]);

  const handleSend = async () => {
    if (!session || !draft.trim() || isSending || !authHeaders) {
      return;
    }

    const content = draft.trim();
    const optimisticMessage: ChatMessage = {
      id: crypto.randomUUID(),
      sessionId: session.id,
      role: 'user',
      content,
      createdAt: new Date().toISOString()
    };

    setMessages((current) => [...current, optimisticMessage]);
    setDraft('');
    setIsSending(true);

    await fetch(`${apiBase}/api/v1/sessions/${session.id}/messages`, {
      method: 'POST',
      headers: authHeaders,
      body: JSON.stringify({ content, stream: true })
    }).then((response) => response.json() as Promise<CreateMessageResponse>);
  };

  return (
    <main className="shell">
      <PetStatusCard petState={petState} openclawStatus={openclawStatus} />
      <ChatPanel
        messages={messages}
        draft={draft}
        isSending={isSending}
        onDraftChange={setDraft}
        onSend={() => {
          void handleSend();
        }}
      />
    </main>
  );
}
