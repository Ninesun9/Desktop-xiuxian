import type { DatabaseSync } from 'node:sqlite';

type Migration = {
	version: number;
	name: string;
	up: (db: DatabaseSync) => void;
};

type SchemaMigrationRow = {
	version: number;
};

const tableExists = (db: DatabaseSync, tableName: string) =>
	Boolean(
		db
			.prepare("SELECT 1 AS ok FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1")
			.get(tableName)
	);

const columnExists = (db: DatabaseSync, tableName: string, columnName: string) => {
	if (!tableExists(db, tableName)) {
		return false;
	}

	const rows = db.prepare(`PRAGMA table_info(${tableName})`).all() as Array<Record<string, unknown>>;
	return rows.some((row) => String(row.name) === columnName);
};

const ensureColumn = (db: DatabaseSync, tableName: string, columnName: string, ddl: string) => {
	if (!columnExists(db, tableName, columnName)) {
		db.exec(`ALTER TABLE ${tableName} ADD COLUMN ${ddl}`);
	}
};

const migrations: Migration[] = [
	{
		version: 1,
		name: 'initial-social-schema',
		up: (db) => {
			db.exec(`
				PRAGMA foreign_keys = ON;

				CREATE TABLE IF NOT EXISTS user_profiles (
					id TEXT PRIMARY KEY,
					name TEXT NOT NULL,
					last_seen_at TEXT NOT NULL
				);

				CREATE TABLE IF NOT EXISTS contact_requests (
					id TEXT PRIMARY KEY,
					requester_user_id TEXT NOT NULL,
					target_user_id TEXT NOT NULL,
					remark TEXT,
					created_at TEXT NOT NULL,
					status TEXT NOT NULL,
					FOREIGN KEY (requester_user_id) REFERENCES user_profiles(id),
					FOREIGN KEY (target_user_id) REFERENCES user_profiles(id)
				);

				CREATE TABLE IF NOT EXISTS contacts (
					owner_user_id TEXT NOT NULL,
					contact_user_id TEXT NOT NULL,
					remark TEXT,
					created_at TEXT NOT NULL,
					PRIMARY KEY (owner_user_id, contact_user_id),
					FOREIGN KEY (owner_user_id) REFERENCES user_profiles(id),
					FOREIGN KEY (contact_user_id) REFERENCES user_profiles(id)
				);

				CREATE TABLE IF NOT EXISTS direct_conversations (
					id TEXT PRIMARY KEY,
					user_low TEXT NOT NULL,
					user_high TEXT NOT NULL,
					created_at TEXT NOT NULL,
					updated_at TEXT NOT NULL,
					UNIQUE (user_low, user_high),
					FOREIGN KEY (user_low) REFERENCES user_profiles(id),
					FOREIGN KEY (user_high) REFERENCES user_profiles(id)
				);

				CREATE TABLE IF NOT EXISTS direct_messages (
					id TEXT PRIMARY KEY,
					conversation_id TEXT NOT NULL,
					sender_user_id TEXT NOT NULL,
					receiver_user_id TEXT NOT NULL,
					kind TEXT NOT NULL,
					content TEXT NOT NULL,
					assistant_meta_json TEXT,
					created_at TEXT NOT NULL,
					read_at TEXT,
					FOREIGN KEY (conversation_id) REFERENCES direct_conversations(id),
					FOREIGN KEY (sender_user_id) REFERENCES user_profiles(id),
					FOREIGN KEY (receiver_user_id) REFERENCES user_profiles(id)
				);

				CREATE TABLE IF NOT EXISTS direct_message_attachments (
					id TEXT PRIMARY KEY,
					message_id TEXT NOT NULL,
					name TEXT NOT NULL,
					stored_path TEXT NOT NULL,
					size_bytes INTEGER NOT NULL,
					content_type TEXT NOT NULL,
					transfer_kind TEXT NOT NULL,
					install_hint_json TEXT,
					created_at TEXT NOT NULL,
					FOREIGN KEY (message_id) REFERENCES direct_messages(id)
				);

				CREATE INDEX IF NOT EXISTS idx_contact_requests_target ON contact_requests(target_user_id, status, created_at);
				CREATE INDEX IF NOT EXISTS idx_contacts_owner ON contacts(owner_user_id);
				CREATE INDEX IF NOT EXISTS idx_direct_messages_conversation ON direct_messages(conversation_id, created_at);
				CREATE INDEX IF NOT EXISTS idx_direct_messages_receiver ON direct_messages(receiver_user_id, read_at, created_at);
			`);
		}
	},
	{
		version: 2,
		name: 'add-transport-public-keys',
		up: (db) => {
			ensureColumn(db, 'user_profiles', 'transport_public_key', 'transport_public_key TEXT');
			ensureColumn(db, 'user_profiles', 'transport_key_updated_at', 'transport_key_updated_at TEXT');
		}
	},
	{
		version: 3,
		name: 'add-delivery-and-encryption-tracking',
		up: (db) => {
			ensureColumn(db, 'direct_messages', 'requires_ack', 'requires_ack INTEGER NOT NULL DEFAULT 0');
			ensureColumn(db, 'direct_messages', 'delivered_at', 'delivered_at TEXT');
			ensureColumn(db, 'direct_messages', 'acked_at', 'acked_at TEXT');
			ensureColumn(db, 'direct_messages', 'ack_note', 'ack_note TEXT');

			ensureColumn(db, 'direct_message_attachments', 'sha256', "sha256 TEXT NOT NULL DEFAULT ''");
			ensureColumn(db, 'direct_message_attachments', 'encryption_json', 'encryption_json TEXT');
			ensureColumn(db, 'direct_message_attachments', 'install_status', "install_status TEXT NOT NULL DEFAULT 'pending'");
			ensureColumn(db, 'direct_message_attachments', 'installed_at', 'installed_at TEXT');
			ensureColumn(db, 'direct_message_attachments', 'install_note', 'install_note TEXT');

			db.exec(`
				CREATE INDEX IF NOT EXISTS idx_direct_messages_delivery ON direct_messages(receiver_user_id, delivered_at, acked_at, created_at);
				CREATE INDEX IF NOT EXISTS idx_direct_attachments_install ON direct_message_attachments(message_id, install_status, created_at);
			`);
		}
	}
];

export const runSocialMigrations = (db: DatabaseSync) => {
	db.exec(`
		CREATE TABLE IF NOT EXISTS schema_migrations (
			version INTEGER PRIMARY KEY,
			name TEXT NOT NULL,
			applied_at TEXT NOT NULL
		)
	`);

	const appliedVersions = new Set(
		(db.prepare('SELECT version FROM schema_migrations ORDER BY version ASC').all() as SchemaMigrationRow[]).map((row) => Number(row.version))
	);

	for (const migration of migrations) {
		if (appliedVersions.has(migration.version)) {
			continue;
		}

		migration.up(db);
		db.prepare('INSERT INTO schema_migrations (version, name, applied_at) VALUES (?, ?, ?)').run(
			migration.version,
			migration.name,
			new Date().toISOString()
		);
	}
};