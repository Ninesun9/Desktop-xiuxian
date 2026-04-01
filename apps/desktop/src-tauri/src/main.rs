#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::env;
use std::fs::{self, File};
use std::io::{Cursor, Write};
use std::path::PathBuf;
use tauri::{Manager, WebviewWindowBuilder, WebviewUrl};
use tauri_plugin_autostart::MacosLauncher;
use zip::ZipArchive;

#[derive(serde::Serialize)]
struct InstallSkillResult {
    target_dir: String,
    installed_files: Vec<String>,
    unpacked: bool,
}

fn default_skill_root() -> PathBuf {
    let home = env::var_os("USERPROFILE")
        .or_else(|| env::var_os("HOME"))
        .map(PathBuf::from)
        .unwrap_or_else(|| env::current_dir().unwrap_or_else(|_| PathBuf::from(".")));
    home.join(".openclaw").join("skills")
}

fn expand_target_dir(input: Option<String>, fallback_name: &str) -> PathBuf {
    let raw = input.unwrap_or_default();
    let trimmed = raw.trim();
    if trimmed.is_empty() {
        return default_skill_root().join(fallback_name);
    }

    if trimmed == "~" {
        return env::var_os("USERPROFILE")
            .or_else(|| env::var_os("HOME"))
            .map(PathBuf::from)
            .unwrap_or_else(default_skill_root);
    }

    if let Some(rest) = trimmed.strip_prefix("~/") {
        return env::var_os("USERPROFILE")
            .or_else(|| env::var_os("HOME"))
            .map(PathBuf::from)
            .unwrap_or_else(default_skill_root)
            .join(rest);
    }

    PathBuf::from(trimmed)
}

#[tauri::command]
fn force_native_drag(window: tauri::Window) {
    let _ = window.start_dragging();
}

#[tauri::command]
fn install_skill_package(bytes: Vec<u8>, target_dir: Option<String>) -> Result<InstallSkillResult, String> {
    let default_name = format!(
        "imported-skill-{}",
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map_err(|error| error.to_string())?
            .as_secs()
    );
    let target_dir = expand_target_dir(target_dir, &default_name);
    fs::create_dir_all(&target_dir).map_err(|error| error.to_string())?;

    let mut archive = ZipArchive::new(Cursor::new(bytes)).map_err(|error| error.to_string())?;
    let mut installed_files = Vec::new();

    for index in 0..archive.len() {
        let mut entry = archive.by_index(index).map_err(|error| error.to_string())?;
        let entry_path = entry
            .enclosed_name()
            .map(|path| path.to_owned())
            .ok_or_else(|| format!("zip 包中存在非法路径: {}", entry.name()))?;
        let output_path = target_dir.join(&entry_path);

        if entry.name().ends_with('/') {
            fs::create_dir_all(&output_path).map_err(|error| error.to_string())?;
            continue;
        }

        if let Some(parent) = output_path.parent() {
            fs::create_dir_all(parent).map_err(|error| error.to_string())?;
        }

        let mut output = File::create(&output_path).map_err(|error| error.to_string())?;
        std::io::copy(&mut entry, &mut output).map_err(|error| error.to_string())?;
        output.flush().map_err(|error| error.to_string())?;
        installed_files.push(output_path.to_string_lossy().to_string());
    }

    Ok(InstallSkillResult {
        target_dir: target_dir.to_string_lossy().to_string(),
        installed_files,
        unpacked: true,
    })
}

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_autostart::init(
            MacosLauncher::LaunchAgent,
            None::<Vec<&'static str>>,
        ))
        .invoke_handler(tauri::generate_handler![install_skill_package, force_native_drag])
        .setup(|app| {
            if let Some(win) = app.get_webview_window("main") {
                // 原生强杀 Windows 拉伸框：禁止 resizable 并死锁最大最小尺寸
                let _ = win.set_resizable(false);
                let _ = win.set_min_size(Some(tauri::LogicalSize::new(276.0, 428.0)));
                let _ = win.set_max_size(Some(tauri::LogicalSize::new(276.0, 428.0)));
                let _ = win.set_shadow(false); // 确保没有残影边框
            } else {
                let _ = WebviewWindowBuilder::new(app, "main", WebviewUrl::default())
                    .title("Desktop Companion")
                    .transparent(true)
                    .decorations(false)
                    .resizable(false)
                    .shadow(false)
                    .always_on_top(true)
                    .min_inner_size(276.0, 428.0)
                    .max_inner_size(276.0, 428.0)
                    .build()
                    .map_err(|error| error.to_string())?;
            }
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("failed to run tauri application");
}
