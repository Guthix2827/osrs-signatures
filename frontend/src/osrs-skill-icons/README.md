# OSRS skill icons

24 original-size PNG skill icons downloaded from images linked by https://oldschool.runescape.wiki/w/Skills on 27 October 2026. Includes Sailing. Each PNG was decoded and verified. See sources.json for source URLs, dimensions, and SHA-256 hashes.

Game artwork belongs to Jagex. This package does not grant an asset license or resolve permission to use or redistribute these assets. Do not include them under your code's open-source license. See https://legal.jagex.com/docs/policies/fan-content-policy.

## Planned backend asset pipeline

1. Fetch approved icons during initial setup or an explicit asset refresh, not per signature request.
2. Validate image format, dimensions, and bounded size; retain a known-good set when refresh fails.
3. Pack losslessly into one transparent PNG using fixed 32 × 32 cells and native-sized icons. Do not stretch differently sized icons.
4. Write a JSON manifest keyed by stable skill name/ID: x, y, width, height, and alignment offsets. Coordinates should describe the icon rectangle, not just the cell.
5. Publish the atlas and manifest as a versioned pair with content hashes, updating them together.
6. Use CSS background-position in the frontend and source-rectangle compositing in the C++ renderer. Preserve crisp pixels; avoid fractional scaling.

The sprite is an internal/editor asset. The final signature PNG already contains all rendered icons and is the only image needed by a forum viewer. Atlas generation and integration are not included in this asset download.
