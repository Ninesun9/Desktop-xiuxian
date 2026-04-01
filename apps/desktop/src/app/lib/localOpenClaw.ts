import type { ChatMessage, OpenClawStatus } from '@desktop-companion/shared';

const DEFAULT_CAPABILITIES: OpenClawStatus['capabilities'] = ['chat', 'stream', 'tools', 'memory', 'presence'];

type LocalOpenClawRequest = {
  sessionId: string;
  userId: string;
  message: string;
  history: Array<Pick<ChatMessage, 'role' | 'content'>>;
};

interface LocalOpenClawAdapter {
  getStatus(): Promise<OpenClawStatus>;
  generateResponse(request: LocalOpenClawRequest): Promise<string>;
}

const normalizeCapabilities = (capabilities?: Array<string>): OpenClawStatus['capabilities'] => {
  if (!capabilities || capabilities.length === 0) {
    return DEFAULT_CAPABILITIES;
  }

  const validCapabilities = capabilities.filter(
    (capability): capability is OpenClawStatus['capabilities'][number] =>
      ['chat', 'stream', 'tools', 'memory', 'presence', 'task-execution'].includes(capability)
  );

  return validCapabilities.length > 0 ? validCapabilities : DEFAULT_CAPABILITIES;
};

class MockLocalOpenClawAdapter implements LocalOpenClawAdapter {
  async getStatus(): Promise<OpenClawStatus> {
    return {
      provider: 'openclaw',
      connected: true,
      capabilities: DEFAULT_CAPABILITIES
    };
  }

  async generateResponse(request: LocalOpenClawRequest): Promise<string> {
    const lastTurns = request.history.slice(-3).map((item) => `${item.role}: ${item.content}`).join(' | ');
    return `本地 OpenClaw 还没有接好。我先记下这条请求：${request.message}。最近上下文：${lastTurns || '无'}。`;
  }
}

class HttpLocalOpenClawAdapter implements LocalOpenClawAdapter {
  constructor(
    private readonly baseUrl: string,
    private readonly apiKey: string | undefined,
    private readonly model: string | undefined
  ) {}

  async getStatus(): Promise<OpenClawStatus> {
    try {
      const response = await fetch(`${this.baseUrl}/status`, {
        headers: this.buildHeaders()
      });

      if (!response.ok) {
        throw new Error(`status request failed: ${response.status}`);
      }

      const payload = (await response.json()) as Partial<OpenClawStatus> & {
        capabilities?: Array<string>;
      };

      return {
        provider: 'openclaw',
        connected: Boolean(payload.connected),
        capabilities: normalizeCapabilities(payload.capabilities)
      };
    } catch {
      return {
        provider: 'openclaw',
        connected: false,
        capabilities: DEFAULT_CAPABILITIES
      };
    }
  }

  async generateResponse(request: LocalOpenClawRequest): Promise<string> {
    const response = await fetch(`${this.baseUrl}/chat`, {
      method: 'POST',
      headers: this.buildHeaders(),
      body: JSON.stringify({
        sessionId: request.sessionId,
        userId: request.userId,
        model: this.model,
        input: request.message,
        history: request.history
      })
    });

    if (!response.ok) {
      throw new Error(`local openclaw request failed: ${response.status}`);
    }

    const payload = (await response.json()) as { content?: string };
    if (!payload.content || !payload.content.trim()) {
      throw new Error('local openclaw response content is empty');
    }

    return payload.content;
  }

  private buildHeaders() {
    const headers: Record<string, string> = {
      'Content-Type': 'application/json'
    };

    if (this.apiKey) {
      headers.Authorization = `Bearer ${this.apiKey}`;
    }

    return headers;
  }
}

const createLocalOpenClawAdapter = (): LocalOpenClawAdapter => {
  const provider = import.meta.env.VITE_LOCAL_OPENCLAW_PROVIDER ?? 'mock';
  const baseUrl = import.meta.env.VITE_LOCAL_OPENCLAW_BASE_URL?.replace(/\/$/, '');

  if (provider === 'http' && baseUrl) {
    return new HttpLocalOpenClawAdapter(
      baseUrl,
      import.meta.env.VITE_LOCAL_OPENCLAW_API_KEY,
      import.meta.env.VITE_LOCAL_OPENCLAW_MODEL
    );
  }

  return new MockLocalOpenClawAdapter();
};

const localOpenClawAdapter = createLocalOpenClawAdapter();

export const readLocalOpenClawStatus = () => localOpenClawAdapter.getStatus();

export const requestLocalOpenClawResponse = (request: LocalOpenClawRequest) =>
  localOpenClawAdapter.generateResponse(request);
