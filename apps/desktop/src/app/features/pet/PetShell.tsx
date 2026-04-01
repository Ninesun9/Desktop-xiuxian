import { useEffect, useState } from 'react';
import type { OpenClawStatus, PetState } from '@desktop-companion/shared';
import type { DockSide } from '../../lib/tauriDesktop';
import petFrame01 from '../../../../../../皮肤/ST-01.png';
import petFrame02 from '../../../../../../皮肤/ST-02.png';
import petFrame03 from '../../../../../../皮肤/ST-03.png';
import petFrame04 from '../../../../../../皮肤/ST-04.png';
import petFrame05 from '../../../../../../皮肤/ST-05.png';
import petFrame06 from '../../../../../../皮肤/ST-06.png';
import petFrame07 from '../../../../../../皮肤/ST-07.png';
import petFrame08 from '../../../../../../皮肤/ST-08.png';
import petFrame09 from '../../../../../../皮肤/ST-09.png';
import petFrame10 from '../../../../../../皮肤/ST-010.png';
import petFrame11 from '../../../../../../皮肤/ST-011.png';
import petFrame12 from '../../../../../../皮肤/ST-012.png';
import petFrame43 from '../../../../../../皮肤/ST-043.png';
import petFrame44 from '../../../../../../皮肤/ST-044.png';
import petFrame45 from '../../../../../../皮肤/ST-045.png';
import petFrame46 from '../../../../../../皮肤/ST-046.png';
import petFrame47 from '../../../../../../皮肤/ST-047.png';
import petFrame48 from '../../../../../../皮肤/ST-048.png';
import petFrame49 from '../../../../../../皮肤/ST-049.png';
import petFrame50 from '../../../../../../皮肤/ST-050.png';
import petFrame51 from '../../../../../../皮肤/ST-051.png';

const idleFrames = [
  petFrame01,
  petFrame02,
  petFrame03,
  petFrame04,
  petFrame05,
  petFrame06,
  petFrame07,
  petFrame08,
  petFrame09,
  petFrame10,
  petFrame11,
  petFrame12
];

const attentiveFrames = [
  petFrame43,
  petFrame44,
  petFrame45,
  petFrame46,
  petFrame47,
  petFrame48,
  petFrame49,
  petFrame50,
  petFrame51
];

const restingFrames = [petFrame10, petFrame11, petFrame12, petFrame11];

const speakingFrames = [petFrame43, petFrame45, petFrame47, petFrame49, petFrame51, petFrame49, petFrame47, petFrame45];

const walkingFrames = [petFrame01, petFrame03, petFrame05, petFrame07, petFrame09, petFrame07, petFrame05, petFrame03];

const lurkingFrames = [petFrame12, petFrame11, petFrame10, petFrame11];

type PetAnimationMode = 'idle' | 'attentive' | 'resting' | 'speaking' | 'walking' | 'lurking';

type PetShellProps = {
  petState: PetState | null;
  openclawStatus: OpenClawStatus | null;
  bubbleText: string;
  dockSide: DockSide;
  animationMode: PetAnimationMode;
  isReacting: boolean;
  isPeekingOut: boolean;
  isPanelOpen: boolean;
  isTauri: boolean;
  onInteract: () => void;
  onTogglePanel: () => void;
  onRequestHide: () => void;
  onRequestDrag: () => void;
};

const clampPercent = (value: number | null | undefined) => {
  if (typeof value !== 'number' || Number.isNaN(value)) {
    return 0;
  }

  return Math.max(0, Math.min(100, value));
};

export function PetShell(props: PetShellProps) {
  const { petState, openclawStatus, bubbleText, dockSide, animationMode, isReacting, isPeekingOut, isPanelOpen, isTauri, onInteract, onTogglePanel, onRequestHide, onRequestDrag } = props;
  const [frameIndex, setFrameIndex] = useState(0);
  const currentFrames =
    animationMode === 'speaking'
      ? speakingFrames
      : animationMode === 'walking'
        ? walkingFrames
        : animationMode === 'lurking'
          ? lurkingFrames
        : animationMode === 'attentive'
          ? attentiveFrames
          : animationMode === 'resting'
            ? restingFrames
            : idleFrames;
  const animationInterval =
    animationMode === 'lurking'
      ? 420
      : animationMode === 'resting'
      ? 320
      : animationMode === 'attentive'
        ? 110
        : animationMode === 'speaking'
          ? 90
          : animationMode === 'walking'
            ? 120
            : 160;

  useEffect(() => {
    setFrameIndex(0);
  }, [animationMode]);

  useEffect(() => {
    const timer = window.setInterval(() => {
      setFrameIndex((current) => (current + 1) % currentFrames.length);
    }, animationInterval);

    return () => {
      window.clearInterval(timer);
    };
  }, [animationInterval, currentFrames.length]);

  const energyPercent = clampPercent(petState?.energy);
  const intimacyPercent = clampPercent(petState?.intimacy);

  return (
    <section className={`pet-shell-stage pet-shell-stage-docked-${dockSide}`}>
      <div className={`pet-bubble ${isPanelOpen ? 'pet-bubble-active' : ''}`}>
        <p>{bubbleText}</p>
        <div className="pet-bubble-meters" aria-hidden="true">
          <span style={{ width: `${energyPercent}%` }} />
          <span style={{ width: `${intimacyPercent}%` }} />
        </div>
      </div>

      <button
        type="button"
        className="pet-shell-button"
        onClick={() => {
          onInteract();
          onTogglePanel();
        }}
        onMouseDown={(event) => {
          if (event.button === 0) {
            onRequestDrag();
          }
        }}
        aria-label="切换桌宠控制台"
      >
        <span className="pet-shadow" aria-hidden="true" />
        <img
          src={currentFrames[frameIndex]}
          alt="桌宠形象"
          className={`pet-shell-image pet-shell-image-${animationMode} ${isReacting ? 'pet-shell-image-reacting' : ''} ${isPeekingOut ? 'pet-shell-image-peeking-out' : ''}`}
          draggable={false}
        />
        <span className="pet-shell-glow" aria-hidden="true" />
      </button>
    </section>
  );
}