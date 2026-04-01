export type PlayerPresence = 'online' | 'recently-active';

export type PlayerProfile = {
  userId: string;
  displayName: string;
  lastSeenAt: string;
  presence: PlayerPresence;
};

export type PlayerListResponse = {
  items: PlayerProfile[];
};

export type ContactSummary = {
  contactUserId: string;
  displayName: string;
  remark?: string;
  lastSeenAt: string;
  presence: PlayerPresence;
  transportPublicKey?: string;
  transportReady: boolean;
};

export type ContactListResponse = {
  items: ContactSummary[];
};

export type ContactRequestStatus = 'pending' | 'accepted' | 'rejected';

export type ContactRequest = {
  id: string;
  requesterUserId: string;
  requesterName: string;
  targetUserId: string;
  targetName: string;
  createdAt: string;
  status: ContactRequestStatus;
};

export type ContactRequestListResponse = {
  items: ContactRequest[];
};

export type CreateContactRequest = {
  target: string;
  remark?: string;
};

export type RespondContactRequest = {
  action: 'accept' | 'reject';
};

export type DirectMessageAssistantMeta = {
  source: 'openclaw';
  action: 'polish' | 'suggest-reply';
  model?: string;
};

export type TransportKeyResponse = {
  userId: string;
  publicKey?: string;
  updatedAt?: string;
  ready: boolean;
};

export type UpdateTransportKeyRequest = {
  publicKey: string;
};

export type DirectMessageDeliveryStatus = 'sent' | 'delivered' | 'acknowledged';

export type DirectMessageTransfer = {
  requiresAck: boolean;
  deliveredAt?: string;
  ackedAt?: string;
  ackNote?: string;
  status: DirectMessageDeliveryStatus;
};

export type DirectMessageAttachmentEncryption = {
  algorithm: 'X25519-HKDF-AESGCM';
  salt: string;
  nonce: string;
  senderPublicKey: string;
};

export type DirectMessageAttachmentInstallState = {
  status: 'pending' | 'installed' | 'failed';
  installedAt?: string;
  note?: string;
  targetDir?: string;
};

export type DirectMessageKind = 'text' | 'file' | 'skill';

export type DirectMessageAttachment = {
  id: string;
  name: string;
  sizeBytes: number;
  sha256: string;
  contentType: string;
  transferKind: Exclude<DirectMessageKind, 'text'>;
  downloadUrl: string;
  encryption?: DirectMessageAttachmentEncryption;
  installHint?: {
    unpackArchive?: boolean;
    targetDir?: string;
  };
  installState?: DirectMessageAttachmentInstallState;
};

export type DirectMessage = {
  id: string;
  conversationId: string;
  senderUserId: string;
  receiverUserId: string;
  kind: DirectMessageKind;
  content: string;
  createdAt: string;
  assistantMeta?: DirectMessageAssistantMeta;
  transfer?: DirectMessageTransfer;
  attachments?: DirectMessageAttachment[];
};

export type DirectMessageListResponse = {
  items: DirectMessage[];
};

export type DirectConversation = {
  id: string;
  participantUserId: string;
  participantName: string;
  title: string;
  lastMessagePreview?: string;
  unreadCount: number;
  updatedAt: string;
};

export type ConversationListResponse = {
  items: DirectConversation[];
};

export type CreateDirectConversationRequest = {
  participantUserId: string;
};

export type CreateDirectMessageRequest = {
  kind?: DirectMessageKind;
  content: string;
  assistantMeta?: DirectMessageAssistantMeta;
  requiresAck?: boolean;
};

export type AckDirectMessageRequest = {
  note?: string;
  installState?: DirectMessageAttachmentInstallState;
};

export type CreateDirectAttachmentMessageResponse = DirectMessage;
