export type PetPersonalityProfileId = 'balanced' | 'clingy' | 'lazy' | 'alert';

export type PetBehaviorProfile = {
  roamDelayMin: number;
  roamDelayMax: number;
  roamChance: number;
  lurkChance: number;
  peekIntervalMin: number;
  peekIntervalMax: number;
  peekDuration: number;
  edgeGreeting: string;
};

type EdgeGreetings = {
  low: string;
  medium: string;
  high: string;
};

type PetPersonalityTuning = {
  label: string;
  roamDelayMinBase: number;
  roamDelayMinLowEnergyWeight: number;
  roamDelayMinLowIntimacyWeight: number;
  roamDelayMaxBase: number;
  roamDelayMaxLowEnergyWeight: number;
  roamDelayMaxLowIntimacyWeight: number;
  roamChanceBase: number;
  roamChanceEnergyWeight: number;
  roamChanceIntimacyWeight: number;
  lurkChanceBase: number;
  lurkChanceLowEnergyWeight: number;
  lurkChanceIntimacyWeight: number;
  peekIntervalMinBase: number;
  peekIntervalMinIntimacyWeight: number;
  peekIntervalMaxBase: number;
  peekIntervalMaxIntimacyWeight: number;
  peekDurationBase: number;
  peekDurationIntimacyWeight: number;
  edgeGreetings: EdgeGreetings;
};

const clampRatio = (value: number) => Math.max(0, Math.min(1, value));

export const PET_PERSONALITY_PROFILES: Record<PetPersonalityProfileId, PetPersonalityTuning> = {
  balanced: {
    label: '均衡型',
    roamDelayMinBase: 7000,
    roamDelayMinLowEnergyWeight: 4200,
    roamDelayMinLowIntimacyWeight: 1200,
    roamDelayMaxBase: 12000,
    roamDelayMaxLowEnergyWeight: 5200,
    roamDelayMaxLowIntimacyWeight: 1600,
    roamChanceBase: 0.24,
    roamChanceEnergyWeight: 0.34,
    roamChanceIntimacyWeight: 0.08,
    lurkChanceBase: 0.2,
    lurkChanceLowEnergyWeight: 0.45,
    lurkChanceIntimacyWeight: 0.18,
    peekIntervalMinBase: 2500,
    peekIntervalMinIntimacyWeight: 700,
    peekIntervalMaxBase: 4500,
    peekIntervalMaxIntimacyWeight: 900,
    peekDurationBase: 1150,
    peekDurationIntimacyWeight: 650,
    edgeGreetings: {
      low: '我听到动静，就先探出来看看。',
      medium: '你一靠近，我就看见你了。',
      high: '我感觉到你靠近了。'
    }
  },
  clingy: {
    label: '粘人型',
    roamDelayMinBase: 6200,
    roamDelayMinLowEnergyWeight: 2600,
    roamDelayMinLowIntimacyWeight: 600,
    roamDelayMaxBase: 9800,
    roamDelayMaxLowEnergyWeight: 3600,
    roamDelayMaxLowIntimacyWeight: 900,
    roamChanceBase: 0.32,
    roamChanceEnergyWeight: 0.3,
    roamChanceIntimacyWeight: 0.18,
    lurkChanceBase: 0.12,
    lurkChanceLowEnergyWeight: 0.28,
    lurkChanceIntimacyWeight: 0.08,
    peekIntervalMinBase: 2100,
    peekIntervalMinIntimacyWeight: 900,
    peekIntervalMaxBase: 3600,
    peekIntervalMaxIntimacyWeight: 1200,
    peekDurationBase: 1450,
    peekDurationIntimacyWeight: 850,
    edgeGreetings: {
      low: '我听见你了，就赶紧探头看看。',
      medium: '你是不是来找我了？',
      high: '你一靠近，我就忍不住出来。'
    }
  },
  lazy: {
    label: '慵懒型',
    roamDelayMinBase: 9200,
    roamDelayMinLowEnergyWeight: 5200,
    roamDelayMinLowIntimacyWeight: 1600,
    roamDelayMaxBase: 14600,
    roamDelayMaxLowEnergyWeight: 6600,
    roamDelayMaxLowIntimacyWeight: 1800,
    roamChanceBase: 0.16,
    roamChanceEnergyWeight: 0.22,
    roamChanceIntimacyWeight: 0.05,
    lurkChanceBase: 0.28,
    lurkChanceLowEnergyWeight: 0.5,
    lurkChanceIntimacyWeight: 0.16,
    peekIntervalMinBase: 3000,
    peekIntervalMinIntimacyWeight: 500,
    peekIntervalMaxBase: 5200,
    peekIntervalMaxIntimacyWeight: 700,
    peekDurationBase: 1050,
    peekDurationIntimacyWeight: 420,
    edgeGreetings: {
      low: '外面有点动静，我慢慢看看。',
      medium: '你靠近了，我就抬头看一眼。',
      high: '你来了啊，我还是会出来看你的。'
    }
  },
  alert: {
    label: '警觉型',
    roamDelayMinBase: 5600,
    roamDelayMinLowEnergyWeight: 3000,
    roamDelayMinLowIntimacyWeight: 800,
    roamDelayMaxBase: 8600,
    roamDelayMaxLowEnergyWeight: 4200,
    roamDelayMaxLowIntimacyWeight: 1000,
    roamChanceBase: 0.3,
    roamChanceEnergyWeight: 0.28,
    roamChanceIntimacyWeight: 0.1,
    lurkChanceBase: 0.24,
    lurkChanceLowEnergyWeight: 0.32,
    lurkChanceIntimacyWeight: 0.12,
    peekIntervalMinBase: 1800,
    peekIntervalMinIntimacyWeight: 500,
    peekIntervalMaxBase: 3000,
    peekIntervalMaxIntimacyWeight: 800,
    peekDurationBase: 1100,
    peekDurationIntimacyWeight: 500,
    edgeGreetings: {
      low: '我察觉到动静了。',
      medium: '你一靠近，我就注意到了。',
      high: '你刚靠近，我已经在看你了。'
    }
  }
};

