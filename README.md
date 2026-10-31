# OSRS Signatures

An independent **Old School RuneScape (OSRS) signature generator**, developed separately from OSRS Market. It creates compact, shareable player-stat signatures as PNG images.

> **Project status:** Local implementation. Docker packaging, production deployment, GitHub publication, and cryptographic signing are **not yet implemented**.

## Technology stack

| Component | Technology | Status |
| --- | --- | --- |
| Frontend | React, TypeScript, Vite | Implemented |
| API | C++20, Crow | Implemented |
| Image rendering | Local Playwright renderer | Implemented |
| Reverse proxy examples | Nginx Proxy Manager snippets | Included; not automatically applied |
| Containers | Docker / Docker Compose | Planned |
| Production hosting | VPS and HTTPS | Planned |
| Persistent data | Storage solution to be selected | Planned |

## Current features

- Generate a **320 × 125 PNG** signature using a server-side renderer.
- Display the generated PNG in the generator interface.
- Share a signature through a **public, name-based URL**.
- Download a copy of the current generated PNG.
- Select published designs from a shared design registry.
- Create and edit draft designs in **Dev**, available only in the Vite development build. Dev edits the same component and CSS used for exported images.
- Check player stats at most once per minute per backend process.
- Reuse existing image bytes when the rendered artwork has not changed.
- Return image **ETags** for conditional requests and support Nginx proxy caching.
- Regenerate an image on direct public GET after a process restart.

### Current limitations

- Player stats, rendering jobs, and PNG data live in **backend process memory**.
- Generated files and snapshot history are **not persisted**.
- Restarting the backend clears in-memory state; public GET requests can regenerate signatures.
- Rate limits and caches are currently process-local; a multi-instance deployment would require a shared strategy.
- No Docker or production deployment configuration is claimed as complete.

## Repository documentation

- [`backend/README.md`](backend/README.md) — backend setup, API endpoints, and cache behavior.
- [`DESIGN-WORKFLOW.md`](DESIGN-WORKFLOW.md) — creating, previewing, and publishing designs.
- [`nginx/`](nginx/) — example Nginx Proxy Manager snippets. These do not modify server configuration automatically.

For local setup and endpoint details, follow the backend README rather than assuming commands or ports from this overview.

## Roadmap

The checkboxes below are **planned work**, not completed features. Priorities are suggested and can change as the project develops.

### Phase 1 — Docker and reproducible local environment (high priority)

- [ ] Create a multi-stage Dockerfile for the C++20/Crow API.
- [ ] Package the Playwright renderer with its required browser binaries, fonts, and system dependencies.
- [ ] Decide whether the renderer runs inside the API container or as a separate service.
- [ ] Add a production frontend build served by Nginx or another static server.
- [ ] Add `compose.yml` for frontend, API, and renderer as appropriate.
- [ ] Add `.env.example`, configuration documentation, and `.dockerignore` files.
- [ ] Configure internal service networking and expose only necessary ports.
- [ ] Add health checks, restart policies, log handling, and resource limits.
- [ ] Verify generated PNG output is visually consistent inside and outside containers, including RuneScape fonts and icons.
- [ ] Document local build, start, stop, logs, and rebuild commands after Compose filenames and service names are finalized.

### Phase 2 — VPS deployment (high priority)

- [ ] Choose production hostname and deployment target.
- [ ] Prepare a production Compose configuration and environment/secrets strategy.
- [ ] Configure DNS, HTTPS, and reverse proxy routing (e.g., Nginx Proxy Manager).
- [ ] Configure caching headers for public PNGs, respecting ETags and stat refresh rules.
- [ ] Restrict API and renderer services to internal networking where possible.
- [ ] Create a least-privilege deployment user and secure filesystem permissions.
- [ ] Configure firewall rules and ensure development tools/routes are not exposed publicly.
- [ ] Add a deployment procedure with image rebuild, health verification, and rollback steps.
- [ ] Test public signature URLs, downloads, cache revalidation, and regeneration after restart.
- [ ] Validate production performance and memory usage under concurrent rendering requests.

