import { invoke } from '@tauri-apps/api/core';
import { isTauriRuntime } from './tauriDesktop';

export type InstallSkillResult = {
	targetDir: string;
	installedFiles: string[];
	unpacked: boolean;
};

export const installSkillArchive = async (bytes: Uint8Array, targetDir?: string) => {
	if (!isTauriRuntime()) {
		throw new Error('只有桌面端才能一键安装 Skill');
	}

	return invoke<InstallSkillResult>('install_skill_package', {
		bytes: Array.from(bytes),
		targetDir
	});
};