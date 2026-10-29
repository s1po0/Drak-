# Drak-

Unreal Engine 4 browser host. Serves a packaged UE4 HTML5 / WebAssembly build
with the loading screen, runtime bootstrap and HTTP headers the engine needs.

## Run

```bash
npm start        # http://localhost:3000  (PORT / HOST env vars supported)
```

No dependencies are required — the host is a single Node static server.

## Add the game

1. In the UE4 editor: **File > Package Project > HTML5** (or your engine fork's
   WebGL / WebAssembly target).
2. Copy the package output into `public/build/`:
   `UE4Game.js`, `UE4Game.wasm`, `UE4Game.data`, `favicon.ico`.
3. Reload the page.

## Configuration

`public/index.html`, in `window.UE4_HOST_CONFIG`:

| key | default | meaning |
| --- | --- | --- |
| `buildDir` | `build/` | folder (relative to the host page) holding the package files |
| `buildName` | `UE4Game` | base name of the packaged `.js` / `.wasm` / `.data` |
| `gameName` | `Unreal Engine 4` | shown on the loading screen and as the page title |
| `backgroundColor` | `#000000` | canvas clear colour behind the WebGL surface |
| `requireCrossOriginIsolation` | `true` | pre-flight check for `SharedArrayBuffer` (pthread builds) |

## Server behaviour

- `application/wasm` for `.wasm`, `application/octet-stream` for `.data`, so
  streaming compilation works.
- `Cross-Origin-Opener-Policy: same-origin` + `Cross-Origin-Embedder-Policy:
  require-corp` so pthread-enabled builds get `SharedArrayBuffer`.
- Range requests, `HEAD`, no-store on HTML/JS, `/healthz` endpoint.
- Everything resolves from `public/`; path traversal is rejected.