### Phase 3 — Reliability, storage, and performance (medium priority)

- [ ] Decide whether persistent PNGs or snapshot history are actually needed; retain on-demand regeneration if it is sufficient.
- [ ] If persistence is added, choose a storage backend, file lifecycle, retention, and cleanup policy.
- [ ] Consider a bounded render queue, concurrency limits, request deduplication, and timeouts.
- [ ] Consider shared cache/rate-limit state only if deploying multiple API instances.
- [ ] Define cache invalidation when a design changes, a player name changes, or stats refresh.
- [ ] Handle missing players, upstream failures, timeouts, and invalid requests gracefully.
- [ ] Add structured logs and metrics for API latency, render time, errors, cache hits, and queue depth.
- [ ] Add monitoring/alerts and a backup policy if persistent state is introduced.

### Phase 4 — Security and abuse prevention (high priority before public launch)

- [ ] Validate and normalize all player names and other request inputs.
- [ ] Rate-limit public image generation and protect against expensive repeated requests.
- [ ] Restrict renderer network access, filesystem access, privileges, and execution time.
- [ ] Prevent arbitrary URL navigation or injection into the rendering process.
- [ ] Apply reasonable request-size, queue-size, and resource limits.
- [ ] Review CORS, response headers, and public endpoint exposure.
- [ ] Scan container images and keep runtime dependencies patched.
- [ ] Review asset attribution, font licenses, and fan-content usage before publication.

### Phase 5 — Testing and CI/CD (medium priority)

- [ ] Add API unit and integration tests, including invalid input and caching behavior.
- [ ] Add renderer regression tests with reference PNGs or image-diff tolerances.
- [ ] Test the design registry and ensure draft designs never appear in production.
- [ ] Add a GitHub Actions workflow for frontend type checking/build and C++ compilation/tests.
- [ ] Add automated Docker image builds and vulnerability checks.
- [ ] Add staging deployment and smoke tests before production deployment.
- [ ] Add a controlled production deployment workflow with rollback instructions.
- [ ] Publish a tagged release process and changelog.

### Phase 6 — Product improvements (optional)

- [ ] Improve signature customization and add more published design themes.
- [ ] Add a live preview and clear loading/error feedback where needed.
- [ ] Improve accessibility and mobile usability of the generator interface.
- [ ] Consider versioned signature links so existing embeds do not unexpectedly change design.
- [ ] Consider a user-facing refresh status and documentation for third-party embedding.
- [ ] Evaluate historical snapshots only if there is a clear user need.
- [ ] Evaluate cryptographic signing only after defining a concrete authenticity or verification requirement.

## Suggested production architecture

```text
Browser / external signature embed
              |
            HTTPS
              |
       Reverse proxy
         /       \
  Frontend      Public API (C++ / Crow)
                      |
               Render service
                 (Playwright)
                      |
              In-memory cache
              (current behavior)
```

This is a **target architecture**, not a description of an existing deployment. A separate renderer service and persistent/shared storage are design decisions to validate before implementation.

## Release readiness checklist

- [ ] Docker builds succeed from a clean checkout.
- [ ] All required fonts, icons, and licenses are included in runtime packaging.
- [ ] Production configuration contains no committed secrets.
- [ ] Dev-only design editing is absent or inaccessible in production.
- [ ] Public signature URLs and image downloads work through HTTPS.
- [ ] Image output is stable across environments.
- [ ] Cache behavior and player-stat refresh intervals are verified.
- [ ] Rendering limits and input validation are tested.
- [ ] Health checks, logging, and a restart/rollback procedure are documented.
- [ ] Project license, third-party attributions, and GitHub documentation are reviewed.

## Attribution and disclaimer

The project uses RuneScape fonts and skill icons. Keep the relevant attribution and license files alongside the project and verify redistribution terms before public release.

**OSRS Signatures is an independent fan project and is not affiliated with, endorsed by, or operated by Jagex.**