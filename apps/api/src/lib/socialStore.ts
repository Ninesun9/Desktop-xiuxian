import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { DatabaseSync } from 'node:sqlite';
import type {
	ContactListResponse,
	ContactRequest,
	ContactRequestListResponse,
	CreateContactRequest,
	DirectConversation,
	DirectMessage,
	DirectMessageAttachment,
	DirectMessageAttachmentEncryption,
	DirectMessageKind,
	DirectMessageListResponse,
	TransportKeyResponse
} from '@desktop-companion/shared';
import { runSocialMigrations } from './socialMigrations.js';

const RUNTIME_DIR = path.resolve(process.cwd(), '.runtime', 'social');
const DB_PATH = path.join(RUNTIME_DIR, 'social.db');
const ATTACHMENT_DIR = path.join(RUNTIME_DIR, 'attachments');

type UserRow = {
	id: string;
	name: string;
	last_seen_at: string;
	transport_public_key: string | null;
	transport_key_updated_at: string | null;
};

type ConversationRow = {
	id: string;
	user_low: string;
	user_high: string;
	created_at: string;
	updated_at: string;
};

type MessageRow = {
	id: string;
	conversation_id: string;
	sender_user_id: string;
	receiver_user_id: string;
	kind: DirectMessageKind;
	content: string;
	assistant_meta_json: string | null;
	created_at: string;
	read_at: string | null;
	requires_ack: number;
	delivered_at: string | null;
	acked_at: string | null;
	ack_note: string | null;
};

type AttachmentRow = {
	id: string;
	message_id: string;
	name: string;
	stored_path: string;
	size_bytes: number;
	sha256: string;
	content_type: string;
	transfer_kind: 'file' | 'skill';
	encryption_json: string | null;
	install_hint_json: string | null;
	install_status: 'pending' | 'installed' | 'failed';
	installed_at: string | null;
	install_note: string | null;
	created_at: string;
};

type DbRow = Record<string, unknown>;

const nowIso = () => new Date().toISOString();

const ensureRuntime = () => {
	fs.mkdirSync(RUNTIME_DIR, { recursive: true });
	fs.mkdirSync(ATTACHMENT_DIR, { recursive: true });
};

const orderedPair = (leftUserId: string, rightUserId: string) =>
	[leftUserId, rightUserId].sort() as [string, string];

const asString = (value: unknown) => String(value ?? '');
const asNullableString = (value: unknown) => (value == null ? null : String(value));
const asNumber = (value: unknown) => Number(value ?? 0);

const toUserRow = (row: DbRow): UserRow => ({
	id: asString(row.id),
	name: asString(row.name),
	last_seen_at: asString(row.last_seen_at),
	transport_public_key: asNullableString(row.transport_public_key),
	transport_key_updated_at: asNullableString(row.transport_key_updated_at)
});

const toConversationRow = (row: DbRow): ConversationRow => ({
	id: asString(row.id),
	user_low: asString(row.user_low),
	user_high: asString(row.user_high),
	created_at: asString(row.created_at),
	updated_at: asString(row.updated_at)
});

const toMessageRow = (row: DbRow): MessageRow => ({
	id: asString(row.id),
	conversation_id: asString(row.conversation_id),
	sender_user_id: asString(row.sender_user_id),
	receiver_user_id: asString(row.receiver_user_id),
	kind: asString(row.kind) as DirectMessageKind,
	content: asString(row.content),
	assistant_meta_json: asNullableString(row.assistant_meta_json),
	created_at: asString(row.created_at),
	read_at: asNullableString(row.read_at),
	requires_ack: asNumber(row.requires_ack),
	delivered_at: asNullableString(row.delivered_at),
	acked_at: asNullableString(row.acked_at),
	ack_note: asNullableString(row.ack_note)
});

const toAttachmentRow = (row: DbRow): AttachmentRow => ({
	id: asString(row.id),
	message_id: asString(row.message_id),
	name: asString(row.name),
	stored_path: asString(row.stored_path),
	size_bytes: asNumber(row.size_bytes),
	sha256: asString(row.sha256),
	content_type: asString(row.content_type),
	transfer_kind: asString(row.transfer_kind) as 'file' | 'skill',
	encryption_json: asNullableString(row.encryption_json),
	install_hint_json: asNullableString(row.install_hint_json),
	install_status: asString(row.install_status) as AttachmentRow['install_status'],
	installed_at: asNullableString(row.installed_at),
	install_note: asNullableString(row.install_note),
	created_at: asString(row.created_at)
});