export const DEFAULT_PET_PERSONALITY: PetPersonalityProfileId = 'balanced';

export const getPetBehaviorProfile = (
  profileId: PetPersonalityProfileId,
  energyRatio: number,
  intimacyRatio: number
): PetBehaviorProfile => {
  const profile = PET_PERSONALITY_PROFILES[profileId];
  const safeEnergy = clampRatio(energyRatio);
  const safeIntimacy = clampRatio(intimacyRatio);

  return {
    roamDelayMin: Math.round(
      profile.roamDelayMinBase +
        (1 - safeEnergy) * profile.roamDelayMinLowEnergyWeight +
        (1 - safeIntimacy) * profile.roamDelayMinLowIntimacyWeight
    ),
    roamDelayMax: Math.round(
      profile.roamDelayMaxBase +
        (1 - safeEnergy) * profile.roamDelayMaxLowEnergyWeight +
        (1 - safeIntimacy) * profile.roamDelayMaxLowIntimacyWeight
    ),
    roamChance:
      profile.roamChanceBase +
      safeEnergy * profile.roamChanceEnergyWeight +
      safeIntimacy * profile.roamChanceIntimacyWeight,
    lurkChance:
      profile.lurkChanceBase +
      (1 - safeEnergy) * profile.lurkChanceLowEnergyWeight +
      safeIntimacy * profile.lurkChanceIntimacyWeight,
    peekIntervalMin: Math.round(
      profile.peekIntervalMinBase - safeIntimacy * profile.peekIntervalMinIntimacyWeight
    ),
    peekIntervalMax: Math.round(
      profile.peekIntervalMaxBase - safeIntimacy * profile.peekIntervalMaxIntimacyWeight
    ),
    peekDuration: Math.round(
      profile.peekDurationBase + safeIntimacy * profile.peekDurationIntimacyWeight
    ),
    edgeGreeting:
      safeIntimacy >= 0.72
        ? profile.edgeGreetings.high
        : safeIntimacy >= 0.42
          ? profile.edgeGreetings.medium
          : profile.edgeGreetings.low
  };
};