# Drak-

Two Unreal projects live here:

- `Ironvow/` — **Unreal Engine 5.4** physics-driven medieval duelling game (C++ core, Blueprint
  facing). See `Ironvow/README.md` and `Ironvow/Docs/`.
- the rest of this repository — a **UE 4.24** HTML5 browser host that serves a packaged 4.24 build.

---

# UE 4.24 HTML5 host

Unreal Engine 4 browser host, targeting **UE 4.24** (the last release with
official HTML5 support). Serves a packaged UE4 HTML5 / WebAssembly build with
the loading screen, runtime bootstrap and HTTP headers the engine needs.

## Run

```bash
npm start        # http://localhost:3000  (PORT / HOST env vars supported)
```

No dependencies are required — the host is a single Node static server.

## Preview it now

`buildDir` ships as `demo/`, which loads `public/demo/UE4Game.js` — a small
placeholder payload that speaks the same Emscripten `Module` protocol as a
packaged game (progress hooks, `onRuntimeInitialized`, canvas render loop).
The host is therefore fully visible on first run: loading screen → animated
canvas.

## Add the real game

1. In UE 4.24: **File > Package Project > HTML5**.
2. Copy the package output into `public/build/`:
   `UE4Game.js`, `UE4Game.wasm`, `UE4Game.data`, `favicon.ico`.
3. Set `buildDir: 'build/'` in `public/index.html` and reload the page.

## Configuration

`public/index.html`, in `window.UE4_HOST_CONFIG`:

| key | default | meaning |
| --- | --- | --- |
| `buildDir` | `demo/` | folder (relative to the host page) holding the package files |
| `buildName` | `UE4Game` | base name of the packaged `.js` / `.wasm` / `.data` |
| `gameName` | `Unreal Engine 4` | shown on the loading screen and as the page title |
| `engineVersion` | `4.24` | engine version shown on the loading screen |
| `backgroundColor` | `#000000` | canvas clear colour behind the WebGL surface |
| `requireCrossOriginIsolation` | `false` | pre-flight check for `SharedArrayBuffer` (enable for pthread builds) |

The loader is built for the Emscripten 1.38-era runtime that 4.24 packages:
`locateFile` plus `memoryInitializerPrefixURL` resolve `UE4Game.data` (and the
legacy `UE4Game.js.mem`), `noExitRuntime` keeps the engine loop alive, and
`monitorRunDependencies` drives the progress bar.

## Server behaviour

- `application/wasm` for `.wasm`, `application/octet-stream` for `.data`, so
  streaming compilation works.
- `Cross-Origin-Opener-Policy: same-origin` + `Cross-Origin-Embedder-Policy:
  require-corp` so pthread-enabled builds get `SharedArrayBuffer`.
- Range requests, `HEAD`, no-store on HTML/JS, `/healthz` endpoint.
- Everything resolves from `public/`; path traversal is rejected.