const toPresence = (lastSeenAt: string): 'online' | 'recently-active' =>
	Date.now() - Date.parse(lastSeenAt) <= 1000 * 60 * 5 ? 'online' : 'recently-active';

const computeSha256 = (bytes: Buffer) => crypto.createHash('sha256').update(bytes).digest('hex');

const resolveDeliveryStatus = (row: Pick<MessageRow, 'acked_at' | 'delivered_at'>) => {
	if (row.acked_at) {
		return 'acknowledged' as const;
	}
	if (row.delivered_at) {
		return 'delivered' as const;
	}
	return 'sent' as const;
};

class SocialStore {
	private readonly db: DatabaseSync;

	constructor() {
		ensureRuntime();
		this.db = new DatabaseSync(DB_PATH);
		runSocialMigrations(this.db);
		this.db.exec('PRAGMA foreign_keys = ON;');
	}

	upsertUserProfile(userId: string, name: string) {
		this.db.prepare(`
			INSERT INTO user_profiles (id, name, last_seen_at)
			VALUES (?, ?, ?)
			ON CONFLICT(id) DO UPDATE SET
				name = excluded.name,
				last_seen_at = excluded.last_seen_at
		`).run(userId, name, nowIso());
	}

	touchUserProfile(userId: string) {
		this.db.prepare('UPDATE user_profiles SET last_seen_at = ? WHERE id = ?').run(nowIso(), userId);
	}

	updateTransportPublicKey(userId: string, publicKey: string): TransportKeyResponse {
		const normalized = publicKey.trim();
		if (!normalized) {
			throw new Error('传输公钥不能为空');
		}

		const updatedAt = nowIso();
		this.db.prepare('UPDATE user_profiles SET transport_public_key = ?, transport_key_updated_at = ? WHERE id = ?').run(
			normalized,
			updatedAt,
			userId
		);

		return {
			userId,
			publicKey: normalized,
			updatedAt,
			ready: true
		};
	}

	getOwnTransportKey(userId: string): TransportKeyResponse {
		const user = this.requireUser(userId);
		return {
			userId,
			publicKey: user.transport_public_key ?? undefined,
			updatedAt: user.transport_key_updated_at ?? undefined,
			ready: Boolean(user.transport_public_key)
		};
	}

	resolveUserTarget(target: string, excludeUserId?: string) {
		const normalized = target.trim();
		if (!normalized) {
			return null;
		}

		const exactIdRow = this.db.prepare('SELECT * FROM user_profiles WHERE id = ?').get(normalized) as DbRow | undefined;
		const exactId = exactIdRow ? toUserRow(exactIdRow) : undefined;
		if (exactId && exactId.id !== excludeUserId) {
			return exactId;
		}

		const exactNameRows = (this.db.prepare('SELECT * FROM user_profiles WHERE name = ? ORDER BY last_seen_at DESC').all(normalized) as DbRow[])
			.map(toUserRow)
			.filter((row) => row.id !== excludeUserId);

		if (exactNameRows.length === 1) {
			return exactNameRows[0];
		}

		if (exactNameRows.length > 1) {
			throw new Error(`名字“${normalized}”对应多个玩家，请直接填写玩家 ID`);
		}

		return null;
	}

	listContacts(userId: string): ContactListResponse {
		const rows = this.db.prepare(`
			SELECT c.contact_user_id, c.remark, u.name, u.last_seen_at, u.transport_public_key
			FROM contacts c
			JOIN user_profiles u ON u.id = c.contact_user_id
			WHERE c.owner_user_id = ?
			ORDER BY u.last_seen_at DESC
		`).all(userId) as DbRow[];

		return {
			items: rows.map((row) => ({
				contactUserId: asString(row.contact_user_id),
				displayName: asString(row.name),
				remark: asNullableString(row.remark) ?? undefined,
				lastSeenAt: asString(row.last_seen_at),
				presence: toPresence(asString(row.last_seen_at)),
				transportPublicKey: asNullableString(row.transport_public_key) ?? undefined,
				transportReady: Boolean(asNullableString(row.transport_public_key))
			}))
		};
	}

