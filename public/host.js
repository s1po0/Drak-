/* Unreal Engine 4 HTML5 host bootstrap. */
(function () {
  'use strict';

  var cfg = window.UE4_HOST_CONFIG || {};
  var BUILD_DIR = cfg.buildDir || 'build/';
  var BUILD_NAME = cfg.buildName || 'UE4Game';
  var GAME_NAME = cfg.gameName || 'Unreal Engine 4';
  var REQUIRE_ISOLATION = cfg.requireCrossOriginIsolation !== false;

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

  document.title = GAME_NAME;
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

  function showError(title, message, stack) {
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
    splash.classList.add('is-hidden');
    canvas.classList.add('is-active');
    setTimeout(function () {
      try { canvas.focus(); } catch (e) { /* noop */ }
    }, 60);
  }

  function buildPath(file) {
    return BUILD_DIR.replace(/\/?$/, '/') + file;
  }

  function probe(url) {
    return fetch(url, { method: 'GET', headers: { Range: 'bytes=0-0' }, cache: 'no-store' })
      .then(function (res) { return res.ok || res.status === 206; })
      .catch(function () { return false; });
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
        setStatus('Starting ' + GAME_NAME);
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
    setDetail(jsUrl);

    probe(jsUrl).then(function (found) {
      if (!found) {
        showError('No Unreal Engine 4 build found',
          'This host is ready, but there is no packaged game to run yet.\n\n' +
          '1. In the editor: File > Package Project > HTML5 (or your WebGL/WASM target).\n' +
          '2. Copy the output (' + BUILD_NAME + '.js, ' + BUILD_NAME + '.wasm, ' +
          BUILD_NAME + '.data, favicon.ico) into public/build/.\n' +
          '3. Reload this page.\n\n' +
          'Expected file: ' + jsUrl);
        return;
      }

      setProgress(4);
      setStatus('Preparing ' + GAME_NAME);
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
})();
