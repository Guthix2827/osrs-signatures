import { chromium } from 'playwright';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { resolve, dirname, extname, sep } from 'node:path';
import { parseArgs } from 'node:util';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';

const origin = 'http://signature.local';
const mimeTypes = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
    '.ttf': 'font/ttf', '.png': 'image/png', '.json': 'application/json' };

// Serve trusted build files through Playwright's routing; no HTTP service is needed.
export async function openArtworkPage(bundleDirectory) {
    const bundle = resolve(bundleDirectory);
    const browser = await chromium.launch({
        headless: true,
        ...(process.env.CHROMIUM_EXECUTABLE_PATH ? { executablePath: process.env.CHROMIUM_EXECUTABLE_PATH } : {}),
    });
    try {
        const context = await browser.newContext({
            viewport: { width: 320, height: 125 }, deviceScaleFactor: 1,
            locale: 'en-US', colorScheme: 'light', reducedMotion: 'reduce',
            serviceWorkers: 'block',
        });
        await context.route('**/*', async route => {
            const url = new URL(route.request().url());
            if (url.origin !== origin) return route.abort();
            try {
                const path = resolve(bundle, '.' + decodeURIComponent(url.pathname));
                if (!path.startsWith(bundle + sep)) return route.abort();
                await route.fulfill({
                    status: 200, body: await readFile(path),
                    contentType: mimeTypes[extname(path)] ?? 'application/octet-stream',
                });
            } catch {
                await route.fulfill({ status: 404, body: 'Asset not found' });
            }
        });
        const page = await context.newPage();
        page.setDefaultTimeout(15000);
        await page.goto(origin + '/render.html', { waitUntil: 'load', timeout: 15000 });
        await page.waitForFunction(() => typeof window.renderSignature === 'function');
        return { browser, page };
    } catch (error) {
        await browser.close();
        throw error;
    }
}

export async function renderSignature({ input, bundleDirectory }) {
    const manifest = JSON.parse(await readFile(resolve(bundleDirectory, 'designs.json'), 'utf8'));
    if (!manifest.designs.some(design => design.id === (input.config.design ?? 'classic'))) {
        throw new Error('Design is not published in this render bundle.');
    }
    const { browser, page } = await openArtworkPage(bundleDirectory);
    const deadline = setTimeout(() => { console.error('Signature rendering timed out.'); void browser.close(); }, 45000);
    try {
        await page.evaluate(async payload => { await window.renderSignature(payload); }, input);
        const image = await page.locator('.signature').screenshot({
            type: 'png', scale: 'css', animations: 'disabled', omitBackground: true,
        });
        if (image.readUInt32BE(16) !== 320 || image.readUInt32BE(20) !== 125) {
            throw new Error('Unexpected PNG dimensions.');
        }
        return image;
    } finally {
        clearTimeout(deadline);
        await browser.close();
    }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
    try {
        const { values } = parseArgs({ options: {
            input: { type: 'string' }, output: { type: 'string' }, bundle: { type: 'string' },
        } });
        if (!values.bundle) throw new Error('Usage: node render.mjs --bundle ../frontend/dist-render < stats.json');
        let payload;
        if (values.input) payload = await readFile(values.input, 'utf8');
        else {
            const chunks = [];
            let length = 0;
            for await (const chunk of process.stdin) {
                length += chunk.length;
                if (length > 16384) throw new Error('Render input exceeds limit.');
                chunks.push(chunk);
            }
            payload = Buffer.concat(chunks).toString('utf8');
        }
        const image = await renderSignature({ input: JSON.parse(payload), bundleDirectory: values.bundle });
        if (values.output) {
            await mkdir(dirname(resolve(values.output)), { recursive: true });
            await writeFile(values.output, image);
        } else {
            process.stdout.write(createHash('sha256').update(image).digest('hex') + '\n');
            process.stdout.write(image);
        }
    } catch (error) {
        console.error('Signature renderer:', error.message);
        process.exitCode = 1;
    }
}