	listPendingContactRequests(userId: string): ContactRequestListResponse {
		const rows = this.db.prepare(`
			SELECT r.id, r.requester_user_id, req.name AS requester_name, r.target_user_id, tar.name AS target_name, r.created_at, r.status
			FROM contact_requests r
			JOIN user_profiles req ON req.id = r.requester_user_id
			JOIN user_profiles tar ON tar.id = r.target_user_id
			WHERE r.status = 'pending' AND (r.requester_user_id = ? OR r.target_user_id = ?)
			ORDER BY r.created_at DESC
		`).all(userId, userId) as DbRow[];

		return {
			items: rows.map((row) => ({
				id: asString(row.id),
				requesterUserId: asString(row.requester_user_id),
				requesterName: asString(row.requester_name),
				targetUserId: asString(row.target_user_id),
				targetName: asString(row.target_name),
				createdAt: asString(row.created_at),
				status: asString(row.status) as ContactRequest['status']
			}))
		};
	}

	createContactRequest(requesterUserId: string, payload: CreateContactRequest): ContactRequest {
		const target = this.resolveUserTarget(payload.target, requesterUserId);
		if (!target) {
			throw new Error('目标玩家不存在');
		}

		if (this.areContacts(requesterUserId, target.id)) {
			throw new Error('你们已经互为联系人');
		}

		const existing = this.db.prepare(`
			SELECT id FROM contact_requests
			WHERE status = 'pending'
				AND ((requester_user_id = ? AND target_user_id = ?) OR (requester_user_id = ? AND target_user_id = ?))
			LIMIT 1
		`).get(requesterUserId, target.id, target.id, requesterUserId) as DbRow | undefined;

		if (existing) {
			throw new Error('已有待处理的联系人申请');
		}

		const requester = this.requireUser(requesterUserId);
		const requestId = crypto.randomUUID();
		const createdAt = nowIso();
		this.db.prepare(`
			INSERT INTO contact_requests (id, requester_user_id, target_user_id, remark, created_at, status)
			VALUES (?, ?, ?, ?, ?, 'pending')
		`).run(requestId, requesterUserId, target.id, payload.remark ?? null, createdAt);

		return {
			id: requestId,
			requesterUserId,
			requesterName: requester.name,
			targetUserId: target.id,
			targetName: target.name,
			createdAt,
			status: 'pending'
		};
	}

	respondContactRequest(requestId: string, currentUserId: string, action: 'accept' | 'reject') {
		const row = this.db.prepare(`
			SELECT id, requester_user_id, target_user_id, created_at, status
			FROM contact_requests
			WHERE id = ?
		`).get(requestId) as DbRow | undefined;

		if (!row || asString(row.target_user_id) !== currentUserId || asString(row.status) !== 'pending') {
			return null;
		}

		const nextStatus = action === 'accept' ? 'accepted' : 'rejected';
		this.db.prepare('UPDATE contact_requests SET status = ? WHERE id = ?').run(nextStatus, requestId);

		if (action === 'accept') {
			const createdAt = nowIso();
			this.db.prepare(`
				INSERT INTO contacts (owner_user_id, contact_user_id, remark, created_at)
				VALUES (?, ?, ?, ?)
				ON CONFLICT(owner_user_id, contact_user_id) DO UPDATE SET remark = excluded.remark
			`).run(asString(row.requester_user_id), asString(row.target_user_id), null, createdAt);
			this.db.prepare(`
				INSERT INTO contacts (owner_user_id, contact_user_id, remark, created_at)
				VALUES (?, ?, ?, ?)
				ON CONFLICT(owner_user_id, contact_user_id) DO NOTHING
			`).run(asString(row.target_user_id), asString(row.requester_user_id), null, createdAt);
		}

		const requester = this.requireUser(asString(row.requester_user_id));
		const target = this.requireUser(asString(row.target_user_id));
		return {
			id: asString(row.id),
			requesterUserId: asString(row.requester_user_id),
			requesterName: requester.name,
			targetUserId: asString(row.target_user_id),
			targetName: target.name,
			createdAt: asString(row.created_at),
			status: nextStatus
		} satisfies ContactRequest;
	}

	areContacts(leftUserId: string, rightUserId: string) {
		const row = this.db.prepare('SELECT 1 AS ok FROM contacts WHERE owner_user_id = ? AND contact_user_id = ?').get(leftUserId, rightUserId) as { ok: number } | undefined;
		const reverse = this.db.prepare('SELECT 1 AS ok FROM contacts WHERE owner_user_id = ? AND contact_user_id = ?').get(rightUserId, leftUserId) as { ok: number } | undefined;
		return Boolean(row && reverse);
	}

