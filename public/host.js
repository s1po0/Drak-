/* Unreal Engine 4 HTML5 host bootstrap. */
(function () {
  'use strict';

  var cfg = window.UE4_HOST_CONFIG || {};
  var BUILD_DIR = cfg.buildDir || 'build/';
  var BUILD_NAME = cfg.buildName || 'UE4Game';
  var GAME_NAME = cfg.gameName || 'Unreal Engine 4';
  var ENGINE_VERSION = cfg.engineVersion || '4.24';
  // UE 4.24 HTML5 is packaged single-threaded by default, so it runs without
  // cross-origin isolation; the check is opt-in for pthread builds.
  var REQUIRE_ISOLATION = cfg.requireCrossOriginIsolation === true;
  var TITLE = GAME_NAME === 'Unreal Engine 4'
    ? 'Unreal Engine ' + ENGINE_VERSION
    : GAME_NAME;

  var canvas = document.getElementById('ue4-canvas');
  var splash = document.getElementById('ue4-splash');
  var statusEl = document.getElementById('ue4-status');
  var detailEl = document.getElementById('ue4-detail');
  var progressBar = document.getElementById('ue4-progressbar');
  var progressEl = document.getElementById('ue4-progress');
  var errorBox = document.getElementById('ue4-error');
  var errorTitle = document.getElementById('ue4-error-title');
  var errorMessage = document.getElementById('ue4-error-message');
  var errorStack = document.getElementById('ue4-error-stack');

  document.title = TITLE;
  if (canvas && cfg.backgroundColor) canvas.style.backgroundColor = cfg.backgroundColor;

  var depsDone = 0;
  var depsTotal = 1;
  var booted = false;

  function setProgress(pct) {
    if (typeof pct !== 'number' || !isFinite(pct)) return;
    pct = Math.max(0, Math.min(100, pct));
    progressEl.classList.remove('is-indeterminate');
    progressEl.style.width = pct.toFixed(1) + '%';
    progressBar.setAttribute('aria-valuenow', String(Math.round(pct)));
  }

  function setIndeterminate() {
    progressEl.classList.add('is-indeterminate');
    progressBar.removeAttribute('aria-valuenow');
  }

  function setStatus(text) {
    if (typeof text === 'string' && text.length) statusEl.textContent = text;
  }

  function setDetail(text) {
    detailEl.textContent = text || '';
  }

  var watchdog = 0;

  function clearWatchdog() {
    if (watchdog) {
      clearTimeout(watchdog);
      watchdog = 0;
    }
  }

  function showError(title, message, stack) {
    clearWatchdog();
    splash.classList.add('is-hidden');
    errorTitle.textContent = title || 'Host error';
    errorMessage.textContent = message || '';
    if (stack) {
      errorStack.hidden = false;
      errorStack.textContent = stack;
    } else {
      errorStack.hidden = true;
      errorStack.textContent = '';
    }
    errorBox.hidden = false;
  }

  function hideSplash() {
    clearWatchdog();
    splash.classList.add('is-hidden');
    canvas.classList.add('is-active');
    setTimeout(function () {
      try { canvas.focus(); } catch (e) { /* noop */ }
    }, 60);
  }

  function buildPath(file) {
    return BUILD_DIR.replace(/\/?$/, '/') + file;
  }

  // 'ok' | 'missing' | 'unknown'. Only a definitive 404/410 stops the boot:
  // proxies and file:// style hosts often answer oddly and must not be treated
  // as "no build".
  function probe(url) {
    return fetch(url, { method: 'GET', headers: { Range: 'bytes=0-0' }, cache: 'no-store' })
      .then(function (res) {
        if (res.ok || res.status === 206) return 'ok';
        if (res.status === 404 || res.status === 410) return 'missing';
        return 'unknown';
      })
      .catch(function () { return 'unknown'; });
  }

  function environmentError() {
    if (typeof WebAssembly !== 'object') {
      return 'This browser does not support WebAssembly, which the Unreal Engine 4 build requires.';
    }
    if (typeof WebGL2RenderingContext === 'undefined' && typeof WebGLRenderingContext === 'undefined') {
      return 'This browser does not expose WebGL, so the Unreal Engine 4 renderer cannot start.';
    }
    if (REQUIRE_ISOLATION && typeof SharedArrayBuffer === 'undefined' && self.crossOriginIsolated === false) {
      return 'This page is not cross-origin isolated, so SharedArrayBuffer is unavailable. ' +
        'The included server.js sends the required COOP/COEP headers — open the host through it ' +
        'instead of a plain static file server.';
    }
    return null;
  }

  function startLoader() {
    window.Module = {
      canvas: canvas,
      locateFile: function (path) {
        return /^https?:|^\//.test(path) ? path : buildPath(path);
      },
      // UE 4.24 ships an Emscripten 1.38-era runtime that still uses this to
      // find the preloaded payload (UE4Game.data, or UE4Game.js.mem for
      // asm.js-style packages). locateFile covers it on newer runtimes.
      memoryInitializerPrefixURL: BUILD_DIR.replace(/\/?$/, '/'),
      // The engine keeps its own loop alive after main().
      noExitRuntime: true,
      print: function (text) { console.log('[UE4] ' + text); },
      printErr: function (text) { console.warn('[UE4] ' + text); },
      setStatus: function (text) {
        if (!text) return;
        setStatus(text);
      },
      monitorRunDependencies: function (left) {
        if (typeof left === 'number') {
          depsTotal = Math.max(depsTotal, left + depsDone);
          depsDone += 1;
        }
        var pct = depsTotal > 0 ? (depsDone / depsTotal) * 80 : 0;
        setProgress(Math.max(5, Math.min(80, pct)));
      },
      onRuntimeInitialized: function () {
        booted = true;
        setProgress(100);
        setStatus('Starting ' + TITLE);
        setTimeout(hideSplash, 220);
      },
      onAbort: function (what) {
        showError('Engine aborted',
          'The Unreal Engine 4 runtime stopped during start-up.\n\n' +
          'Check the browser console for the full engine log.',
          what ? String(what) : '');
      }
    };

    var script = document.createElement('script');
    script.src = buildPath(BUILD_NAME + '.js');
    script.async = true;
    script.onerror = function () {
      showError('Failed to load engine',
        'Could not load ' + buildPath(BUILD_NAME + '.js') + '.\n\n' +
        'Copy the packaged HTML5 output into public/build/ and keep the file name ' +
        'in window.UE4_HOST_CONFIG.buildName in sync with it.');
    };
    document.body.appendChild(script);
  }

  function awaitBuild() {
    var jsUrl = buildPath(BUILD_NAME + '.js');
    setStatus('Looking for build');
    setDetail('UE ' + ENGINE_VERSION + ' HTML5 · ' + jsUrl);

    probe(jsUrl).then(function (state) {
      if (state === 'missing') {
        showError('No Unreal Engine 4.24 build found',
          'This host is ready, but there is no packaged game to run yet.\n\n' +
          '1. In UE 4.24: File > Package Project > HTML5.\n' +
          '2. Copy the output (' + BUILD_NAME + '.js, ' + BUILD_NAME + '.wasm, ' +
          BUILD_NAME + '.data, favicon.ico) into public/build/.\n' +
          '3. Reload this page.\n\n' +
          'Expected file: ' + jsUrl);
        return;
      }

      setProgress(4);
      setStatus('Preparing ' + TITLE);
      setDetail('');
      startLoader();
    });
  }

  document.getElementById('ue4-error-reload').addEventListener('click', function () {
    location.reload();
  });

  window.addEventListener('error', function (event) {
    if (!booted && event && event.message) {
      showError('Engine error', event.message, event.error && event.error.stack ? event.error.stack : '');
    }
  });

  window.addEventListener('unhandledrejection', function (event) {
    if (!booted && event && event.reason) {
      var reason = event.reason;
      showError('Engine error',
        reason && reason.message ? reason.message : String(reason),
        reason && reason.stack ? reason.stack : '');
    }
  });

  var envIssue = environmentError();
  if (envIssue) {
    setProgress(0);
    showError('Unsupported environment', envIssue);
    return;
  }

  setIndeterminate();
  awaitBuild();

  // If nothing reports ready the page still says what is happening instead of
  // sitting on a silent splash screen.
  watchdog = setTimeout(function () {
    if (!booted) {
      setDetail('Host is running. The payload has not reported ready yet - open the browser console for engine logs.');
    }
  }, 8000);
})();
