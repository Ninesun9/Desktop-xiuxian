import type { OpenClawStatus, PetState } from '@desktop-companion/shared';

type PetStatusCardProps = {
  petState: PetState | null;
  openclawStatus: OpenClawStatus | null;
};

export function PetStatusCard(props: PetStatusCardProps) {
  const { petState, openclawStatus } = props;

  return (
    <>
      <section className="pet-card">
        <p className="eyebrow">Desktop Companion</p>
        <h1>桌面宠物伴侣</h1>
        <p className="status">{petState?.statusText ?? '正在等待 API...'}</p>
        <div className="metrics">
          <div>
            <span>情绪</span>
            <strong>{petState?.mood ?? '--'}</strong>
          </div>
          <div>
            <span>能量</span>
            <strong>{petState?.energy ?? '--'}</strong>
          </div>
          <div>
            <span>亲密度</span>
            <strong>{petState?.intimacy ?? '--'}</strong>
          </div>
        </div>
      </section>

      <section className="provider-card">
        <p className="eyebrow">OpenClaw</p>
        <h2>{openclawStatus?.connected ? '已连接' : '未连接'}</h2>
        <p>能力：{openclawStatus?.capabilities.join(' / ') ?? '等待检测'}</p>
      </section>
    </>
  );
}
