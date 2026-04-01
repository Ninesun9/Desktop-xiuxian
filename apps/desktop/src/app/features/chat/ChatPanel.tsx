import type { ChatMessage } from '@desktop-companion/shared';

type ChatPanelProps = {
  messages: ChatMessage[];
  draft: string;
  isSending: boolean;
  sessionTitle: string;
  companionReady: boolean;
  quickPrompts: string[];
  onDraftChange: (value: string) => void;
  onSend: () => void;
  onQuickPrompt: (value: string) => void;
};

const formatTime = (value: string) => {
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) {
    return '--:--';
  }
  return date.toLocaleTimeString('zh-CN', {
    hour: '2-digit',
    minute: '2-digit'
  });
};

export function ChatPanel(props: ChatPanelProps) {
  const { messages, draft, isSending, sessionTitle, companionReady, quickPrompts, onDraftChange, onSend, onQuickPrompt } = props;

  return (
    <section className="chat-card">
      <div className="chat-header">
        <div>
          <p className="eyebrow">Companion Chat</p>
          <h2>{sessionTitle}</h2>
        </div>
        <span className="chat-badge">{isSending ? '响应中' : companionReady ? '在线' : '待命'}</span>
      </div>

      <div className="quick-prompts">
        {quickPrompts.map((prompt) => (
          <button key={prompt} type="button" className="quick-prompt" onClick={() => onQuickPrompt(prompt)}>
            {prompt}
          </button>
        ))}
      </div>

      <div className="message-list">
        {messages.length === 0 ? (
          <p className="empty-state">输入一句话，桌面伴侣会开始回应。</p>
        ) : (
          messages.map((message) => (
            <article key={message.id} className={`message message-${message.role}`}>
              <header className="message-meta">
                <span className="message-role">
                  {message.role === 'user' ? '你' : message.role === 'assistant' ? '伴侣' : '系统'}
                </span>
                <time>{formatTime(message.createdAt)}</time>
              </header>
              <p>{message.content || '...'}</p>
            </article>
          ))
        )}
      </div>

      <div className="composer">
        <textarea
          value={draft}
          onChange={(event) => onDraftChange(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === 'Enter' && !event.shiftKey) {
              event.preventDefault();
              onSend();
            }
          }}
          placeholder="例如：帮我规划今天的工作和休息时间"
          rows={3}
        />
        <button type="button" onClick={onSend} disabled={isSending || !draft.trim()}>
          发送
        </button>
      </div>
    </section>
  );
}
