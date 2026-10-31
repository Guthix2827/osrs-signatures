import { createRoot } from 'react-dom/client';
import { flushSync } from 'react-dom';
import SignatureArtwork from './components/SignatureArtwork';
import type { CardConfig } from './types';
import type { PlayerStats } from './api';
import './render.css';

interface RenderInput {
    config: CardConfig;
    player: PlayerStats;
    combatLevel: number | null;
}

declare global {
    interface Window {
        renderSignature: (input: RenderInput) => Promise<void>;
    }
}

const root = createRoot(document.getElementById('root')!);
window.renderSignature = async ({ config, player, combatLevel }) => {
    flushSync(() => root.render(
        <SignatureArtwork config={config} player={player} calculatedCombat={combatLevel} />
    ));
    await document.fonts.load('16px "RuneScape Plain"');
    await document.fonts.load('700 24px "RuneScape Bold"');
    await document.fonts.ready;
    await Promise.all(Array.from(document.images, image => image.decode()));
    const signature = document.querySelector<HTMLElement>('.signature')!;
    // Settle layout after loading the fonts; export at an integer origin and DPR 1.
    const box = signature.getBoundingClientRect();
    if (box.width !== 320 || box.height !== 125 || box.x !== 0 || box.y !== 0) {
        throw new Error('Signature must occupy 320 × 125 CSS pixels at the origin.');
    }
    if (!document.fonts.check('16px "RuneScape Plain"')
        || !document.fonts.check('700 24px "RuneScape Bold"')) {
        throw new Error('Signature fonts failed to load.');
    }
    await new Promise<void>(resolve => requestAnimationFrame(() => requestAnimationFrame(() => resolve())));
};
