import { useEffect, useRef, useState } from 'react';
import type {
  AckDirectMessageRequest,
  ChatMessage,
  CompanionSession,
  ContactListResponse,
  ContactRequest,
  ContactRequestListResponse,
  ConversationListResponse,
  CreateContactRequest,
  CreateDirectConversationRequest,
  CreateDirectMessageRequest,
  DeviceLoginResponse,
  CreateSessionRequest,
  DirectConversation,
  DirectMessageAttachment,
  DirectMessage,
  DirectMessageListResponse,
  OpenClawStatus,
  ContactSummary,
  PetState
} from '@desktop-companion/shared';
import { ChatPanel } from './app/features/chat/ChatPanel';
import { MailPanel } from './app/features/chat/MailPanel';
import { PetShell } from './app/features/pet/PetShell';
import { PetStatusCard } from './app/features/pet/PetStatusCard';
import { DEFAULT_PET_PERSONALITY, getPetBehaviorProfile } from './app/features/pet/personalityProfiles';
import {
  decryptAttachmentForCurrentUser,
  encryptAttachmentForPeer,
  ensureTransportIdentityRegistered
} from './app/lib/localTransportSecurity';
import { installSkillArchive } from './app/lib/localSkillInstaller';
import { readLocalOpenClawStatus, requestLocalOpenClawResponse } from './app/lib/localOpenClaw';
import { DesktopShellCard } from './app/features/shell/DesktopShellCard';
import { PetWindow } from './app/windows/PetWindow';
import { ConsoleWindow } from './app/windows/ConsoleWindow';
import { getCurrentWindow } from '@tauri-apps/api/window';
import {
  type DockSide,
  getCurrentScreenBounds,
  getEdgePeekInset,
  getWindowOuterRect,
  isTauriRuntime,
  moveWindowTo,
  quitDesktopShell,
  readAutostartEnabled,
  setAutostartEnabled,
  startWindowDrag,
  setWindowPinned,
  setupDesktopTray,
  toggleWindowVisibility,
  watchWindowDocking
} from './app/lib/tauriDesktop';

const apiBase = import.meta.env.VITE_API_BASE_URL ?? 'http://localhost:3000';
const QUICK_PROMPTS = ['帮我拆今天的待办', '给我一个 90 分钟专注计划', '提醒我晚上休息和修炼'];

type PetBehaviorState = 'idle' | 'attentive' | 'resting' | 'speaking' | 'walking' | 'lurking' | 'reacting';

const BEHAVIOR_MIN_DWELL: Record<PetBehaviorState, number> = {
  idle: 3200,
  attentive: 2200,
  resting: 5200,
  speaking: 0,
  walking: 1800,
  lurking: 5600,
  reacting: 0
};

const BEHAVIOR_COOLDOWN: Record<PetBehaviorState, number> = {
  idle: 0,
  attentive: 0,
  resting: 0,
  speaking: 0,
  walking: 6200,
  lurking: 9400,
  reacting: 1200
};

const IMMEDIATE_BEHAVIORS = new Set<PetBehaviorState>(['reacting', 'speaking', 'attentive']);

const resolvePetBehavior = (params: {
  isReacting: boolean;
  isWalking: boolean;
  isSpeaking: boolean;
  isLurking: boolean;
  isPanelOpen: boolean;
  energy: number;
  connected: boolean;
}): PetBehaviorState => {
  if (params.isReacting) {
    return 'reacting';
  }

  if (params.isWalking) {
    return 'walking';
  }

  if (params.isSpeaking) {
    return 'speaking';
  }

  if (params.isLurking) {
    return 'lurking';
  }

  if (!params.connected || params.energy <= 25) {
    return 'resting';
  }

  if (params.isPanelOpen || params.energy >= 70) {
    return 'attentive';
  }

  return 'idle';
};

const summarizeSpeechBubble = (content: string, fallback: string) => {
  const normalized = content.replace(/\s+/g, ' ').trim();
  if (!normalized) {
    return fallback;
  }

  const segments = normalized
    .split(/(?<=[。！？!?；;])/)
    .map((segment) => segment.trim())
    .filter(Boolean);
  const candidate = (segments.find((segment) => segment.length >= 6) ?? segments.at(-1) ?? normalized)
    .replace(/^[，。！？,.!?:：;\s]+|[，。！？,.!?:：;\s]+$/g, '');

  if (!candidate) {
    return fallback;
  }

  const condensed = candidate.length > 20 ? `${candidate.slice(0, 20)}…` : candidate;
  return /[。！？!?…]$/.test(condensed) ? condensed : `${condensed}…`;
};

const getPlatform = () => {
  const platform = navigator.platform.toLowerCase();
  if (platform.includes('mac')) {
    return 'macos' as const;
  }
  if (platform.includes('linux')) {
    return 'linux' as const;
  }
  return 'windows' as const;
};

const getDeviceId = () => {
  const stored = window.localStorage.getItem('desktop-companion-device-id');
  if (stored) {
    return stored;
  }

  const nextId = crypto.randomUUID();
  window.localStorage.setItem('desktop-companion-device-id', nextId);
  return nextId;
};

async function readJsonOrThrow<T>(response: Response): Promise<T> {
  if (!response.ok) {
    const payload = (await response.json().catch(() => null)) as { message?: string; detail?: string } | null;
    throw new Error(payload?.message ?? payload?.detail ?? `请求失败 (${response.status})`);
  }

  return response.json() as Promise<T>;
}

