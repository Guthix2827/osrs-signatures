import { useEffect, useState } from 'react';
import { generateSignature } from '../api';
import type { GeneratedSignature, SignatureProps } from '../types';

type State =
    | { status: 'idle' | 'loading' }
    | { status: 'ready'; signature: GeneratedSignature }
    | { status: 'error'; message: string };

export default function ProdSignature({ config, requestId, onGenerated, onError }: SignatureProps) {
    const [state, setState] = useState<State>({ status: 'idle' });

    useEffect(() => {
        if (requestId === 0) return;
        const controller = new AbortController();
        let active = true;
        let timedOut = false;
        const timer = window.setTimeout(() => {
            timedOut = true;
            controller.abort();
        }, 120_000);
        setState({ status: 'loading' });

        async function generate() {
            try {
                const signature = await generateSignature(config, controller.signal);
                if (!active) return;
                setState({ status: 'ready', signature });
            } catch (error) {
                if (!active) return;
                const message = timedOut ? 'Signature generation timed out. Please try again.'
                    : error instanceof Error ? error.message : 'Could not generate the signature.';
                setState({ status: 'error', message });
                onError(message);
            } finally {
                clearTimeout(timer);
            }
        }
        void generate();
        return () => { active = false; clearTimeout(timer); controller.abort(); };
    }, [config, requestId, onError]);

    if (state.status === 'idle') return <p>Create a signature to load its image from the server.</p>;
    if (state.status === 'loading') return <p role="status">Generating signature…</p>;
    if (state.status === 'error') return <p role="alert">{state.message} Click Create signature to retry.</p>;
    if (state.status !== 'ready') return null;
    return <img
        key={state.signature.imageUrl}
        src={state.signature.imageUrl}
        width={320}
        height={125}
        alt={`${config.name}'s OSRS signature`}
        onLoad={() => onGenerated(state.signature)}
        onError={() => {
            const message = 'The generated signature image could not be loaded.';
            setState({ status: 'error', message });
            onError(message);
        }}
        style={{ display: 'block', flexShrink: 0 }}
    />;
}
