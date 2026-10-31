import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { createHash } from 'node:crypto';
import designs from './src/designs/registry.json';

export default defineConfig({
    plugins: [react(), {
        name: 'signature-design-manifest',
        generateBundle(_options, bundle) {
            const hash = createHash('sha256');
            for (const name of Object.keys(bundle).sort()) {
                const output = bundle[name];
                hash.update(name);
                hash.update(output.type === 'chunk' ? output.code : output.source);
            }
            hash.update(JSON.stringify(designs));
            this.emitFile({
                type: 'asset', fileName: 'designs.json',
                source: JSON.stringify({ version: hash.digest('hex'), designs: designs.filter(design => design.published) }, null, 2),
            });
        },
    }],
    build: { outDir: 'dist-render', rollupOptions: { input: 'render.html' } },
});
