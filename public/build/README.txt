Put the packaged Unreal Engine 4.24 HTML5 output in this folder.

UE 4.24 > File > Package Project > HTML5 produces (names match
window.UE4_HOST_CONFIG.buildName in ../index.html, default "UE4Game"):

  UE4Game.js        engine/runtime bootstrap
  UE4Game.wasm      compiled engine
  UE4Game.data      cooked game content (memory initializer)
  favicon.ico       optional

Also supported, depending on how the package was configured:

  UE4Game.js.mem    legacy asm.js-style memory initializer instead of .data
  UE4Game.worker.js emitted when the build uses pthreads (serve via server.js
                    so SharedArrayBuffer and the COOP/COEP headers are present)

While this folder is empty the host runs public/demo/UE4Game.js instead
(window.UE4_HOST_CONFIG.buildDir is 'demo/'). After copying a real package,
set buildDir to 'build/'.

The host does not need anything else from the package: index.html, styles.css
and the loading screen in ../ are already provided by this repository.
