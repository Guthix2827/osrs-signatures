import type { CardConfig, GeneratedSignature } from './types';

export interface PlayerStats {
    name: string;
    fetchedAt: number;
    totalLevel: number;
    overallRank: number | null;
    skills: Record<string, { level: number | null; xp: number | null; rank: number | null }>;
}

export async function getPlayerStats(name: string, signal: AbortSignal): Promise<PlayerStats> {
    const response = await fetch(`/api/players?${new URLSearchParams({name})}`, {signal});
    const data = await response.json();
    if (!response.ok) throw new Error(data.error ?? 'Could not load player stats.');
    if (typeof data.totalLevel !== 'number' || typeof data.totalXp !== 'number' || !data.skills) {
        throw new Error('Invalid player stats response.');
    }
    return data;
}

export function combatLevel(stats: PlayerStats): number | null {
    const levels = ['attack','strength','defence','hitpoints','ranged','prayer','magic']
        .map(name => stats.skills[name]?.level);
    if (levels.some(level => level == null)) return null;
    const [attack,strength,defence,hp,ranged,prayer,magic] = levels as number[];
    return Math.floor((10 * (defence + hp + Math.floor(prayer / 2))
        + 13 * Math.max(attack + strength, Math.floor(ranged * 1.5), Math.floor(magic * 1.5))) / 40);
}

function wait(ms: number, signal: AbortSignal) {
    return new Promise<void>((resolve, reject) => {
        if (signal.aborted) { reject(new DOMException('Cancelled', 'AbortError')); return; }
        const abort = () => { clearTimeout(timer); reject(new DOMException('Cancelled', 'AbortError')); };
        const timer = setTimeout(() => { signal.removeEventListener('abort', abort); resolve(); }, ms);
        signal.addEventListener('abort', abort, {once:true});
    });
}

export async function generateSignature(config: CardConfig, signal: AbortSignal): Promise<GeneratedSignature> {
    let response = await fetch('/api/signatures', {
        method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify(config), signal,
    });
    let data = await response.json();
    if (!response.ok) throw new Error(data.error ?? 'Could not queue signature.');
    while (data.status === 'queued' || data.status === 'running') {
        const statusUrl = data.statusUrl ?? `/api/signature-jobs/${encodeURIComponent(data.jobId)}`;
        await wait(1000, signal);
        response = await fetch(statusUrl, {signal, cache:'no-store'});
        const next = await response.json();
        if (!response.ok) throw new Error(next.error ?? 'Could not check job status.');
        data = {...next, statusUrl};
    }
    if (data.status !== 'ready') throw new Error(data.error ?? 'Signature generation failed.');
    const url = (value: unknown) => {
        if (typeof value !== 'string' || !value) throw new Error('Missing image URL.');
        const parsed = new URL(value, location.origin);
        if (!['http:','https:'].includes(parsed.protocol)) throw new Error('Invalid image URL.');
        return parsed.href;
    };
    return {imageUrl:url(data.imageUrl), shareUrl:url(data.shareUrl),
        downloadUrl:url(data.downloadUrl)};
}
