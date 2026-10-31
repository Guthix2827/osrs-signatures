# OSRS signature backend

Independent C++20/Crow API. Player stats, jobs and generated PNGs are kept in memory. The C++ worker sends JSON to a local Node/Playwright helper through stdin and receives PNG bytes plus their SHA-256 ETag through stdout. It does not write generated images, snapshots, job indexes or request JSON to disk. Chromium maintains its own temporary browser profile while running; that is separate from generated image storage.

## Local setup

Use Node 20.19+ and the existing frontend fonts/icons. Build the shared artwork before starting the backend:

```bash
cd frontend
npm install
npm run build
npm run build:render
cd ../renderer
npm ci
npm run install:browser
cd ../backend
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/osrs-signature-backend
```

If package.json has your custom font download scripts, preserve them and add only `"build:render": "tsc -b && vite build --config vite.render.config.ts"` to scripts. On Debian, Playwright can install missing system libraries with `npx playwright install-deps chromium` from renderer/. Native build dependencies remain build-essential, cmake, libasio-dev and libcurl4-openssl-dev. ImageMagick is needed only for the optional existing --prepare-assets sprite tool; it no longer renders signatures.

Run from backend/. Defaults: BIND_ADDRESS=127.0.0.1, PORT=8081, SIGNATURE_RENDER_BUNDLE=../frontend/dist-render, SIGNATURE_RENDERER_SCRIPT=../renderer/render.mjs, NODE_BINARY=node. CHROMIUM_EXECUTABLE_PATH can override Playwright's installed Chromium. DATA_DIR and REFRESH_SECONDS are no longer used. Existing data folders are ignored; this update does not delete them.

## Endpoints

- GET /api/players?name=Zezima: normalized stats; no combat level. Successful lookups and failed upstream attempts share a per-player 60-second cache across designs, spelling case and underscore/space aliases.
- POST /api/signatures with {"name":"Zezima","design":"classic","background":"blue","pose":"none"}: 202 with jobId/statusUrl, or 200 ready with imageUrl/shareUrl/downloadUrl.
- GET /api/signature-jobs/{id}: queued/running/ready/failed. Jobs are transient. After a restart, resubmit or request the public image URL directly.
- GET /signatures/Zezima.png?design=classic&background=blue: public PNG; cold requests generate it and wait. This URL works after restart without a previous POST or database row.
- Append &download=1 to get an attachment. The downloaded file is a fixed copy; the server keeps no snapshot history. /snapshots and /downloads routes have been removed.

Old random-ID URLs cannot be reconstructed without their deleted mapping. Generate a new name-based share link once when migrating. Names with spaces are percent encoded in the URL. The exact supplied name is retained for the displayed name; player lookups share a canonical cache key.

One worker coalesces concurrent image requests. Cached images are checked when requested, with no polling of inactive players. Ranks and fetch timestamps do not trigger rendering. Changed visible stats or artwork trigger a render; the SHA-256 hash of the final PNG controls the ETag. Equal bytes keep the same ETag. Missing combat stats display --.

The backend returns Cache-Control: public, max-age=0, s-maxage=N, must-revalidate, with N between 1 and 60 based on the cached stats' age. If-None-Match supports 304 responses, including weak validators and tag lists. Failures are not cacheable; retries are throttled for a minute. No stale success is silently substituted for a failed refresh.

Limits: 256 image entries, 256 KiB per PNG, 1000 player entries, 1000 retained jobs, 32 queued jobs. Images use LRU eviction; a later GET regenerates an evicted image. Fresh player entries are not evicted to bypass the lookup limit; excess new players receive 503. In-memory rate limits apply per process and reset on restart. Run one backend process for this local setup; multiple replicas would need a shared limiter/cache.

## Nginx Proxy Manager

The nginx/ directory has two configuration snippets. Merge http.conf into NPM's /data/nginx/custom/http.conf to define the cache zone. Create a custom /signatures/ location forwarded to the backend and paste signatures-location.conf into that location's Advanced configuration. Forward /api/ to the backend without caching. Reload NPM after validating nginx configuration. The cache directory must be writable by its worker user. These snippets are supplied for configuration; they have not been deployed to your server.

Nginx checks an expired entry with If-None-Match: 304 retains cached PNG bytes, while 200 replaces that URL's cached representation. Query parameters are included in the cache key, including design, background and download. Cache locking coalesces cold proxy requests. A cache hit may show the previous image for up to one minute; this is timed revalidation, not an active purge notification. If you later need immediate push invalidation, a separate purge mechanism is needed.

Nginx normally stores its proxy cache in files under the configured directory. The application stores generated images only in RAM. For a fully volatile proxy cache too, mount the Nginx cache directory on tmpfs. No server configuration was changed by this update.

Official references:
https://nginxproxymanager.com/advanced-config/
https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_cache_revalidate

## Tests

```bash
ctest --test-dir build --output-on-failure
TEST_SERVER_BINARY="$PWD/build/memory-service-tests" python3 tests/memory_integration.py
cd ../renderer
npm test
```

Build frontend release and render bundles before testing. Memory service tests use a deterministic injected clock and stats loader. HTTP integration uses that test executable with controlled fixture files; production never reads those fixtures. Renderer tests compare the Dev artwork and server PNG pixel for pixel in the same Chromium environment. Browser/OS rasterization can differ; use the same Chromium build for reproducible exports.