	getOrCreateConversation(currentUserId: string, participantUserId: string): DirectConversation {
		const [userLow, userHigh] = orderedPair(currentUserId, participantUserId);
		let conversationRow = this.db.prepare(`
			SELECT id, user_low, user_high, created_at, updated_at
			FROM direct_conversations
			WHERE user_low = ? AND user_high = ?
		`).get(userLow, userHigh) as DbRow | undefined;
		let conversation = conversationRow ? toConversationRow(conversationRow) : undefined;

		if (!conversation) {
			const conversationId = crypto.randomUUID();
			const createdAt = nowIso();
			this.db.prepare(`
				INSERT INTO direct_conversations (id, user_low, user_high, created_at, updated_at)
				VALUES (?, ?, ?, ?, ?)
			`).run(conversationId, userLow, userHigh, createdAt, createdAt);
			conversation = {
				id: conversationId,
				user_low: userLow,
				user_high: userHigh,
				created_at: createdAt,
				updated_at: createdAt
			};
		}

		return this.toConversationSummary(conversation, currentUserId);
	}

	listConversations(currentUserId: string): { items: DirectConversation[] } {
		const rows = (this.db.prepare(`
			SELECT id, user_low, user_high, created_at, updated_at
			FROM direct_conversations
			WHERE user_low = ? OR user_high = ?
			ORDER BY updated_at DESC
		`).all(currentUserId, currentUserId) as DbRow[]).map(toConversationRow);

		return { items: rows.map((row) => this.toConversationSummary(row, currentUserId)) };
	}

	getConversationForUser(conversationId: string, currentUserId: string) {
		const row = this.db.prepare(`
			SELECT id, user_low, user_high, created_at, updated_at
			FROM direct_conversations
			WHERE id = ?
		`).get(conversationId) as DbRow | undefined;
		const conversation = row ? toConversationRow(row) : undefined;
		if (!conversation || (conversation.user_low !== currentUserId && conversation.user_high !== currentUserId)) {
			return null;
		}
		return conversation;
	}

	listMessages(conversationId: string, currentUserId?: string): DirectMessageListResponse {
		if (currentUserId) {
			this.markConversationDelivered(conversationId, currentUserId);
		}

		const rows = (this.db.prepare(`
			SELECT *
			FROM direct_messages
			WHERE conversation_id = ?
			ORDER BY created_at ASC
		`).all(conversationId) as DbRow[]).map(toMessageRow);

		return { items: rows.map((row) => this.toDirectMessage(row)) };
	}

	markConversationRead(conversationId: string, currentUserId: string) {
		this.db.prepare(`
			UPDATE direct_messages
			SET read_at = COALESCE(read_at, ?)
			WHERE conversation_id = ? AND receiver_user_id = ?
		`).run(nowIso(), conversationId, currentUserId);
	}

	markConversationDelivered(conversationId: string, currentUserId: string) {
		this.db.prepare(`
			UPDATE direct_messages
			SET delivered_at = COALESCE(delivered_at, ?)
			WHERE conversation_id = ? AND receiver_user_id = ?
		`).run(nowIso(), conversationId, currentUserId);
	}

	createTextMessage(
		conversationId: string,
		senderUserId: string,
		content: string,
		assistantMeta?: DirectMessage['assistantMeta'],
		requiresAck = false
	) {
		const conversation = this.requireConversation(conversationId, senderUserId);
		const receiverUserId = conversation.user_low === senderUserId ? conversation.user_high : conversation.user_low;
		const messageId = crypto.randomUUID();
		const createdAt = nowIso();
		this.db.prepare(`
			INSERT INTO direct_messages (
				id, conversation_id, sender_user_id, receiver_user_id, kind, content, assistant_meta_json, created_at, read_at, requires_ack, delivered_at, acked_at, ack_note
			)
			VALUES (?, ?, ?, ?, 'text', ?, ?, ?, NULL, ?, NULL, NULL, NULL)
		`).run(
			messageId,
			conversationId,
			senderUserId,
			receiverUserId,
			content,
			assistantMeta ? JSON.stringify(assistantMeta) : null,
			createdAt,
			requiresAck ? 1 : 0
		);
		this.touchConversation(conversationId, createdAt);
		const row = toMessageRow(this.db.prepare('SELECT * FROM direct_messages WHERE id = ?').get(messageId) as DbRow);
		return this.toDirectMessage(row);
	}

