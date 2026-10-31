# Shared signature designs

The Dev tab and PNG export now render the same React component and CSS:

- frontend/src/components/SignatureArtwork.tsx: signature markup, icons and displayed values.
- frontend/src/signature.css: your uploaded signature/font rules, including the latest equal-width skill columns. This is now the place to edit signature styling.
- frontend/src/styles.css: site layout and controls only.
- frontend/src/designs/registry.json: design IDs, labels and publication status.
- frontend/src/render.tsx: standalone export entry, built separately from the public client.

The existing Dev tab stays development-only. The shared artwork is also compiled into the private render bundle; the public release client displays the backend PNG. It does not include the Dev component or alignment hook.

## Add a design

Add an entry to registry.json:

```json
{"id":"midnight","label":"Midnight","published":false}
```

Add scoped rules to signature.css, inheriting the classic layout as a starting point:

```css
.signature[data-design="midnight"] {
    background: #182238;
    border-color: #73809b;
}
.signature[data-design="midnight"] .sig-name {
    color: #f7d56d;
}
```

The design appears in the Dev selector. Update the preview to apply the selected form values. Refine its styles with Vite HMR. For different markup, branch inside SignatureArtwork using config.design; both Dev and export use that same branch. Keep the artwork at 320x125 and use scoped rules so variants do not change one another.

When ready, set published=true, then:

```bash
cd frontend
npm run build
npm run build:render
```

Restart the backend to load the new published-design manifest. No C++ compilation is needed for subsequent CSS/component/design changes. Old immutable snapshots are no longer created. The same public URL generates the updated artwork; Nginx can retain the previous PNG until its one-minute cache window expires.

The background selector still passes blue/green/red, but your current CSS sets a flat yellowgreen background for all three. That behavior is intentionally preserved. Scope backgrounds inside each design if you want the selector to vary them.

The alignment hook now uses layout offsets rather than a compositor translate, avoiding resampling at fractional page positions. The signature uses flex:none so a column wrapper cannot stretch its height to 320px. The initial Dev sample now shows its calculated combat level 123; 126 was hardcoded and inconsistent with its sample Prayer level.

## Installing this update

Copy frontend/src/, frontend/render.html, frontend/vite.render.config.ts, frontend/vite.config.ts, backend/ and renderer/ into the corresponding project directories. Keep your existing frontend fonts, skill icons and package.json. Merge frontend/package.changes.json into your package.json scripts. Renderer has its own package.json and lockfile; install it separately. Start with backend/README.md for commands.
