import { strict as assert } from 'node:assert';
import { readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { resolve, dirname } from 'node:path';
import { chromium } from 'playwright';
import { createServer } from '../../frontend/node_modules/vite/dist/node/index.js';
import { renderSignature } from '../render.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../../frontend');
const levels = {attack:99,strength:99,defence:99,ranged:99,prayer:80,hitpoints:99,agility:85,
    herblore:90,thieving:81,crafting:88,mining:87,smithing:82,fishing:92,cooking:99,
    firemaking:99,magic:99,fletching:91,woodcutting:94,runecraft:77,slayer:93,farming:96,
    construction:84,hunter:89,sailing:72};
const player = { name:'Guthix', fetchedAt:0, totalLevel:2173, totalXp:287400000, overallRank:null,
    skills:Object.fromEntries(Object.entries(levels).map(([name,level]) => [name,{level,xp:0,rank:null}])) };
const config = {name:'Guthix',design:'classic',background:'blue',pose:'none'};
const png = await renderSignature({ input:{config,player,combatLevel:123}, bundleDirectory:resolve(root,'dist-render') });
assert.equal(png.readUInt32BE(16),320);
assert.equal(png.readUInt32BE(20),125);

const vite = await createServer({ root, configFile:resolve(root,'vite.config.ts'),
    server:{host:'127.0.0.1',port:0}, clearScreen:false });
await vite.listen();
const browser = await chromium.launch({headless:true});
try {
    const page = await browser.newPage({viewport:{width:1000,height:800},deviceScaleFactor:1,locale:'en-US'});
    await page.route('**/api/players?*', route => route.fulfill({json:player}));
    const hash = new URLSearchParams({tab:'dev',card:JSON.stringify(config)});
    const port = vite.httpServer.address().port;
    await page.goto(`http://127.0.0.1:${port}/#${hash}`);
    await page.locator('button.primary:visible').click();
    const signature = page.locator('.signature[aria-label="Hiscores signature for Guthix"]');
    await signature.waitFor();
    await page.evaluate(async () => {
        await document.fonts.ready;
        await Promise.all(Array.from(document.images,img=>img.decode()));
        await new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)));
    });
    const devPng = await signature.screenshot({type:'png',scale:'css',animations:'disabled',omitBackground:true});
    await writeFile('/tmp/dev-artwork-qa.png', devPng);
    await writeFile('/tmp/export-artwork-qa.png', png);
    assert(png.equals(devPng),'Dev artwork and export PNG must match pixel for pixel in Chromium.');
    const release = await readFile(resolve(root,'dist/index.html'),'utf8');
    const asset = release.match(/src="([^"]+\.js)"/)[1];
    const releaseCode = await readFile(resolve(root,'dist',asset.slice(1)),'utf8');
    assert(!releaseCode.includes('loadingdone'),'Dev alignment hook must be excluded from release client.');
    console.log('PASS: dev/export pixel equality, loaded fonts/icons, 320x125 PNG and dev hook excluded from release client');
} finally {
    await browser.close();
    await vite.close();
}