	createAttachmentMessage(params: {
		conversationId: string;
		senderUserId: string;
		kind: 'file' | 'skill';
		content: string;
		originalName: string;
		contentType: string;
		bytes: Buffer;
		sha256?: string;
		requiresAck?: boolean;
		encryption?: DirectMessageAttachmentEncryption;
		installHint?: DirectMessageAttachment['installHint'];
	}) {
		const conversation = this.requireConversation(params.conversationId, params.senderUserId);
		const receiverUserId = conversation.user_low === params.senderUserId ? conversation.user_high : conversation.user_low;
		const messageId = crypto.randomUUID();
		const createdAt = nowIso();
		const safeName = path.basename(params.originalName);
		const attachmentId = crypto.randomUUID();
		const messageDir = path.join(ATTACHMENT_DIR, messageId);
		fs.mkdirSync(messageDir, { recursive: true });
		const storedPath = path.join(messageDir, `${attachmentId}-${safeName}`);
		fs.writeFileSync(storedPath, params.bytes);

		this.db.prepare(`
			INSERT INTO direct_messages (
				id, conversation_id, sender_user_id, receiver_user_id, kind, content, assistant_meta_json, created_at, read_at, requires_ack, delivered_at, acked_at, ack_note
			)
			VALUES (?, ?, ?, ?, ?, ?, NULL, ?, NULL, ?, NULL, NULL, NULL)
		`).run(
			messageId,
			params.conversationId,
			params.senderUserId,
			receiverUserId,
			params.kind,
			params.content,
			createdAt,
			params.requiresAck === false ? 0 : 1
		);

		this.db.prepare(`
			INSERT INTO direct_message_attachments (
				id, message_id, name, stored_path, size_bytes, sha256, content_type, transfer_kind, encryption_json, install_hint_json, install_status, installed_at, install_note, created_at
			)
			VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'pending', NULL, NULL, ?)
		`).run(
			attachmentId,
			messageId,
			safeName,
			storedPath,
			params.bytes.byteLength,
			params.sha256 ?? computeSha256(params.bytes),
			params.contentType || 'application/octet-stream',
			params.kind,
			params.encryption ? JSON.stringify(params.encryption) : null,
			params.installHint ? JSON.stringify(params.installHint) : null,
			createdAt
		);

		this.touchConversation(params.conversationId, createdAt);
		const row = toMessageRow(this.db.prepare('SELECT * FROM direct_messages WHERE id = ?').get(messageId) as DbRow);
		return this.toDirectMessage(row);
	}

	acknowledgeMessage(
		messageId: string,
		currentUserId: string,
		payload?: {
			note?: string;
			installState?: {
				status: 'pending' | 'installed' | 'failed';
				targetDir?: string;
				note?: string;
			};
		}
	) {
		const row = this.db.prepare('SELECT * FROM direct_messages WHERE id = ?').get(messageId) as DbRow | undefined;
		if (!row) {
			return null;
		}

		const message = toMessageRow(row);
		if (message.receiver_user_id !== currentUserId) {
			return null;
		}

		const ackedAt = nowIso();
		this.db.prepare(`
			UPDATE direct_messages
			SET delivered_at = COALESCE(delivered_at, ?), acked_at = ?, ack_note = ?
			WHERE id = ?
		`).run(ackedAt, ackedAt, payload?.note ?? null, messageId);

		if (payload?.installState) {
			const note = payload.installState.targetDir
				? `${payload.installState.note ?? payload.installState.status} @ ${payload.installState.targetDir}`
				: payload.installState.note ?? payload.installState.status;
			this.db.prepare(`
				UPDATE direct_message_attachments
				SET install_status = ?, installed_at = ?, install_note = ?
				WHERE message_id = ?
			`).run(payload.installState.status, ackedAt, note, messageId);
		}

		const next = toMessageRow(this.db.prepare('SELECT * FROM direct_messages WHERE id = ?').get(messageId) as DbRow);
		return this.toDirectMessage(next);
	}

