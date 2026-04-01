import { defaultWindowIcon } from '@tauri-apps/api/app';
import { Menu } from '@tauri-apps/api/menu';
import { TrayIcon } from '@tauri-apps/api/tray';
import { currentMonitor, getCurrentWindow, PhysicalPosition, PhysicalSize } from '@tauri-apps/api/window';
import { WebviewWindow } from '@tauri-apps/api/webviewWindow';
import { invoke } from '@tauri-apps/api/core';
import { disable, enable, isEnabled } from '@tauri-apps/plugin-autostart';

let traySetupPromise: Promise<void> | null = null;
let dockWatcherBound = false;
let dockWatcherTimer: number | null = null;
let dockWatcherCallback: ((side: DockSide) => void) | null = null;

const dockSnapThreshold = 36;
const dockSnapDelay = 140;
const edgePeekInset = 92;

export type DockSide = 'left' | 'right' | 'floating';
export type ScreenBounds = {
  x: number;
  y: number;
  width: number;
  height: number;
};

export const isTauriRuntime = () => typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;

const resolveWindowDockSide = async (): Promise<DockSide> => {
  if (!isTauriRuntime()) {
    return 'floating';
  }

  const currentWindow = getCurrentWindow();
  // Only applies to 'main' pet window
  if (currentWindow.label !== 'main') return 'floating';

  const [position, size, monitor] = await Promise.all([
    currentWindow.outerPosition(),
    currentWindow.outerSize(),
    currentMonitor()
  ]);

  if (!monitor) {
    return 'floating';
  }

  const leftGap = Math.abs(position.x - monitor.position.x);
  const rightGap = Math.abs(monitor.position.x + monitor.size.width - (position.x + size.width));

  if (leftGap <= dockSnapThreshold && leftGap <= rightGap) {
    return 'left';
  }

  if (rightGap <= dockSnapThreshold) {
    return 'right';
  }

  return 'floating';
};

const snapWindowToEdge = async (): Promise<DockSide> => {
  // Disabling the forceful SetPosition during drag, as it breaks the Windows OS drag loops.
  return resolveWindowDockSide();
};

export const getCurrentScreenBounds = async (): Promise<ScreenBounds | null> => {
  if (!isTauriRuntime()) {
    return null;
  }

  const monitor = await currentMonitor();
  if (!monitor) {
    return null;
  }

  return {
    x: monitor.position.x,
    y: monitor.position.y,
    width: monitor.size.width,
    height: monitor.size.height
  };
};

export const getWindowOuterRect = async () => {
  if (!isTauriRuntime()) {
    return null;
  }

  const currentWindow = getCurrentWindow();
  const [position, size] = await Promise.all([currentWindow.outerPosition(), currentWindow.outerSize()]);

  return {
    x: position.x,
    y: position.y,
    width: size.width,
    height: size.height
  };
};

export const moveWindowTo = async (x: number, y: number) => {
  if (!isTauriRuntime()) {
    return;
  }

  await getCurrentWindow().setPosition(new PhysicalPosition(Math.round(x), Math.round(y)));
};

export const getEdgePeekInset = () => edgePeekInset;

export const toggleWindowVisibility = async () => {
  if (!isTauriRuntime()) {
    return;
  }

  const currentWindow = getCurrentWindow();
  if (await currentWindow.isVisible()) {
    await currentWindow.hide();
    return;
  }

  await currentWindow.show();
  await currentWindow.setFocus();
};

export const openConsoleWindow = async () => {
  if (!isTauriRuntime()) return;
  try {
    const consoleWin = await WebviewWindow.getByLabel('console');
    if (consoleWin) {
      await consoleWin.show();
      await consoleWin.setFocus();
    } else {
      // Fallback manual spawning
      const win = new WebviewWindow('console', {
        title: '桌宠控制台',
        width: 812,
        height: 688,
        resizable: true,
        transparent: false,
        decorations: true
      });
      await win.once('tauri://created', async () => {
        await win.show();
      });
    }
  } catch (error) {
    console.error('Failed to open console window', error);
  }
};

export const quitDesktopShell = async () => {
  if (!isTauriRuntime()) {
    return;
  }
  
  // Close all windows
  const windows = await WebviewWindow.getAll();
  for (const win of windows) {
     await win.close();
  }
};

export const startWindowDrag = async () => {
  if (!isTauriRuntime()) {
    return;
  }

  await invoke('force_native_drag');
};

export const setWindowPinned = async (pinned: boolean) => {
  if (!isTauriRuntime()) {
    return;
  }

  await getCurrentWindow().setAlwaysOnTop(pinned);
};

export const readAutostartEnabled = async () => {
  if (!isTauriRuntime()) {
    return false;
  }

  return isEnabled();
};

export const setAutostartEnabled = async (enabled: boolean) => {
  if (!isTauriRuntime()) {
    return false;
  }

  if (enabled) {
    await enable();
  } else {
    await disable();
  }

  return isEnabled();
};

export const watchWindowDocking = async (callback: (side: DockSide) => void) => {
  dockWatcherCallback = callback;

  if (!isTauriRuntime()) {
    callback('floating');
    return () => {
      if (dockWatcherCallback === callback) {
        dockWatcherCallback = null;
      }
    };
  }

  if (!dockWatcherBound) {
    dockWatcherBound = true;
    const currentWindow = getCurrentWindow();
    
    // Only snap the 'main' window
    if (currentWindow.label === 'main') {
        await currentWindow.onMoved(() => {
          if (dockWatcherTimer) {
            window.clearTimeout(dockWatcherTimer);
          }

          dockWatcherTimer = window.setTimeout(async () => {
            const side = await resolveWindowDockSide().catch(() => 'floating' as DockSide);
            dockWatcherCallback?.(side);
          }, dockSnapDelay);
        });
    }
  }

  callback(await resolveWindowDockSide());

  return () => {
    if (dockWatcherCallback === callback) {
      dockWatcherCallback = null;
    }
  };
};

export const setupDesktopTray = async () => {
  if (!isTauriRuntime()) {
    return;
  }

  if (traySetupPromise) {
    return traySetupPromise;
  }

  traySetupPromise = (async () => {
    const trayMenu = await Menu.new({
      items: [
        {
          id: 'show',
          text: '显示或隐藏桌宠控制台',
          action: async () => {
            await openConsoleWindow();
          }
        },
        {
          id: 'quit',
          text: '退出桌宠',
          action: async () => {
            await quitDesktopShell();
          }
        }
      ]
    });

    const icon = await defaultWindowIcon().catch(() => null);

    await TrayIcon.new({
      tooltip: 'Desktop Companion',
      icon: icon ?? undefined,
      menu: trayMenu,
      menuOnLeftClick: false,
      action: async (event) => {
        if (event.type === 'Click' && event.button === 'Left' && event.buttonState === 'Up') {
          // Toggle main window on tray left click
          const mainWin = await WebviewWindow.getByLabel('main');
          if (mainWin) {
             if (await mainWin.isVisible()) {
                 await mainWin.hide();
             } else {
                 await mainWin.show();
             }
          }
        }
      }
    });
  })();

  return traySetupPromise;
};
