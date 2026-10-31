import { lazy, Suspense, useState } from 'react';
import ProdSignature from './components/ProdSignature';
import GeneratorPage from './pages/GeneratorPage';

type Tab = 'generator' | 'dev' | 'about';

// Vite removes this import and its full dependency graph from release builds.
const DevPage = import.meta.env.DEV ? lazy(() => import('./pages/DevPage')) : null;

function initialTab(): Tab {
    const requestedTab = new URLSearchParams(location.hash.slice(1)).get('tab');
    if (import.meta.env.DEV && requestedTab === 'dev') return 'dev';
    return 'generator';
}

export default function App() {
    const [activeTab, setActiveTab] = useState<Tab>(initialTab);
    const [devOpened, setDevOpened] = useState(activeTab === 'dev');

    function openDevTab() {
        setDevOpened(true);
        setActiveTab('dev');
    }

    return (
        <div className="page">
            <header>
                <a className="brand" href={location.pathname}>
                    <span aria-hidden="true" />OSRS Signatures
                </a>
                <nav aria-label="Main navigation">
                    <button
                        className={activeTab === 'generator' ? 'active' : ''}
                        aria-current={activeTab === 'generator' ? 'page' : undefined}
                        onClick={() => setActiveTab('generator')}
                    >Generator</button>
                    {import.meta.env.DEV && (
                        <button
                            className={activeTab === 'dev' ? 'active' : ''}
                            aria-current={activeTab === 'dev' ? 'page' : undefined}
                            onClick={openDevTab}
                        >Dev</button>
                    )}
                    <button
                        className={activeTab === 'about' ? 'active' : ''}
                        aria-current={activeTab === 'about' ? 'page' : undefined}
                        onClick={() => setActiveTab('about')}
                    >About</button>
                </nav>
            </header>
            <main>
                {/* Keep mounted so navigating between tabs preserves the generated image. */}
                <div hidden={activeTab !== 'generator'}>
                    <GeneratorPage mode="production" SignatureComponent={ProdSignature} />
                </div>
                {import.meta.env.DEV && DevPage && devOpened && (
                    <div hidden={activeTab !== 'dev'}>
                        <Suspense fallback={<p role="status">Loading dev preview…</p>}>
                            <DevPage />
                        </Suspense>
                    </div>
                )}
                <section className="about" hidden={activeTab !== 'about'}>
                    <h1>About OSRS Signatures</h1>
                    <p>Create and share a classic Old School RuneScape signature.</p>
                    <p>The generator displays a PNG rendered by the backend. The live link refreshes
                        its stats, while snapshot links preserve one generated image.</p>
                    <p>This independent project is not affiliated with Jagex.</p>
                    <button className="primary" onClick={() => setActiveTab('generator')}>
                        Back to generator
                    </button>
                </section>
            </main>
            <footer>Independent fan project. Not affiliated with Jagex.</footer>
        </div>
    );
}
