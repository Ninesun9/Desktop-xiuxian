type DesktopShellCardProps = {
  isTauri: boolean;
  isPinned: boolean;
  autostartEnabled: boolean;
  shellStatus: 'ready' | 'limited';
  desktopError: string | null;
  onTogglePinned: () => void;
  onToggleAutostart: () => void;
  onToggleVisibility: () => void;
  onQuit: () => void;
};

export function DesktopShellCard(props: DesktopShellCardProps) {
  const {
    isTauri,
    isPinned,
    autostartEnabled,
    shellStatus,
    desktopError,
    onTogglePinned,
    onToggleAutostart,
    onToggleVisibility,
    onQuit
  } = props;

  return (
    <section className="shell-card">
      <div className="shell-card-header">
        <div>
          <p className="eyebrow">Desktop Shell</p>
          <h2>Tauri 桌宠壳</h2>
        </div>
        <span className={`shell-badge shell-badge-${shellStatus}`}>{shellStatus === 'ready' ? '已接管' : '浏览器模式'}</span>
      </div>

      <p className="shell-copy">
        {isTauri
          ? '当前运行在 Tauri 容器内，桌宠可置顶、显隐、自启动，关闭按钮会直接退出。'
          : '当前是 Web 开发模式，托盘、自启动和原生窗口行为不会生效。'}
      </p>

      <div className="shell-metrics">
        <div>
          <span>置顶</span>
          <strong>{isPinned ? '开启' : '关闭'}</strong>
        </div>
        <div>
          <span>自启动</span>
          <strong>{autostartEnabled ? '开启' : '关闭'}</strong>
        </div>
      </div>

      <div className="shell-actions">
        <button type="button" onClick={onTogglePinned} disabled={!isTauri}>
          {isPinned ? '取消置顶' : '恢复置顶'}
        </button>
        <button type="button" onClick={onToggleAutostart} disabled={!isTauri}>
          {autostartEnabled ? '关闭自启动' : '开启自启动'}
        </button>
        <button type="button" onClick={onToggleVisibility} disabled={!isTauri}>
          显示或隐藏主窗口
        </button>
        <button type="button" onClick={onQuit} disabled={!isTauri}>
          退出桌宠
        </button>
      </div>

      {desktopError ? <p className="shell-error">{desktopError}</p> : null}
    </section>
  );
}