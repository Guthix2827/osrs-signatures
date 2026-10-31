import designs from './designs/registry.json';
import type { CardConfig } from './types';

export const defaults: CardConfig = { design: 'classic', name: 'Zezima', background: 'blue', pose: 'none' };

export function readConfig(): CardConfig | null {
    try {
        const raw = new URLSearchParams(location.hash.slice(1)).get('card');
        if (!raw) return null;
        const c = JSON.parse(raw);
        if (!c || typeof c.name !== 'string' || !/^[a-zA-Z0-9 _-]{1,12}$/.test(c.name) || !['blue', 'green', 'red'].includes(c.background) || !['none', 'knight', 'wizard'].includes(c.pose)) return null;
        const design = c.design ?? 'classic';
        if (!designs.some(item => item.id === design)) return null;
        return {name: c.name, background: c.background, pose: c.pose, design};
    } catch {
        return null;
    }
}

