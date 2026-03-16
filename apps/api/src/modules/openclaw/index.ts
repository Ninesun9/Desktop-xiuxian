import type { ChatMessage, OpenClawStatus } from '@desktop-companion/shared';

const DEFAULT_CAPABILITIES: OpenClawStatus['capabilities'] = ['chat', 'stream', 'tools', 'memory', 'presence'];

type OpenClawChatRequest = {
  sessionId: string;
  userId: string;
  traceId: string;
  message: string;
  history: Array<Pick<ChatMessage, 'role' | 'content'>>;
};

export interface OpenClawAdapter {
  getStatus(): Promise<OpenClawStatus>;
  generateResponse(request: OpenClawChatRequest): Promise<string>;
}

const splitIntoChunks = async function* (content: string) {
  const normalized = content.trim();
  const chunks = normalized.match(/.{1,24}/g) ?? [normalized];
  for (const chunk of chunks) {
    yield chunk;
  }
};

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

class MockOpenClawAdapter implements OpenClawAdapter {
  async getStatus(): Promise<OpenClawStatus> {
    return {
      provider: 'openclaw',
      connected: false,
      capabilities: DEFAULT_CAPABILITIES
    };
  }

  async generateResponse(request: OpenClawChatRequest): Promise<string> {
    const lastTurns = request.history.slice(-3).map((item) => `${item.role}: ${item.content}`).join(' | ');
    return `OpenClaw 目前还没有接入真实网关。我已经记录你的请求：${request.message}。最近上下文：${lastTurns || '无'}。`;
  }
}

class HttpOpenClawAdapter implements OpenClawAdapter {
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

  async generateResponse(request: OpenClawChatRequest): Promise<string> {
    const response = await fetch(`${this.baseUrl}/chat`, {
      method: 'POST',
      headers: this.buildHeaders(),
      body: JSON.stringify({
        sessionId: request.sessionId,
        userId: request.userId,
        traceId: request.traceId,
        model: this.model,
        input: request.message,
        history: request.history
      })
    });

    if (!response.ok) {
      throw new Error(`chat request failed: ${response.status}`);
    }

    const payload = (await response.json()) as { content?: string };
    if (!payload.content || !payload.content.trim()) {
      throw new Error('openclaw response content is empty');
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

export const createOpenClawAdapter = () => {
  const provider = process.env.OPENCLAW_PROVIDER ?? 'mock';
  const baseUrl = process.env.OPENCLAW_BASE_URL?.replace(/\/$/, '');

  if (provider === 'gateway' && baseUrl) {
    return new HttpOpenClawAdapter(baseUrl, process.env.OPENCLAW_API_KEY, process.env.OPENCLAW_MODEL);
  }

  return new MockOpenClawAdapter();
};

export const streamOpenClawResponse = async function* (adapter: OpenClawAdapter, request: OpenClawChatRequest) {
  const content = await adapter.generateResponse(request);
  yield* splitIntoChunks(content);
};