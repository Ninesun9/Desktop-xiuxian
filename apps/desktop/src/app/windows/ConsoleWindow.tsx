import { PetStatusCard } from '../features/pet/PetStatusCard';
import { DesktopShellCard } from '../features/shell/DesktopShellCard';
import { MailPanel } from '../features/chat/MailPanel';
import { ChatPanel } from '../features/chat/ChatPanel';
import type { PetState, OpenClawStatus } from '@desktop-companion/shared';

type ConsoleWindowProps = {
   petState: PetState | null;
   openclawStatus: OpenClawStatus | null;
   desktopError: string | null;
   autostartEnabled: boolean;
   isPinned: boolean;
   isTauriShell: boolean;
   onTogglePinned: () => void;
   onToggleAutostart: () => void;
   onQuit: () => void;
   mailProps: any;
   chatProps: any;
};

export const ConsoleWindow = (props: ConsoleWindowProps) => {
   return (
      <div className="console-window-container" style={{ 
          padding: '24px 32px', 
          background: 'linear-gradient(135deg, rgba(255, 247, 237, 1) 0%, rgba(255, 251, 235, 1) 100%)', 
          minHeight: '100vh', 
          maxHeight: '100vh',
          display: 'grid',
          gridTemplateRows: 'auto minmax(0, 1fr)',
          gap: 16,
          boxSizing: 'border-box'
      }}>
        <div className="dock-header" style={{ marginBottom: 0 }}>
          <div>
            <p className="eyebrow">Companion Console</p>
            <h2>桌宠控制台</h2>
          </div>
          <div className="dock-header-actions">
            <button
              type="button"
              className="dock-pill dock-pill-strong"
              onClick={() => {
                void props.onQuit();
              }}
              disabled={!props.isTauriShell}
            >
              退出
            </button>
          </div>
        </div>

        <div className="dock-scroll" style={{ paddingRight: 8, paddingBottom: 16 }}>
           <PetStatusCard petState={props.petState} openclawStatus={props.openclawStatus} />
           
           <DesktopShellCard
              isTauri={props.isTauriShell}
              isPinned={props.isPinned}
              autostartEnabled={props.autostartEnabled}
              shellStatus={props.isTauriShell ? 'ready' : 'limited'}
              desktopError={props.desktopError}
              onTogglePinned={props.onTogglePinned}
              onToggleAutostart={props.onToggleAutostart}
              onToggleVisibility={() => {}}
              onQuit={props.onQuit}
           />

           <MailPanel {...props.mailProps} />
           
           {!props.isTauriShell ? (
              <ChatPanel {...props.chatProps} />
           ) : null}
        </div>
      </div>
   );
};
