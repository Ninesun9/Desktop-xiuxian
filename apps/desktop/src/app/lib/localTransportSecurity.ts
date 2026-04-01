import { x25519 } from '@noble/curves/ed25519.js';
import type {
	DirectMessageAttachment,
	DirectMessageAttachmentEncryption,
	TransportKeyResponse,
	UpdateTransportKeyRequest
} from '@desktop-companion/shared';

const TRANSPORT_INFO = new TextEncoder().encode('xiuxian-spirit-mail-attachment');

type TransportIdentity = {
	privateKey: string;
	publicKey: string;
};

const storageKey = (userId: string) => `desktop-companion-transport:${userId}`;

const toBase64 = (bytes: Uint8Array) => {
	let binary = '';
	for (const byte of bytes) {
		binary += String.fromCharCode(byte);
	}
	return btoa(binary);
};

const fromBase64 = (value: string) => {
	const binary = atob(value);
	const bytes = new Uint8Array(binary.length);
	for (let index = 0; index < binary.length; index += 1) {
		bytes[index] = binary.charCodeAt(index);
	}
	return bytes;
};

const getRandomBytes = (length: number) => crypto.getRandomValues(new Uint8Array(length));

const toBufferSource = (bytes: Uint8Array) =>
	bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) as ArrayBuffer;

const deriveAesKey = async (privateKey: Uint8Array, peerPublicKey: Uint8Array, salt: Uint8Array) => {
	const sharedSecret = x25519.getSharedSecret(privateKey, peerPublicKey);
	const keyMaterial = await crypto.subtle.importKey('raw', toBufferSource(sharedSecret), 'HKDF', false, ['deriveKey']);

	return crypto.subtle.deriveKey(
		{
			name: 'HKDF',
			hash: 'SHA-256',
			salt: toBufferSource(salt),
			info: TRANSPORT_INFO
		},
		keyMaterial,
		{
			name: 'AES-GCM',
			length: 256
		},
		false,
		['encrypt', 'decrypt']
	);
};

export const getOrCreateTransportIdentity = (userId: string): TransportIdentity => {
	const existing = window.localStorage.getItem(storageKey(userId));
	if (existing) {
		return JSON.parse(existing) as TransportIdentity;
	}

	const privateKeyBytes = getRandomBytes(32);
	const publicKeyBytes = x25519.getPublicKey(privateKeyBytes);
	const identity = {
		privateKey: toBase64(privateKeyBytes),
		publicKey: toBase64(publicKeyBytes)
	};
	window.localStorage.setItem(storageKey(userId), JSON.stringify(identity));
	return identity;
};

export const ensureTransportIdentityRegistered = async (
	apiBase: string,
	headers: Record<string, string>,
	userId: string
) => {
	const identity = getOrCreateTransportIdentity(userId);
	const current = await fetch(`${apiBase}/api/v1/social/transport-key`, {
		headers
	}).then((response) => {
		if (!response.ok) {
			throw new Error(`transport key read failed: ${response.status}`);
		}
		return response.json() as Promise<TransportKeyResponse>;
	});

	if (current.publicKey === identity.publicKey) {
		return identity;
	}

	await fetch(`${apiBase}/api/v1/social/transport-key`, {
		method: 'POST',
		headers,
		body: JSON.stringify({ publicKey: identity.publicKey } satisfies UpdateTransportKeyRequest)
	}).then((response) => {
		if (!response.ok) {
			throw new Error(`transport key update failed: ${response.status}`);
		}
	});

	return identity;
};

const digestSha256Hex = async (bytes: Uint8Array) => {
	const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', toBufferSource(bytes)));
	return Array.from(digest, (value) => value.toString(16).padStart(2, '0')).join('');
};

export const encryptAttachmentForPeer = async (userId: string, peerPublicKey: string, bytes: Uint8Array) => {
	const identity = getOrCreateTransportIdentity(userId);
	const salt = getRandomBytes(16);
	const nonce = getRandomBytes(12);
	const key = await deriveAesKey(fromBase64(identity.privateKey), fromBase64(peerPublicKey), salt);
	const cipherBytes = new Uint8Array(
		await crypto.subtle.encrypt({ name: 'AES-GCM', iv: toBufferSource(nonce) }, key, toBufferSource(bytes))
	);

	return {
		bytes: cipherBytes,
		sha256: await digestSha256Hex(bytes),
		encryption: {
			algorithm: 'X25519-HKDF-AESGCM',
			salt: toBase64(salt),
			nonce: toBase64(nonce),
			senderPublicKey: identity.publicKey
		} satisfies DirectMessageAttachmentEncryption
	};
};

export const decryptAttachmentForCurrentUser = async (
	userId: string,
	attachment: Pick<DirectMessageAttachment, 'sha256' | 'encryption'>,
	bytes: Uint8Array
) => {
	if (!attachment.encryption) {
		return bytes;
	}

	const identity = getOrCreateTransportIdentity(userId);
	const key = await deriveAesKey(
		fromBase64(identity.privateKey),
		fromBase64(attachment.encryption.senderPublicKey),
		fromBase64(attachment.encryption.salt)
	);
	const plainBytes = new Uint8Array(
		await crypto.subtle.decrypt(
			{ name: 'AES-GCM', iv: toBufferSource(fromBase64(attachment.encryption.nonce)) },
			key,
			toBufferSource(bytes)
		)
	);

	if (attachment.sha256) {
		const actualSha256 = await digestSha256Hex(plainBytes);
		if (actualSha256 !== attachment.sha256) {
			throw new Error('附件校验失败，内容可能已损坏');
		}
	}

	return plainBytes;
};