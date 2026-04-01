import { useState } from 'react';
import { PetShell } from '../features/pet/PetShell';
import { openConsoleWindow, quitDesktopShell, setWindowPinned } from '../lib/tauriDesktop';
import type { PetState, OpenClawStatus } from '@desktop-companion/shared';

type PetWindowProps = {
  petState: PetState | null;
  openclawStatus: OpenClawStatus | null;
  bubbleText: string | null;
  dockSide: 'left' | 'right' | 'floating';
  animationMode: 'idle' | 'attentive' | 'resting' | 'speaking' | 'walking' | 'lurking' | 'reacting';
  isReacting: boolean;
  isPeekingOut: boolean;
  isTauriShell: boolean;
  onInteract: () => void;
  onRequestDrag: () => void;
};

export const PetWindow = (props: PetWindowProps) => {
  const [showMenu, setShowMenu] = useState(false);

  return (
    <div 
      className={`pet-layer ${props.dockSide !== 'floating' ? `pet-layer-peeking pet-layer-peeking-${props.dockSide}` : ''}`}
      onContextMenu={(e) => { 
        e.preventDefault(); 
        setShowMenu(!showMenu); 
      }}
      onClick={() => {
        if (showMenu) setShowMenu(false);
      }}
    >
      <PetShell 
         petState={props.petState} 
         openclawStatus={props.openclawStatus} 
         bubbleText={props.bubbleText ?? ''} 
         dockSide={props.dockSide}
         animationMode={props.animationMode as any}
         isReacting={props.isReacting}
         isPeekingOut={props.isPeekingOut}
         isPanelOpen={false}
         isTauri={props.isTauriShell}
         onInteract={props.onInteract}
         onRequestHide={() => {}}
         onRequestDrag={props.onRequestDrag}
         onTogglePanel={() => {}} // Disabled in pet view
      />
      {showMenu && (
        <div 
          className="custom-context-menu" 
          style={{ 
            position: 'absolute', 
            left: props.dockSide === 'right' ? -120 : '50%',
            top: '20%', 
            background: 'rgba(255,255,255,0.95)', 
            border: '1px solid rgba(251, 191, 36, 0.4)', 
            borderRadius: 16, 
            padding: 12, 
            display: 'flex', 
            flexDirection: 'column', 
            gap: 8,
            boxShadow: '0 12px 28px rgba(120,53,15,0.18)', 
            backdropFilter: 'blur(8px)',
            zIndex: 999 
          }}
          onClick={(e) => e.stopPropagation()}
        >
           <p className="eyebrow" style={{ margin: 0, textAlign: 'center', marginBottom: 4 }}>功能栏</p>
           <button 
             className="dock-pill hover:bg-orange-50" 
             style={{ width: '100%' }}
             onClick={() => { setShowMenu(false); void openConsoleWindow(); }}
           >
             修仙状态 / 飞书
           </button>
           <button 
             className="dock-pill" 
             style={{ width: '100%' }}
             onClick={() => { setShowMenu(false); void setWindowPinned(true); }}
           >
             置顶保持
           </button>
           <button 
             className="dock-pill" 
             style={{ width: '100%', borderColor: 'rgba(239,68,68,0.3)', color: '#b91c1c' }}
             onClick={() => { setShowMenu(false); void quitDesktopShell(); }}
           >
             结束修炼 (退出)
           </button>
        </div>
      )}
    </div>
  );
};
