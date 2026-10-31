import { useState, type ComponentType, type FormEvent } from 'react';
import designs from '../designs/registry.json';
import { defaults, readConfig } from '../config';
import type { Background, CardConfig, GeneratedSignature, Pose, SignatureProps } from '../types';

type GeneratorMode = 'production' | 'development';

interface GeneratorPageProps {
    mode: GeneratorMode;
    SignatureComponent: ComponentType<SignatureProps>;
}

export default function GeneratorPage({ mode, SignatureComponent }: GeneratorPageProps) {
    const isDevelopmentPreview = mode === 'development';
    const [initialConfiguration] = useState(() => {
        const requested = readConfig() ?? defaults;
        const selectedDesign = designs.find(design => design.id === requested.design);
        const configuration = !isDevelopmentPreview && !selectedDesign?.published
            ? { ...requested, design: 'classic' } : requested;
        return isDevelopmentPreview ? configuration : { ...configuration, pose: 'none' as Pose };
    });
    const [form, setForm] = useState<CardConfig>(initialConfiguration);
    const [configuration, setConfiguration] = useState<CardConfig>(initialConfiguration);
    const [requestId, setRequestId] = useState(0);
    const [generatedSignature, setGeneratedSignature] = useState<GeneratedSignature | null>(null);
    const [validationError, setValidationError] = useState('');
    const [requestError, setRequestError] = useState('');
    const [notice, setNotice] = useState('');

    const previewUrl = new URL(location.pathname, location.origin);
    previewUrl.hash = new URLSearchParams({ tab: 'dev', card: JSON.stringify(configuration) }).toString();
    const shareUrl = isDevelopmentPreview ? previewUrl.href : generatedSignature?.shareUrl ?? '';
    const fieldPrefix = isDevelopmentPreview ? 'dev' : 'generator';

    function createSignature(event: FormEvent<HTMLFormElement>) {
        event.preventDefault();
        const name = form.name.trim();

        if (!/^[a-zA-Z0-9 _-]{1,12}$/.test(name)) {
            setValidationError('Enter a name of 1–12 letters, numbers, spaces, hyphens, or underscores.');
            return;
        }

        setConfiguration({ ...form, name });
        setRequestId(current => current + 1);
        setGeneratedSignature(null);
        setValidationError('');
        setRequestError('');
        setNotice('');
    }

    async function copyShareLink() {
        if (!shareUrl) return;

        try {
            await navigator.clipboard.writeText(shareUrl);
            setNotice(isDevelopmentPreview ? 'Dev preview link copied.' : 'Share link copied.');
        } catch {
            setNotice('Clipboard unavailable. Select the link and copy it manually.');
        }
    }

    return (
        <>
            <h1>{isDevelopmentPreview ? 'Signature development' : 'Create your signature'}</h1>
            <p className="subtitle">
                {isDevelopmentPreview
                    ? 'Editable HTML preview for refining the signature design.'
                    : 'Your Old School stats. Ready to share.'}
            </p>

            <form onSubmit={createSignature}>
                <div className="fields">
                    <label htmlFor={`${fieldPrefix}-name`}>
                        Character name
                        <input
                            id={`${fieldPrefix}-name`}
                            value={form.name}
                            maxLength={12}
                            required
                            autoComplete="off"
                            placeholder="Enter character name"
                            aria-invalid={!!validationError}
                            aria-describedby={validationError ? `${fieldPrefix}-name-error` : undefined}
                            onChange={event => setForm({ ...form, name: event.target.value })}
                        />
                    </label>
                    <label htmlFor={`${fieldPrefix}-design`}>
                        Design
                        <select
                            id={`${fieldPrefix}-design`}
                            value={form.design ?? 'classic'}
                            onChange={event => setForm({ ...form, design: event.target.value })}
                        >
                            {designs.filter(design => isDevelopmentPreview || design.published).map(design => (
                                <option key={design.id} value={design.id}>{design.label}</option>
                            ))}
                        </select>
                    </label>
                    <label htmlFor={`${fieldPrefix}-background`}>
                        Background
                        <select
                            id={`${fieldPrefix}-background`}
                            value={form.background}
                            onChange={event => setForm({ ...form, background: event.target.value as Background })}
                        >
                            <option value="blue">Blue crystal</option>
                            <option value="green">Green crystal</option>
                            <option value="red">Red crystal</option>
                        </select>
                    </label>
                    <label htmlFor={`${fieldPrefix}-pose`}>
                        Pose
                        <select
                            id={`${fieldPrefix}-pose`}
                            value={form.pose}
                            disabled={!isDevelopmentPreview}
                            onChange={event => setForm({ ...form, pose: event.target.value as Pose })}
                        >
                            <option value="none">No pose</option>
                            {isDevelopmentPreview && <option value="knight">Rune knight</option>}
                            {isDevelopmentPreview && <option value="wizard">Blue wizard</option>}
                        </select>
                    </label>
                </div>
                {validationError && (
                    <p className="error" id={`${fieldPrefix}-name-error`} role="alert">{validationError}</p>
                )}
                <button className="primary" type="submit">
                    {isDevelopmentPreview ? 'Update dev preview' : 'Create signature'}
                </button>
            </form>

            <section className="preview-section">
                <h2>Signature preview</h2>
                <p className="section-caption">Classic style. Always yours.</p>
                <div className="preview">
                    <SignatureComponent
                        config={configuration}
                        requestId={requestId}
                        onGenerated={setGeneratedSignature}
                        onError={setRequestError}
                    />
                </div>
                <p className="demo-note">
                    {isDevelopmentPreview
                        ? 'Editable preview · Initial sample stats · Hiscores on request'
                        : 'Server-generated PNG · 320 × 125px'}
                </p>
                {requestError && <p className="error" role="alert">{requestError}</p>}

                <label htmlFor={`${fieldPrefix}-share-link`} className="share-label">
                    {isDevelopmentPreview ? 'Dev preview link' : 'Share link'}
                </label>
                <div className="share-row">
                    <input
                        id={`${fieldPrefix}-share-link`}
                        readOnly
                        value={shareUrl}
                        placeholder="Create a signature to get its link"
                        aria-describedby={`${fieldPrefix}-share-help`}
                        onFocus={event => event.target.select()}
                    />
                    <button className="copy" disabled={!shareUrl} onClick={copyShareLink}>Copy link</button>
                </div>
                {!isDevelopmentPreview && generatedSignature?.downloadUrl && (
                    <p>
                        <a href={generatedSignature.downloadUrl}>Download image</a>

                    </p>
                )}
                <p className="share-help" id={`${fieldPrefix}-share-help`}>
                    {isDevelopmentPreview
                        ? 'This page link opens the Dev tab and is only available on the development server.'
                        : 'Share the live image link, or download a copy of the current image.'}
                </p>
                <p className="notice" role="status">{notice}</p>
            </section>
        </>
    );
}