	getAttachmentForUser(conversationId: string, messageId: string, attachmentId: string, currentUserId: string) {
		const conversation = this.getConversationForUser(conversationId, currentUserId);
		if (!conversation) {
			return null;
		}

		const row = this.db.prepare(`
			SELECT a.*
			FROM direct_message_attachments a
			JOIN direct_messages m ON m.id = a.message_id
			WHERE a.id = ? AND a.message_id = ? AND m.conversation_id = ?
		`).get(attachmentId, messageId, conversationId) as DbRow | undefined;

		return row ? toAttachmentRow(row) : null;
	}

	private requireUser(userId: string) {
		const row = this.db.prepare('SELECT * FROM user_profiles WHERE id = ?').get(userId) as DbRow | undefined;
		if (!row) {
			throw new Error('玩家不存在');
		}
		return toUserRow(row);
	}

	private requireConversation(conversationId: string, currentUserId: string) {
		const conversation = this.getConversationForUser(conversationId, currentUserId);
		if (!conversation) {
			throw new Error('私聊会话不存在');
		}
		return conversation;
	}

	private touchConversation(conversationId: string, updatedAt: string) {
		this.db.prepare('UPDATE direct_conversations SET updated_at = ? WHERE id = ?').run(updatedAt, conversationId);
	}

	private toConversationSummary(row: ConversationRow, currentUserId: string): DirectConversation {
		const participantUserId = row.user_low === currentUserId ? row.user_high : row.user_low;
		const participant = this.requireUser(participantUserId);
		const lastMessageRow = this.db.prepare(`
			SELECT *
			FROM direct_messages
			WHERE conversation_id = ?
			ORDER BY created_at DESC
			LIMIT 1
		`).get(row.id) as DbRow | undefined;
		const lastMessage = lastMessageRow ? toMessageRow(lastMessageRow) : undefined;
		const unread = this.db.prepare(`
			SELECT COUNT(*) AS total
			FROM direct_messages
			WHERE conversation_id = ? AND receiver_user_id = ? AND read_at IS NULL
		`).get(row.id, currentUserId) as DbRow | undefined;

		let preview = lastMessage?.content;
		if (lastMessage?.kind === 'file') {
			const attachment = this.listMessageAttachments(lastMessage.id)[0];
			preview = `[文件] ${attachment?.name ?? '附件'}`;
		}
		if (lastMessage?.kind === 'skill') {
			const attachment = this.listMessageAttachments(lastMessage.id)[0];
			preview = `[Skill] ${attachment?.name ?? '技能包'}`;
		}

		return {
			id: row.id,
			participantUserId,
			participantName: participant.name,
			title: participant.name,
			lastMessagePreview: preview,
			unreadCount: asNumber(unread?.total),
			updatedAt: row.updated_at
		};
	}

	private listMessageAttachments(messageId: string): DirectMessageAttachment[] {
		const rows = (this.db.prepare(`
			SELECT *
			FROM direct_message_attachments
			WHERE message_id = ?
			ORDER BY created_at ASC
		`).all(messageId) as DbRow[]).map(toAttachmentRow);

		return rows.map((row) => {
			const installHint = row.install_hint_json ? JSON.parse(row.install_hint_json) : undefined;
			return {
				id: row.id,
				name: row.name,
				sizeBytes: row.size_bytes,
				sha256: row.sha256,
				contentType: row.content_type,
				transferKind: row.transfer_kind,
				downloadUrl: `/api/v1/social/messages/${messageId}/attachments/${row.id}`,
				encryption: row.encryption_json ? JSON.parse(row.encryption_json) : undefined,
				installHint,
				installState: {
					status: row.install_status,
					installedAt: row.installed_at ?? undefined,
					note: row.install_note ?? undefined,
					targetDir: installHint?.targetDir
				}
			};
		});
	}

	private toDirectMessage(row: MessageRow): DirectMessage {
		return {
			id: row.id,
			conversationId: row.conversation_id,
			senderUserId: row.sender_user_id,
			receiverUserId: row.receiver_user_id,
			kind: row.kind,
			content: row.content,
			createdAt: row.created_at,
			assistantMeta: row.assistant_meta_json ? JSON.parse(row.assistant_meta_json) : undefined,
			transfer: {
				requiresAck: Boolean(row.requires_ack),
				deliveredAt: row.delivered_at ?? undefined,
				ackedAt: row.acked_at ?? undefined,
				ackNote: row.ack_note ?? undefined,
				status: resolveDeliveryStatus(row)
			},
			attachments: row.kind === 'text' ? undefined : this.listMessageAttachments(row.id)
		};
	}
}

export const socialStore = new SocialStore();