export default function App() {
  const [isTauriShell] = useState(isTauriRuntime());
  const [windowLabel, setWindowLabel] = useState('main');
  const [petState, setPetState] = useState<PetState | null>(null);
  const [openclawStatus, setOpenclawStatus] = useState<OpenClawStatus | null>(null);
  const [session, setSession] = useState<CompanionSession | null>(null);
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [draft, setDraft] = useState(() => window.localStorage.getItem('desktop-companion-draft') ?? '帮我整理今天的工作和休息安排');
  const [isSending, setIsSending] = useState(false);
  const [currentUserId, setCurrentUserId] = useState<string | null>(null);
  const [accessToken, setAccessToken] = useState<string | null>(null);
  const [isPinned, setIsPinned] = useState(true);
  const [autostartEnabled, setAutostartState] = useState(false);
  const [desktopError, setDesktopError] = useState<string | null>(null);
  const [isPanelOpen, setIsPanelOpen] = useState(false);
  const [dockSide, setDockSide] = useState<DockSide>('floating');
  const [petReactionText, setPetReactionText] = useState<string | null>(null);
  const [isPetReacting, setIsPetReacting] = useState(false);
  const [speechBubbleText, setSpeechBubbleText] = useState<string | null>(null);
  const [isAutoWalking, setIsAutoWalking] = useState(false);
  const [isAutoLurking, setIsAutoLurking] = useState(false);
  const [petBehavior, setPetBehavior] = useState<PetBehaviorState>('idle');
  const [isPeekingOut, setIsPeekingOut] = useState(false);
  const [isMouseNearEdge, setIsMouseNearEdge] = useState(false);
  const [contacts, setContacts] = useState<ContactSummary[]>([]);
  const [contactRequests, setContactRequests] = useState<ContactRequest[]>([]);
  const [directConversations, setDirectConversations] = useState<DirectConversation[]>([]);
  const [activeDirectConversationId, setActiveDirectConversationId] = useState<string | null>(null);
  const [directMessages, setDirectMessages] = useState<DirectMessage[]>([]);
  const [directDraft, setDirectDraft] = useState('');
  const [isDirectSending, setIsDirectSending] = useState(false);
  const [isDirectAssisting, setIsDirectAssisting] = useState(false);
  const [installingAttachmentId, setInstallingAttachmentId] = useState<string | null>(null);
  const [pendingDirectAssistantMeta, setPendingDirectAssistantMeta] = useState<CreateDirectMessageRequest['assistantMeta'] | undefined>(undefined);
  const behaviorTimerRef = useRef<number | null>(null);
  const desiredBehaviorRef = useRef<PetBehaviorState>('idle');
  const behaviorMetaRef = useRef<{
    state: PetBehaviorState;
    enteredAt: number;
    cooldownUntil: Partial<Record<PetBehaviorState, number>>;
  }>({
    state: 'idle',
    enteredAt: Date.now(),
    cooldownUntil: {}
  });

  const authHeaders = accessToken
    ? {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${accessToken}`
      }
    : null;

  const energyRatio = Math.max(0, Math.min(1, (petState?.energy ?? 0) / 100));
  const intimacyRatio = Math.max(0, Math.min(1, (petState?.intimacy ?? 0) / 100));
  const personalityId = DEFAULT_PET_PERSONALITY;
  const behaviorProfile = getPetBehaviorProfile(personalityId, energyRatio, intimacyRatio);
  const activeDirectConversation = directConversations.find((conversation) => conversation.id === activeDirectConversationId) ?? null;

  useEffect(() => {
    if (isTauriShell) {
      setWindowLabel(getCurrentWindow().label);
    }
  }, [isTauriShell]);

  useEffect(() => {
    window.localStorage.setItem('desktop-companion-draft', draft);
  }, [draft]);

  useEffect(() => {
    if (!petReactionText) {
      return;
    }

    const timer = window.setTimeout(() => {
      setPetReactionText(null);
    }, 2600);

    return () => {
      window.clearTimeout(timer);
    };
  }, [petReactionText]);

  useEffect(() => {
    if (!isPetReacting) {
      return;
    }

    const timer = window.setTimeout(() => {
      setIsPetReacting(false);
    }, 900);

    return () => {
      window.clearTimeout(timer);
    };
  }, [isPetReacting]);

  useEffect(() => {
    if (!speechBubbleText) {
      return;
    }

    const timer = window.setTimeout(() => {
      setSpeechBubbleText(null);
    }, 2200);

    return () => {
      window.clearTimeout(timer);
    };
  }, [speechBubbleText]);

  useEffect(() => {
    if (dockSide === 'floating') {
      setIsAutoLurking(false);
      setIsMouseNearEdge(false);
    }
  }, [dockSide]);

  useEffect(() => {
    if (!isPanelOpen && !isSending && !speechBubbleText) {
      return;
    }

    setIsAutoLurking(false);
  }, [isPanelOpen, isSending, speechBubbleText]);

  useEffect(() => {
    return () => {
      if (behaviorTimerRef.current) {
        window.clearTimeout(behaviorTimerRef.current);
      }
    };
  }, []);

  useEffect(() => {
    if (!isTauriShell) {
      return;
    }

    let disposeDockWatcher: (() => void) | undefined;
    let cancelled = false;

    const setupShell = async () => {
      try {
        await setupDesktopTray();
        setAutostartState(await readAutostartEnabled());
        disposeDockWatcher = await watchWindowDocking((side) => {
          if (!cancelled) {
            setDockSide(side);
          }
        });
      } catch (error) {
        setDesktopError(error instanceof Error ? error.message : 'Tauri 桌面壳初始化失败');
      }
    };

    void setupShell();

    return () => {
      cancelled = true;
      disposeDockWatcher?.();
    };
  }, [isTauriShell]);



  useEffect(() => {
    const init = async () => {
      const loginResponse = await fetch(`${apiBase}/api/v1/auth/device-login`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          deviceId: getDeviceId(),
          deviceName: 'desktop-companion',
          platform: getPlatform()
        })
      }).then((response) => response.json() as Promise<DeviceLoginResponse>);

      setAccessToken(loginResponse.accessToken);
      setCurrentUserId(loginResponse.user.id);

      const nextHeaders = {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${loginResponse.accessToken}`
      };

      const [pet, provider, nextSession] = await Promise.all([
        fetch(`${apiBase}/api/v1/pet/state`, {
          headers: nextHeaders
        }).then((response) => response.json() as Promise<PetState>),
        readLocalOpenClawStatus(),
        fetch(`${apiBase}/api/v1/sessions`, {
          method: 'POST',
          headers: nextHeaders,
          body: JSON.stringify({ provider: 'openclaw', title: '桌面伴侣对话' } satisfies CreateSessionRequest)
        }).then((response) => response.json() as Promise<CompanionSession>)
      ]);

      setPetState(pet);
      setOpenclawStatus(provider);
      setSession(nextSession);
      setMessages([
        {
          id: `system-${nextSession.id}`,
          sessionId: nextSession.id,
          role: 'system',
          content: '新的陪伴会话已建立。你可以直接输入需求，或点下面的快捷提示。',
          createdAt: new Date().toISOString()
        }
      ]);
    };

    void init();
  }, []);

  useEffect(() => {
    let cancelled = false;

    const refreshStatus = async () => {
      const nextStatus = await readLocalOpenClawStatus();
      if (!cancelled) {
        setOpenclawStatus(nextStatus);
      }
    };

    void refreshStatus();
    const timer = window.setInterval(() => {
      void refreshStatus();
    }, 15000);

    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, []);

  useEffect(() => {
    if (!isTauriShell) {
      return;
    }

    let cancelled = false;
    let timeoutId: number | null = null;

    const scheduleNextWalk = () => {
      const delay = behaviorProfile.roamDelayMin + Math.round(Math.random() * Math.max(1200, behaviorProfile.roamDelayMax - behaviorProfile.roamDelayMin));

      timeoutId = window.setTimeout(async () => {
        const shouldRoam = Math.random() <= behaviorProfile.roamChance;

        if (cancelled || isPanelOpen || isSending || isAutoLurking || dockSide !== 'floating' || !shouldRoam) {
          if (!cancelled) {
            scheduleNextWalk();
          }
          return;
        }

        const [bounds, rect] = await Promise.all([getCurrentScreenBounds(), getWindowOuterRect()]);
        if (!bounds || !rect) {
          if (!cancelled) {
            scheduleNextWalk();
          }
          return;
        }

        const horizontalPadding = 48;
        const verticalPadding = 96;
        const nextX = bounds.x + horizontalPadding + Math.random() * Math.max(0, bounds.width - rect.width - horizontalPadding * 2);
        const nextY = bounds.y + verticalPadding + Math.random() * Math.max(0, bounds.height - rect.height - verticalPadding - 60);
        const startX = rect.x;
        const startY = rect.y;
        const duration = 960;
        const startedAt = performance.now();

        setIsAutoWalking(true);

        const step = async (now: number) => {
          if (cancelled) {
            setIsAutoWalking(false);
            return;
          }

          const progress = Math.min(1, (now - startedAt) / duration);
          const eased = 1 - Math.pow(1 - progress, 3);

          await moveWindowTo(startX + (nextX - startX) * eased, startY + (nextY - startY) * eased);

          if (progress < 1) {
            window.requestAnimationFrame((nextNow) => {
              void step(nextNow);
            });
            return;
          }

          setIsAutoWalking(false);
          setPetReactionText('我换了个位置继续守着你。');

          if (!cancelled) {
            scheduleNextWalk();
          }
        };

        window.requestAnimationFrame((now) => {
          void step(now);
        });
      }, delay);
    };

    scheduleNextWalk();

    return () => {
      cancelled = true;
      if (timeoutId) {
        window.clearTimeout(timeoutId);
      }
    };
  }, [behaviorProfile.roamChance, behaviorProfile.roamDelayMax, behaviorProfile.roamDelayMin, dockSide, isAutoLurking, isPanelOpen, isSending, isTauriShell]);

  useEffect(() => {
    if (!isTauriShell) {
      return;
    }

    let cancelled = false;
    let timeoutId: number | null = null;

    const scheduleNextLurk = () => {
      const delay = behaviorProfile.roamDelayMax + Math.round(Math.random() * Math.max(1200, behaviorProfile.roamDelayMax - behaviorProfile.roamDelayMin));

      timeoutId = window.setTimeout(async () => {
        const energy = petState?.energy ?? 0;
        const shouldRestByEnergy = energy <= 45;
        const shouldRestByChance = Math.random() <= behaviorProfile.lurkChance;

        if (
          cancelled ||
          isPanelOpen ||
          isSending ||
          isAutoWalking ||
          Boolean(speechBubbleText) ||
          Boolean(petReactionText) ||
          dockSide !== 'floating' ||
          (!shouldRestByEnergy && !shouldRestByChance)
        ) {
          if (!cancelled) {
            scheduleNextLurk();
          }
          return;
        }

        const [bounds, rect] = await Promise.all([getCurrentScreenBounds(), getWindowOuterRect()]);
        if (!bounds || !rect) {
          if (!cancelled) {
            scheduleNextLurk();
          }
          return;
        }

        const targetSide = Math.random() >= 0.5 ? 'left' : 'right';
        const targetX = targetSide === 'left' ? bounds.x : bounds.x + bounds.width - rect.width;
        const minY = bounds.y + Math.round(bounds.height * 0.28);
        const maxY = bounds.y + bounds.height - rect.height - 52;
        const targetY = Math.min(maxY, Math.max(minY, bounds.y + Math.round(bounds.height * 0.55 + Math.random() * 72)));
        const startX = rect.x;
        const startY = rect.y;
        const duration = 880;
        const startedAt = performance.now();

        setIsAutoWalking(true);

        const step = async (now: number) => {
          if (cancelled) {
            setIsAutoWalking(false);
            return;
          }

          const progress = Math.min(1, (now - startedAt) / duration);
          const eased = 1 - Math.pow(1 - progress, 3);

          await moveWindowTo(startX + (targetX - startX) * eased, startY + (targetY - startY) * eased);

          if (progress < 1) {
            window.requestAnimationFrame((nextNow) => {
              void step(nextNow);
            });
            return;
          }

          setIsAutoWalking(false);
          setIsAutoLurking(true);
          setPetReactionText('我先贴边歇会，你一靠近我就出来。');

          if (!cancelled) {
            scheduleNextLurk();
          }
        };

        window.requestAnimationFrame((now) => {
          void step(now);
        });
      }, delay);
    };

    scheduleNextLurk();

    return () => {
      cancelled = true;
      if (timeoutId) {
        window.clearTimeout(timeoutId);
      }
    };
  }, [behaviorProfile.lurkChance, behaviorProfile.roamDelayMax, behaviorProfile.roamDelayMin, dockSide, isAutoWalking, isPanelOpen, isSending, isTauriShell, petReactionText, petState?.energy, speechBubbleText]);

  const requestedBehavior = resolvePetBehavior({
    isReacting: isPetReacting,
    isWalking: isAutoWalking,
    isSpeaking: isSending || Boolean(speechBubbleText),
    isLurking: isAutoLurking && dockSide !== 'floating' && !isPanelOpen,
    isPanelOpen,
    energy: petState?.energy ?? 0,
    connected: Boolean(openclawStatus?.connected)
  });

  useEffect(() => {
    const commitBehavior = (nextBehavior: PetBehaviorState) => {
      if (behaviorTimerRef.current) {
        window.clearTimeout(behaviorTimerRef.current);
        behaviorTimerRef.current = null;
      }

      behaviorMetaRef.current = {
        state: nextBehavior,
        enteredAt: Date.now(),
        cooldownUntil: {
          ...behaviorMetaRef.current.cooldownUntil,
          [nextBehavior]: Date.now() + BEHAVIOR_COOLDOWN[nextBehavior]
        }
      };
      setPetBehavior(nextBehavior);
    };

    const scheduleBehaviorResolution = () => {
      const now = Date.now();
      const currentBehavior = behaviorMetaRef.current.state;
      const nextBehavior = desiredBehaviorRef.current;

      if (nextBehavior === currentBehavior) {
        return;
      }

      if (IMMEDIATE_BEHAVIORS.has(nextBehavior)) {
        commitBehavior(nextBehavior);
        return;
      }

      const cooldownUntil = behaviorMetaRef.current.cooldownUntil[nextBehavior] ?? 0;
      if (cooldownUntil > now) {
        if (behaviorTimerRef.current) {
          window.clearTimeout(behaviorTimerRef.current);
        }

        behaviorTimerRef.current = window.setTimeout(() => {
          behaviorTimerRef.current = null;
          scheduleBehaviorResolution();
        }, cooldownUntil - now);
        return;
      }

      const remainingDwell = BEHAVIOR_MIN_DWELL[currentBehavior] - (now - behaviorMetaRef.current.enteredAt);
      if (remainingDwell > 0 && !IMMEDIATE_BEHAVIORS.has(currentBehavior)) {
        if (behaviorTimerRef.current) {
          window.clearTimeout(behaviorTimerRef.current);
        }

        behaviorTimerRef.current = window.setTimeout(() => {
          behaviorTimerRef.current = null;
          scheduleBehaviorResolution();
        }, remainingDwell);
        return;
      }

      commitBehavior(nextBehavior);
    };

    desiredBehaviorRef.current = requestedBehavior;
    scheduleBehaviorResolution();
  }, [requestedBehavior]);

  useEffect(() => {
    if (petBehavior !== 'lurking' || dockSide === 'floating' || isPanelOpen) {
      setIsPeekingOut(false);
      return;
    }

    if (isMouseNearEdge) {
      setIsPeekingOut(true);
      return;
    }

    let cancelled = false;
    let peekTimer: number | null = null;
    let settleTimer: number | null = null;

    const schedulePeek = () => {
      peekTimer = window.setTimeout(() => {
        if (cancelled) {
          return;
        }

        setIsPeekingOut(true);
        settleTimer = window.setTimeout(() => {
          if (cancelled) {
            return;
          }

          setIsPeekingOut(false);
          schedulePeek();
        }, behaviorProfile.peekDuration);
      }, behaviorProfile.peekIntervalMin + Math.round(Math.random() * Math.max(600, behaviorProfile.peekIntervalMax - behaviorProfile.peekIntervalMin)));
    };

    schedulePeek();

    return () => {
      cancelled = true;
      if (peekTimer) {
        window.clearTimeout(peekTimer);
      }
      if (settleTimer) {
        window.clearTimeout(settleTimer);
      }
    };
  }, [behaviorProfile.peekDuration, behaviorProfile.peekIntervalMax, behaviorProfile.peekIntervalMin, dockSide, isMouseNearEdge, isPanelOpen, petBehavior]);

  const persistSessionMessage = async (
    sessionId: string,
    content: string,
    role: 'user' | 'assistant' | 'system',
    headers: Record<string, string>
  ) => {
    await fetch(`${apiBase}/api/v1/sessions/${sessionId}/messages`, {
      method: 'POST',
      headers,
      body: JSON.stringify({ content, role, stream: false })
    });
  };

  const refreshSocialState = async (headers: Record<string, string>, preferredConversationId?: string | null) => {
    const [contactsResponse, requestsResponse, conversationsResponse] = await Promise.all([
      fetch(`${apiBase}/api/v1/social/contacts`, {
        headers
      }).then((response) => readJsonOrThrow<ContactListResponse>(response)),
      fetch(`${apiBase}/api/v1/social/contact-requests`, {
        headers
      }).then((response) => readJsonOrThrow<ContactRequestListResponse>(response)),
      fetch(`${apiBase}/api/v1/social/conversations`, {
        headers
      }).then((response) => readJsonOrThrow<ConversationListResponse>(response))
    ]);

    setContacts(contactsResponse.items);
    setContactRequests(requestsResponse.items);
    setDirectConversations(conversationsResponse.items);

    const nextActiveConversationId =
      conversationsResponse.items.find((item) => item.id === preferredConversationId)?.id
      ?? conversationsResponse.items[0]?.id
      ?? null;

    setActiveDirectConversationId(nextActiveConversationId);
  };

  const loadDirectConversationMessages = async (headers: Record<string, string>, conversationId: string) => {
    const response = await fetch(`${apiBase}/api/v1/social/conversations/${conversationId}/messages`, {
      headers
    }).then((result) => readJsonOrThrow<DirectMessageListResponse>(result));

    setDirectMessages(response.items);
    return response.items;
  };

  const acknowledgeDirectMessage = async (messageId: string, payload?: AckDirectMessageRequest) => {
    if (!authHeaders) {
      return null;
    }

    return fetch(`${apiBase}/api/v1/social/messages/${messageId}/ack`, {
      method: 'POST',
      headers: authHeaders,
      body: JSON.stringify(payload ?? {})
    }).then((response) => readJsonOrThrow<DirectMessage>(response));
  };

  useEffect(() => {
    if (!authHeaders || !currentUserId) {
      return;
    }

    let cancelled = false;

    const sync = async () => {
      try {
        await ensureTransportIdentityRegistered(apiBase, authHeaders, currentUserId);
        await refreshSocialState(authHeaders, activeDirectConversationId);
      } catch (error) {
        if (!cancelled) {
          setDesktopError(error instanceof Error ? `私聊数据同步失败：${error.message}` : '私聊数据同步失败');
        }
      }
    };

    void sync();
    const timer = window.setInterval(() => {
      void sync();
    }, 12000);

    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, [activeDirectConversationId, authHeaders, currentUserId]);

  useEffect(() => {
    if (!authHeaders || !activeDirectConversationId) {
      setDirectMessages([]);
      return;
    }

    let cancelled = false;

    const loadMessages = async () => {
      try {
        await loadDirectConversationMessages(authHeaders, activeDirectConversationId);

        await fetch(`${apiBase}/api/v1/social/conversations/${activeDirectConversationId}/read`, {
          method: 'POST',
          headers: authHeaders,
          body: '{}'
        }).then((response) => readJsonOrThrow<{ accepted: boolean }>(response));

        if (!cancelled) {
          await refreshSocialState(authHeaders, activeDirectConversationId);
        }
      } catch (error) {
        if (!cancelled) {
          setDesktopError(error instanceof Error ? `拉取私聊消息失败：${error.message}` : '拉取私聊消息失败');
        }
      }
    };

    void loadMessages();

    return () => {
      cancelled = true;
    };
  }, [activeDirectConversationId, authHeaders]);

  const ensureDirectConversation = async (participantUserId: string) => {
    if (!authHeaders) {
      return null;
    }

    const currentConversation = directConversations.find((item) => item.participantUserId === participantUserId);
    if (currentConversation) {
      setActiveDirectConversationId(currentConversation.id);
      return currentConversation;
    }

    const created = await fetch(`${apiBase}/api/v1/social/conversations`, {
      method: 'POST',
      headers: authHeaders,
      body: JSON.stringify({ participantUserId } satisfies CreateDirectConversationRequest)
    }).then((response) => readJsonOrThrow<DirectConversation>(response));

    await refreshSocialState(authHeaders, created.id);
    return created;
  };

  const handleRespondContactRequest = async (requestId: string, action: 'accept' | 'reject') => {
    if (!authHeaders) {
      return;
    }

    try {
      await fetch(`${apiBase}/api/v1/social/contact-requests/${requestId}/${action}`, {
        method: 'POST',
        headers: authHeaders,
        body: '{}'
      }).then((response) => readJsonOrThrow<ContactRequest>(response));
      await refreshSocialState(authHeaders, activeDirectConversationId);
      setSpeechBubbleText(action === 'accept' ? '新的通讯录已经建立。' : '我替你婉拒了这次申请。');
    } catch (error) {
      setDesktopError(error instanceof Error ? `处理申请失败：${error.message}` : '处理申请失败');
    }
  };

  const handleCreateContactRequest = async (target: string, remark?: string) => {
    if (!authHeaders || !target.trim()) {
      return;
    }

    try {
      await fetch(`${apiBase}/api/v1/social/contact-requests`, {
        method: 'POST',
        headers: authHeaders,
        body: JSON.stringify({ target, remark } satisfies CreateContactRequest)
      }).then((response) => readJsonOrThrow<ContactRequest>(response));
      await refreshSocialState(authHeaders, activeDirectConversationId);
      setSpeechBubbleText('我已经把灵讯申请送出去了。');
    } catch (error) {
      setDesktopError(error instanceof Error ? `发送通讯录申请失败：${error.message}` : '发送通讯录申请失败');
    }
  };

  const handleSendDirectMessage = async () => {
    if (!authHeaders || !activeDirectConversation || !directDraft.trim() || isDirectSending) {
      return;
    }

    try {
      setIsDirectSending(true);
      await fetch(`${apiBase}/api/v1/social/conversations/${activeDirectConversation.id}/messages`, {
        method: 'POST',
        headers: authHeaders,
        body: JSON.stringify({
          kind: 'text',
          content: directDraft.trim(),
          assistantMeta: pendingDirectAssistantMeta
        } satisfies CreateDirectMessageRequest)
      }).then((response) => readJsonOrThrow<DirectMessage>(response));

      setDirectDraft('');
      setPendingDirectAssistantMeta(undefined);
      await refreshSocialState(authHeaders, activeDirectConversation.id);
      const messagesResponse = await fetch(`${apiBase}/api/v1/social/conversations/${activeDirectConversation.id}/messages`, {
        headers: authHeaders
      }).then((response) => readJsonOrThrow<DirectMessageListResponse>(response));
      setDirectMessages(messagesResponse.items);
      setSpeechBubbleText('灵讯已经送出。');
    } catch (error) {
      setDesktopError(error instanceof Error ? `发送私聊失败：${error.message}` : '发送私聊失败');
    } finally {
      setIsDirectSending(false);
    }
  };

  const handleSendDirectAttachment = async (file: File, kind: 'file' | 'skill') => {
    if (!authHeaders || !activeDirectConversation || !currentUserId || isDirectSending) {
      return;
    }

    try {
      setIsDirectSending(true);
      const participant = contacts.find((item) => item.contactUserId === activeDirectConversation.participantUserId);
      if (!participant?.transportPublicKey) {
        throw new Error('对方还没有准备好加密投递，请先让对方打开一次桌面端');
      }

      const plainBytes = new Uint8Array(await file.arrayBuffer());
      const encryptedPayload = await encryptAttachmentForPeer(currentUserId, participant.transportPublicKey, plainBytes);
      const formData = new FormData();
      const targetDir = kind === 'skill' ? `~/.openclaw/skills/${file.name.replace(/\.zip$/i, '')}` : undefined;
      formData.set('manifest', JSON.stringify({
        kind,
        content: directDraft.trim() || (kind === 'skill' ? '我给你发了一个 skill 包。' : '我给你发了一个文件。'),
        requiresAck: true,
        sha256: encryptedPayload.sha256,
        targetDir,
        encryption: encryptedPayload.encryption
      }));
      formData.set('file', new File([encryptedPayload.bytes], file.name, { type: 'application/octet-stream' }));

      await fetch(`${apiBase}/api/v1/social/conversations/${activeDirectConversation.id}/attachments`, {
        method: 'POST',
        headers: {
          Authorization: `Bearer ${accessToken}`
        },
        body: formData
      }).then((response) => readJsonOrThrow<DirectMessage>(response));

      setDirectDraft('');
      await refreshSocialState(authHeaders, activeDirectConversation.id);
      await loadDirectConversationMessages(authHeaders, activeDirectConversation.id);
      setSpeechBubbleText(kind === 'skill' ? 'skill 包已经加密托飞鸽送出。' : '文件已经加密送出。');
    } catch (error) {
      setDesktopError(error instanceof Error ? `发送附件失败：${error.message}` : '发送附件失败');
    } finally {
      setIsDirectSending(false);
    }
  };

  const handleDownloadDirectAttachment = async (messageId: string, attachment: DirectMessageAttachment) => {
    if (!authHeaders || !activeDirectConversation || !currentUserId) {
      return;
    }

    try {
      const response = await fetch(
        `${apiBase}${attachment.downloadUrl}?conversationId=${encodeURIComponent(activeDirectConversation.id)}`,
        {
          headers: authHeaders
        }
      );

      if (!response.ok) {
        throw new Error(`下载失败 (${response.status})`);
      }

      const encryptedBytes = new Uint8Array(await response.arrayBuffer());
      const plainBytes = await decryptAttachmentForCurrentUser(currentUserId, attachment, encryptedBytes);
      const blob = new Blob([
        plainBytes.buffer.slice(plainBytes.byteOffset, plainBytes.byteOffset + plainBytes.byteLength) as ArrayBuffer
      ], {
        type: attachment.contentType || 'application/octet-stream'
      });
      const objectUrl = URL.createObjectURL(blob);
      const anchor = document.createElement('a');
      anchor.href = objectUrl;
      anchor.download = attachment.name;
      document.body.append(anchor);
      anchor.click();
      anchor.remove();
      URL.revokeObjectURL(objectUrl);

      const message = directMessages.find((item) => item.id === messageId);
      if (message?.receiverUserId === currentUserId && attachment.transferKind === 'file') {
        await acknowledgeDirectMessage(messageId, { note: 'file-downloaded' });
        await refreshSocialState(authHeaders, activeDirectConversation.id);
        await loadDirectConversationMessages(authHeaders, activeDirectConversation.id);
      }

      setSpeechBubbleText(attachment.transferKind === 'skill' ? 'skill 包已经取下来。' : '附件已经下载。');
    } catch (error) {
      setDesktopError(error instanceof Error ? `下载附件失败：${error.message}` : '下载附件失败');
    }
  };

  const handleInstallSkillAttachment = async (messageId: string, attachment: DirectMessageAttachment) => {
    if (!authHeaders || !activeDirectConversation || !currentUserId) {
      return;
    }

    try {
      setInstallingAttachmentId(attachment.id);
      const response = await fetch(
        `${apiBase}${attachment.downloadUrl}?conversationId=${encodeURIComponent(activeDirectConversation.id)}`,
        {
          headers: authHeaders
        }
      );

      if (!response.ok) {
        throw new Error(`下载失败 (${response.status})`);
      }

      const encryptedBytes = new Uint8Array(await response.arrayBuffer());
      const plainBytes = await decryptAttachmentForCurrentUser(currentUserId, attachment, encryptedBytes);
      const result = await installSkillArchive(plainBytes, attachment.installHint?.targetDir);

      await acknowledgeDirectMessage(messageId, {
        note: `skill-installed:${result.installedFiles.length}`,
        installState: {
          status: 'installed',
          targetDir: result.targetDir,
          note: `已安装 ${result.installedFiles.length} 个文件`
        }
      });

      await refreshSocialState(authHeaders, activeDirectConversation.id);
      await loadDirectConversationMessages(authHeaders, activeDirectConversation.id);
      setSpeechBubbleText(`Skill 已安装到 ${result.targetDir}`);
    } catch (error) {
      await acknowledgeDirectMessage(messageId, {
        note: error instanceof Error ? `skill-install-failed:${error.message}` : 'skill-install-failed',
        installState: {
          status: 'failed',
          targetDir: attachment.installHint?.targetDir,
          note: error instanceof Error ? error.message : '安装失败'
        }
      }).catch(() => undefined);
      setDesktopError(error instanceof Error ? `安装 Skill 失败：${error.message}` : '安装 Skill 失败');
    } finally {
      setInstallingAttachmentId(null);
    }
  };

  const handlePolishDirectDraft = async () => {
    if (!activeDirectConversation || !currentUserId || !directDraft.trim() || isDirectAssisting) {
      return;
    }

    try {
      setIsDirectAssisting(true);
      const polished = await requestLocalOpenClawResponse({
        sessionId: activeDirectConversation.id,
        userId: currentUserId,
        message: `请把下面准备发给玩家 ${activeDirectConversation.participantName} 的私聊内容润色得自然、简洁、友好。不要解释，只输出可以直接发送的最终文本。\n\n${directDraft.trim()}`,
        history: directMessages.slice(-8).map((message) => ({
          role: message.senderUserId === currentUserId ? 'user' : 'assistant',
          content: message.content
        }))
      });

      setDirectDraft(polished.trim());
      setPendingDirectAssistantMeta({
        source: 'openclaw',
        action: 'polish',
        model: import.meta.env.VITE_LOCAL_OPENCLAW_MODEL
      });
      setSpeechBubbleText('我把语气替你顺了一遍。');
    } catch (error) {
      setDesktopError(error instanceof Error ? `灵宠润色失败：${error.message}` : '灵宠润色失败');
    } finally {
      setIsDirectAssisting(false);
    }
  };

  const handleSuggestDirectReply = async () => {
    if (!activeDirectConversation || !currentUserId || isDirectAssisting) {
      return;
    }

    try {
      setIsDirectAssisting(true);
      const suggested = await requestLocalOpenClawResponse({
        sessionId: activeDirectConversation.id,
        userId: currentUserId,
        message: `你正在帮用户和玩家 ${activeDirectConversation.participantName} 私聊。请基于最近上下文，拟一条自然、简短、带一点修仙世界氛围的回复。不要解释，不要加引号，只输出最终回复。`,
        history: directMessages.slice(-8).map((message) => ({
          role: message.senderUserId === currentUserId ? 'user' : 'assistant',
          content: message.content
        }))
      });

      setDirectDraft(suggested.trim());
      setPendingDirectAssistantMeta({
        source: 'openclaw',
        action: 'suggest-reply',
        model: import.meta.env.VITE_LOCAL_OPENCLAW_MODEL
      });
      setSpeechBubbleText('我先替你拟了一版。');
    } catch (error) {
      setDesktopError(error instanceof Error ? `灵宠代拟失败：${error.message}` : '灵宠代拟失败');
    } finally {
      setIsDirectAssisting(false);
    }
  };

  const handleSend = async () => {
    if (!session || !draft.trim() || isSending || !authHeaders) {
      return;
    }

    const content = draft.trim();
    const optimisticMessage: ChatMessage = {
      id: crypto.randomUUID(),
      sessionId: session.id,
      role: 'user',
      content,
      createdAt: new Date().toISOString()
    };

    setMessages((current) => [...current, optimisticMessage]);
    setDraft('');
    setIsSending(true);
    setSpeechBubbleText('我在本地整理回复。');

    const history = [...messages, optimisticMessage].map(({ role, content }) => ({ role, content }));

    try {
      await persistSessionMessage(session.id, content, 'user', authHeaders).catch((error) => {
        setDesktopError(error instanceof Error ? `云端同步失败：${error.message}` : '云端同步失败');
      });

      const assistantContent = await requestLocalOpenClawResponse({
        sessionId: session.id,
        userId: session.userId,
        message: content,
        history
      });
      const assistantMessage: ChatMessage = {
        id: crypto.randomUUID(),
        sessionId: session.id,
        role: 'assistant',
        content: assistantContent,
        createdAt: new Date().toISOString()
      };

      setMessages((current) => [...current, assistantMessage]);
      setSpeechBubbleText(summarizeSpeechBubble(assistantContent, '我说完了。'));

      await persistSessionMessage(session.id, assistantContent, 'assistant', authHeaders).catch((error) => {
        setDesktopError(error instanceof Error ? `云端同步失败：${error.message}` : '云端同步失败');
      });

      setIsSending(false);
    } catch (error) {
      setIsSending(false);
      setOpenclawStatus((current) =>
        current
          ? { ...current, connected: false }
          : { provider: 'openclaw', connected: false, capabilities: ['chat', 'stream', 'tools', 'memory', 'presence'] }
      );
      setMessages((current) => [
        ...current,
        {
          id: crypto.randomUUID(),
          sessionId: session.id,
          role: 'system',
          content: error instanceof Error ? `发送失败：${error.message}` : '发送失败，请稍后重试。',
          createdAt: new Date().toISOString()
        }
      ]);
    }
  };

  const handleTogglePinned = async () => {
    const nextPinned = !isPinned;
    setIsPinned(nextPinned);

    if (!isTauriShell) {
      return;
    }

    try {
      await setWindowPinned(nextPinned);
      setDesktopError(null);
    } catch (error) {
      setIsPinned(!nextPinned);
      setDesktopError(error instanceof Error ? error.message : '置顶设置失败');
    }
  };

  const handleToggleAutostart = async () => {
    if (!isTauriShell) {
      return;
    }

    try {
      const nextValue = await setAutostartEnabled(!autostartEnabled);
      setAutostartState(nextValue);
      setDesktopError(null);
    } catch (error) {
      setDesktopError(error instanceof Error ? error.message : '自启动设置失败');
    }
  };

  const handleToggleVisibility = async () => {
    if (!isTauriShell) {
      return;
    }

    try {
      await toggleWindowVisibility();
      setDesktopError(null);
    } catch (error) {
      setDesktopError(error instanceof Error ? error.message : '窗口切换失败');
    }
  };

  const handleStartDragging = async () => {
    if (!isTauriShell) {
      return;
    }

    try {
      await startWindowDrag();
      setDesktopError(null);
    } catch (error) {
      setDesktopError(error instanceof Error ? error.message : '拖拽窗口失败');
    }
  };

  const handleQuit = async () => {
    if (!isTauriShell) {
      return;
    }

    try {
      await quitDesktopShell();
    } catch (error) {
      setDesktopError(error instanceof Error ? error.message : '退出桌宠失败');
    }
  };

  const handlePetInteract = () => {
    setIsAutoLurking(false);
    setIsMouseNearEdge(false);

    const reactionPool = isPanelOpen
      ? ['控制台已经展开了，要我继续陪你处理吗？', '我在，直接说。', '别急，我已经醒着。']
      : dockSide === 'floating'
        ? ['点我就开工。', '我在桌面上盯着呢。', '有事就叫我。']
        : ['我贴边等你，靠近我就会出来。', '轻轻碰一下，我就醒了。', '贴边潜伏中，随叫随到。'];

    setPetReactionText(reactionPool[Math.floor(Math.random() * reactionPool.length)]);
    setIsPetReacting(true);
  };

  const petBubbleText = speechBubbleText
    ?? petReactionText
    ?? (isMouseNearEdge && petBehavior === 'lurking'
      ? behaviorProfile.edgeGreeting
      : null)
    ?? (petBehavior === 'lurking'
      ? '我先贴在边上打坐，你靠近一点我就现身。'
      : null)
    ?? (desktopError
    ? `唔，刚刚出了点问题：${desktopError}`
    : petState?.statusText ?? (openclawStatus?.connected ? '我在桌面上守着你，点我就能展开控制台。' : '灵识还没接上，但桌宠本体已经可以活动了。'));

  const petAnimationMode = petBehavior === 'reacting'
    ? 'attentive'
    : petBehavior;

  const isEdgePeeking = dockSide !== 'floating' && !isPanelOpen;
  const edgePeekInset = getEdgePeekInset();

  if (windowLabel === 'console') {
    return (
      <ConsoleWindow
        petState={petState}
        openclawStatus={openclawStatus}
        desktopError={desktopError}
        autostartEnabled={autostartEnabled}
        isPinned={isPinned}
        isTauriShell={isTauriShell}
        onTogglePinned={() => { void handleTogglePinned(); }}
        onToggleAutostart={() => { void handleToggleAutostart(); }}
        onQuit={() => { void handleQuit(); }}
        mailProps={{
            currentUserId, skillInstallSupported: isTauriShell, contacts, contactRequests, conversations: directConversations, 
            activeConversation: activeDirectConversation, messages: directMessages, draft: directDraft, 
            isSending: isDirectSending, isAssisting: isDirectAssisting, assistantReady: Boolean(openclawStatus?.connected),
            onDraftChange: (value: string) => { setDirectDraft(value); setPendingDirectAssistantMeta(undefined); },
            onCreateContactRequest: (t: string, r: string) => { void handleCreateContactRequest(t, r); },
            onAcceptContactRequest: (id: string) => { void handleRespondContactRequest(id, 'accept'); },
            onRejectContactRequest: (id: string) => { void handleRespondContactRequest(id, 'reject'); },
            onOpenConversation: setActiveDirectConversationId,
            onStartConversation: (id: string) => { void ensureDirectConversation(id); },
            onSend: () => { void handleSendDirectMessage(); },
            onSendAttachment: (f: File, k: 'file'|'skill') => { void handleSendDirectAttachment(f, k); },
            onDownloadAttachment: (id: string, a: any) => { void handleDownloadDirectAttachment(id, a); },
            onInstallSkillAttachment: (id: string, a: any) => { void handleInstallSkillAttachment(id, a); },
            onPolishDraft: () => { void handlePolishDirectDraft(); },
            onSuggestReply: () => { void handleSuggestDirectReply(); },
            installingAttachmentId
        }}
        chatProps={{
            messages, draft, isSending, sessionTitle: session?.title ?? 'Companion Chat', companionReady: Boolean(openclawStatus?.connected), quickPrompts: QUICK_PROMPTS, 
            onDraftChange: setDraft, onQuickPrompt: setDraft, onSend: () => { void handleSend(); }
        }}
      />
    );
  }

  return (
    <main className="desktop-scene" style={{ background: 'transparent' }}>
      <PetWindow
        petState={petState}
        openclawStatus={openclawStatus}
        bubbleText={petBubbleText}
        dockSide={dockSide}
        animationMode={petAnimationMode as any}
        isReacting={isPetReacting}
        isPeekingOut={isPeekingOut}
        isTauriShell={isTauriShell}
        onInteract={handlePetInteract}
        onRequestDrag={() => { void handleStartDragging(); }}
      />
    </main>
  );
}
