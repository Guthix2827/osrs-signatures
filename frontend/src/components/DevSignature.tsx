import { useEffect, useState } from 'react';
import { getPlayerStats, type PlayerStats } from '../api';
import type { SignatureProps } from '../types';
import { usePixelAlignment } from '../hooks/usePixelAlignment';
import SignatureArtwork from './SignatureArtwork';

export default function DevSignature({ config, requestId, onError }: SignatureProps) {
    const [player, setPlayer] = useState<PlayerStats | null>(null);
    const [loading, setLoading] = useState(false);
    useEffect(() => {
        if (!requestId) return;
        const controller = new AbortController();
        let active = true;
        setLoading(true);
        setPlayer(null);
        getPlayerStats(config.name, controller.signal)
            .then(data => { if (active) setPlayer(data); })
            .catch(error => { if (active && error.name !== 'AbortError') onError(error.message); })
            .finally(() => { if (active) setLoading(false); });
        return () => { active = false; controller.abort(); };
    }, [config.name, requestId, onError]);
    const signatureRef = usePixelAlignment<HTMLDivElement>([config.name, config.background, config.pose, config.design]);
    return (
        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 10 }}>
            {loading && <span role="status">Loading hiscores…</span>}
            <SignatureArtwork ref={signatureRef} config={config} player={player} />
        </div>
    );
}
