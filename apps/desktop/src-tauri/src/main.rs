#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use tauri::{Manager, WebviewWindowBuilder, WebviewUrl};

#[tauri::command]
fn window_pin(pinned: bool, app: tauri::AppHandle) -> Result<(), String> {
    if let Some(window) = app.get_webview_window("main") {
        window
            .set_always_on_top(pinned)
            .map_err(|error| error.to_string())?;
    }
    Ok(())
}

fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![window_pin])
        .setup(|app| {
            if app.get_webview_window("main").is_none() {
                let _ = WebviewWindowBuilder::new(app, "main", WebviewUrl::default())
                    .title("Desktop Companion")
                    .transparent(true)
                    .decorations(false)
                    .always_on_top(true)
                    .build()
                    .map_err(|error| error.to_string())?;
            }
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("failed to run tauri application");
}
