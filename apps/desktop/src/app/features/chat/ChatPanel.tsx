import type { ChatMessage } from '@desktop-companion/shared';

type ChatPanelProps = {
  messages: ChatMessage[];
  draft: string;
  isSending: boolean;
  onDraftChange: (value: string) => void;
  onSend: () => void;
};

export function ChatPanel(props: ChatPanelProps) {
  const { messages, draft, isSending, onDraftChange, onSend } = props;

  return (
    <section className="chat-card">
      <div className="chat-header">
        <div>
          <p className="eyebrow">Companion Chat</p>
          <h2>陪伴对话</h2>
        </div>
        <span className="chat-badge">{isSending ? '响应中' : '待命'}</span>
      </div>

      <div className="message-list">
        {messages.length === 0 ? (
          <p className="empty-state">输入一句话，桌面伴侣会开始回应。</p>
        ) : (
          messages.map((message) => (
            <article key={message.id} className={`message message-${message.role}`}>
              <span className="message-role">{message.role === 'user' ? '你' : '伴侣'}</span>
              <p>{message.content || '...'}</p>
            </article>
          ))
        )}
      </div>

      <div className="composer">
        <textarea
          value={draft}
          onChange={(event) => onDraftChange(event.target.value)}
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
