import { useRef, useState } from 'react';
import type {
  ContactRequest,
  ContactSummary,
  DirectConversation,
  DirectMessage,
  DirectMessageAttachment
} from '@desktop-companion/shared';

type MailPanelProps = {
  currentUserId: string | null;
  skillInstallSupported: boolean;
  contacts: ContactSummary[];
  contactRequests: ContactRequest[];
  conversations: DirectConversation[];
  activeConversation: DirectConversation | null;
  messages: DirectMessage[];
  draft: string;
  isSending: boolean;
  isAssisting: boolean;
  assistantReady: boolean;
  onDraftChange: (value: string) => void;
  onCreateContactRequest: (target: string, remark?: string) => void;
  onAcceptContactRequest: (requestId: string) => void;
  onRejectContactRequest: (requestId: string) => void;
  onOpenConversation: (conversationId: string) => void;
  onStartConversation: (contactUserId: string) => void;
  onSend: () => void;
  onSendAttachment: (file: File, kind: 'file' | 'skill') => void;
  onDownloadAttachment: (messageId: string, attachment: DirectMessageAttachment) => void;
  onInstallSkillAttachment: (messageId: string, attachment: DirectMessageAttachment) => void;
  onPolishDraft: () => void;
  onSuggestReply: () => void;
  installingAttachmentId?: string | null;
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

const formatPresence = (presence: 'online' | 'recently-active') => (presence === 'online' ? '在线' : '最近活跃');

const formatSize = (value: number) => {
  if (value < 1024) {
    return `${value} B`;
  }
  if (value < 1024 * 1024) {
    return `${(value / 1024).toFixed(1)} KB`;
  }
  return `${(value / (1024 * 1024)).toFixed(1)} MB`;
};

const formatTransferStatus = (status?: DirectMessage['transfer']) => {
  if (!status) {
    return null;
  }

  if (status.status === 'acknowledged') {
    return status.ackNote ? `已确认 · ${status.ackNote}` : '已确认';
  }
  if (status.status === 'delivered') {
    return '已送达';
  }
  return status.requiresAck ? '等待确认' : '已发出';
};

export function MailPanel(props: MailPanelProps) {
  const {
    currentUserId,
    skillInstallSupported,
    contacts,
    contactRequests,
    conversations,
    activeConversation,
    messages,
    draft,
    isSending,
    isAssisting,
    assistantReady,
    onDraftChange,
    onCreateContactRequest,
    onAcceptContactRequest,
    onRejectContactRequest,
    onOpenConversation,
    onStartConversation,
    onSend,
    onSendAttachment,
    onDownloadAttachment,
    onInstallSkillAttachment,
    onPolishDraft,
    onSuggestReply,
    installingAttachmentId
  } = props;

  const [contactTarget, setContactTarget] = useState('');
  const [contactRemark, setContactRemark] = useState('');
  const fileInputRef = useRef<HTMLInputElement | null>(null);
  const skillInputRef = useRef<HTMLInputElement | null>(null);
  const incomingRequests = contactRequests.filter((item) => item.targetUserId === currentUserId);
  const outgoingRequests = contactRequests.filter((item) => item.requesterUserId === currentUserId);

  return (
    <section className="mail-card">
      <div className="chat-header">
        <div>
          <p className="eyebrow">Spirit Mail</p>
          <h2>飞鸽传书</h2>
        </div>
        <span className="chat-badge">{conversations.length > 0 ? `${conversations.length} 个会话` : '等待开聊'}</span>
      </div>

      <div className="mail-layout">
        <div className="mail-sidebar">
          <section className="mail-section">
            <div className="mail-section-header">
              <strong>手动加好友</strong>
              <span>输入 ID / 名字</span>
            </div>
            <div className="mail-add-contact">
              <input
                value={contactTarget}
                onChange={(event) => setContactTarget(event.target.value)}
                placeholder="输入玩家 ID 或精确名字"
              />
              <input
                value={contactRemark}
                onChange={(event) => setContactRemark(event.target.value)}
                placeholder="备注名，可选"
              />
              <button
                type="button"
                onClick={() => {
                  onCreateContactRequest(contactTarget.trim(), contactRemark.trim() || undefined);
                  setContactTarget('');
                  setContactRemark('');
                }}
                disabled={!contactTarget.trim()}
              >
                发送申请
              </button>
            </div>
          </section>

          <section className="mail-section">
            <div className="mail-section-header">
              <strong>待处理申请</strong>
              <span>{incomingRequests.length}</span>
            </div>
            {incomingRequests.length === 0 ? (
              <p className="empty-state">暂时没有新的通讯录申请。</p>
            ) : (
              <div className="mail-list">
                {incomingRequests.map((item) => (
                  <article key={item.id} className="mail-list-item">
                    <div>
                      <strong>{item.requesterName}</strong>
                      <p>{item.requesterUserId}</p>
                    </div>
                    <div className="mail-inline-actions">
                      <button type="button" onClick={() => onAcceptContactRequest(item.id)}>
                        接受
                      </button>
                      <button type="button" onClick={() => onRejectContactRequest(item.id)}>
                        拒绝
                      </button>
                    </div>
                  </article>
                ))}
              </div>
            )}

            {outgoingRequests.length > 0 ? (
              <div className="mail-pending-block">
                <strong>我发出的申请</strong>
                <div className="mail-list compact">
                  {outgoingRequests.map((item) => (
                    <article key={item.id} className="mail-list-item muted">
                      <div>
                        <strong>{item.targetName}</strong>
                        <p>等待对方接受</p>
                      </div>
                    </article>
                  ))}
                </div>
              </div>
            ) : null}
          </section>

          <section className="mail-section">
            <div className="mail-section-header">
              <strong>通讯录</strong>
              <span>{contacts.length}</span>
            </div>
            {contacts.length === 0 ? (
              <p className="empty-state">先手动输入玩家 ID 或精确名字加几位道友。</p>
            ) : (
              <div className="mail-list">
                {contacts.map((contact) => (
                  <article key={contact.contactUserId} className="mail-list-item">
                    <div>
                      <strong>{contact.remark || contact.displayName}</strong>
                      <p>
                        {contact.displayName} · {formatPresence(contact.presence)}
                      </p>
                    </div>
                    <button type="button" onClick={() => onStartConversation(contact.contactUserId)}>
                      私聊
                    </button>
                  </article>
                ))}
              </div>
            )}
          </section>
        </div>

        <div className="mail-thread-card">
          <div className="mail-conversation-strip">
            {conversations.length === 0 ? (
              <p className="empty-state">互加通讯录后，就能在这里开始私聊。</p>
            ) : (
              conversations.map((conversation) => (
                <button
                  key={conversation.id}
                  type="button"
                  className={`mail-conversation-pill ${activeConversation?.id === conversation.id ? 'mail-conversation-pill-active' : ''}`}
                  onClick={() => onOpenConversation(conversation.id)}
                >
                  <span>{conversation.title}</span>
                  {conversation.unreadCount > 0 ? <em>{conversation.unreadCount}</em> : null}
                </button>
              ))
            )}
          </div>

          <div className="mail-thread-header">
            <div>
              <strong>{activeConversation?.title ?? '还没有选中私聊对象'}</strong>
              <p>{activeConversation?.lastMessagePreview ?? '先从通讯录里选择一位道友开始私聊。'}</p>
            </div>
            <span className="chat-badge">{assistantReady ? '灵宠可代笔' : '灵宠待命'}</span>
          </div>

          <div className="message-list mail-message-list">
            {!activeConversation ? (
              <p className="empty-state">选择一位联系人后，这里会显示你们的私聊记录。</p>
            ) : messages.length === 0 ? (
              <p className="empty-state">先发第一条消息试试。</p>
            ) : (
              messages.map((message) => (
                <article
                  key={message.id}
                  className={`message ${message.senderUserId === currentUserId ? 'message-user' : 'message-assistant'} mail-message`}
                >
                  <header className="message-meta">
                    <span className="message-role">{message.senderUserId === currentUserId ? '你' : activeConversation?.participantName ?? '对方'}</span>
                    <time>{formatTime(message.createdAt)}</time>
                  </header>
                  <p>{message.content}</p>
                  {message.assistantMeta ? <span className="mail-assistant-tag">灵宠辅助</span> : null}
                  {message.senderUserId === currentUserId && message.transfer ? (
                    <span className="mail-transfer-status">{formatTransferStatus(message.transfer)}</span>
                  ) : null}
                  {message.attachments?.length ? (
                    <div className="mail-attachment-list">
                      {message.attachments.map((attachment) => (
                        <div key={attachment.id} className="mail-attachment-actions">
                          <button
                            type="button"
                            className="mail-attachment-chip"
                            onClick={() => onDownloadAttachment(message.id, attachment)}
                          >
                            <span>{attachment.transferKind === 'skill' ? '[Skill]' : '[文件]'} {attachment.name}</span>
                            <em>{formatSize(attachment.sizeBytes)}</em>
                          </button>
                          {attachment.transferKind === 'skill' && message.receiverUserId === currentUserId ? (
                            <button
                              type="button"
                              className="mail-install-button"
                              onClick={() => onInstallSkillAttachment(message.id, attachment)}
                              disabled={!skillInstallSupported || installingAttachmentId === attachment.id}
                            >
                              {installingAttachmentId === attachment.id ? '安装中' : '安装到本地 OpenClaw'}
                            </button>
                          ) : null}
                          <span className="mail-attachment-meta">
                            {attachment.encryption ? '已加密' : '未加密'}
                            {attachment.installState?.status === 'installed' ? ` · 已安装` : ''}
                            {attachment.installState?.status === 'failed' ? ` · 安装失败` : ''}
                          </span>
                        </div>
                      ))}
                    </div>
                  ) : null}
                </article>
              ))
            )}
          </div>

          <div className="composer mail-composer">
            <div className="mail-transfer-actions">
              <input
                ref={fileInputRef}
                type="file"
                hidden
                onChange={(event) => {
                  const file = event.target.files?.[0];
                  if (file) {
                    onSendAttachment(file, 'file');
                  }
                  event.currentTarget.value = '';
                }}
              />
              <input
                ref={skillInputRef}
                type="file"
                hidden
                accept=".zip,application/zip"
                onChange={(event) => {
                  const file = event.target.files?.[0];
                  if (file) {
                    onSendAttachment(file, 'skill');
                  }
                  event.currentTarget.value = '';
                }}
              />
              <button type="button" onClick={() => fileInputRef.current?.click()} disabled={!activeConversation || isSending}>
                发送文件
              </button>
              <button type="button" onClick={() => skillInputRef.current?.click()} disabled={!activeConversation || isSending}>
                发送 Skill(zip)
              </button>
            </div>
            <div className="mail-helper-actions">
              <button type="button" onClick={onPolishDraft} disabled={!assistantReady || !draft.trim() || isAssisting}>
                {isAssisting ? '润色中' : '灵宠润色'}
              </button>
              <button type="button" onClick={onSuggestReply} disabled={!assistantReady || !activeConversation || isAssisting}>
                {isAssisting ? '思考中' : '灵宠代拟回复'}
              </button>
            </div>
            <textarea
              value={draft}
              onChange={(event) => onDraftChange(event.target.value)}
              onKeyDown={(event) => {
                if (event.key === 'Enter' && !event.shiftKey) {
                  event.preventDefault();
                  onSend();
                }
              }}
              placeholder={activeConversation ? '发一句悄悄话给这位道友' : '先从左侧选中一个私聊会话'}
              rows={4}
              disabled={!activeConversation}
            />
            <button type="button" onClick={onSend} disabled={!activeConversation || isSending || !draft.trim()}>
              发送私聊
            </button>
          </div>
        </div>
      </div>
    </section>
  );
}