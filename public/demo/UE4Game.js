/*
 * Demo payload for the UE4 host.
 *
 * This is NOT Unreal Engine. It is a tiny stand-in "build" that speaks the same
 * Emscripten Module protocol as a packaged UE 4.24 HTML5 game, so the host page,
 * progress bar, splash hand-off and canvas can be exercised end to end.
 *
 * Replace public/build/UE4Game.js|wasm|data with a real package and point
 * buildDir back to 'build/' when you have one.
 */
(function () {
  'use strict';

  var M = window.Module || (window.Module = {});
  var canvas = M.canvas || document.getElementById('ue4-canvas');
  var ctx = canvas.getContext('2d', { alpha: false });

  var status = function (text) {
    if (typeof M.setStatus === 'function') M.setStatus(text);
  };

  /* --- boot sequence, mirrors the runtime dependency hooks of UE 4.24 --- */
  var remaining = 4;
  function boot() {
    if (remaining > 0) {
      if (typeof M.monitorRunDependencies === 'function') M.monitorRunDependencies(remaining);
      status('Loading demo payload (' + (5 - remaining) + '/5)');
      remaining -= 1;
      setTimeout(boot, 260);
      return;
    }
    status('Compiling shaders');
    setTimeout(function () {
      status('Starting Unreal Engine 4.24');
      if (typeof M.onRuntimeInitialized === 'function') M.onRuntimeInitialized();
      start();
    }, 320);
  }

  /* --- scene --- */
  var w = 0;
  var h = 0;
  var dpr = 1;
  var angleX = -0.55;
  var angleY = 0.7;
  var zoom = 1;
  var dragging = false;
  var lastX = 0;
  var lastY = 0;
  var t0 = performance.now();
  var raf = 0;

  var CUBE = [
    [-1, -1, -1], [1, -1, -1], [1, 1, -1], [-1, 1, -1],
    [-1, -1, 1], [1, -1, 1], [1, 1, 1], [-1, 1, 1]
  ];
  var EDGES = [
    [0, 1], [1, 2], [2, 3], [3, 0],
    [4, 5], [5, 6], [6, 7], [7, 4],
    [0, 4], [1, 5], [2, 6], [3, 7]
  ];

  function resize() {
    dpr = Math.min(window.devicePixelRatio || 1, 2);
    var rect = canvas.getBoundingClientRect();
    w = Math.max(1, Math.round((rect.width || window.innerWidth) * dpr));
    h = Math.max(1, Math.round((rect.height || window.innerHeight) * dpr));
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
    }
  }

  function project(p, ax, ay, scale) {
    var x = p[0];
    var y = p[1];
    var z = p[2];

    var cy = Math.cos(ay);
    var sy = Math.sin(ay);
    var x1 = x * cy - z * sy;
    var z1 = x * sy + z * cy;

    var cx = Math.cos(ax);
    var sx = Math.sin(ax);
    var y1 = y * cx - z1 * sx;
    var z2 = y * sx + z1 * cx;

    var dist = 5.2;
    var persp = dist / (dist + z2);
    return {
      x: w / 2 + x1 * scale * persp,
      y: h / 2 - y1 * scale * persp,
      d: z2
    };
  }

  function drawGrid(scale) {
    var size = 4;
    var step = 0.5;
    ctx.lineWidth = Math.max(1, dpr * 0.6);
    for (var i = -size; i <= size; i += step) {
      var alpha = 0.05 + 0.05 * (1 - Math.abs(i) / size);
      ctx.strokeStyle = 'rgba(14, 165, 233, ' + alpha.toFixed(3) + ')';
      var a = project([i, -1.35, -size], angleX, angleY, scale);
      var b = project([i, -1.35, size], angleX, angleY, scale);
      ctx.beginPath();
      ctx.moveTo(a.x, a.y);
      ctx.lineTo(b.x, b.y);
      ctx.stroke();

      var c = project([-size, -1.35, i], angleX, angleY, scale);
      var d = project([size, -1.35, i], angleX, angleY, scale);
      ctx.beginPath();
      ctx.moveTo(c.x, c.y);
      ctx.lineTo(d.x, d.y);
      ctx.stroke();
    }
  }

  function drawCube(scale, elapsed) {
    var bob = Math.sin(elapsed * 0.0011) * 0.12;
    var points = CUBE.map(function (v) {
      return project([v[0], v[1] + bob, v[2]], angleX, angleY, scale);
    });

    ctx.lineWidth = Math.max(1.4, dpr * 1.6);
    ctx.strokeStyle = 'rgba(14, 165, 233, 0.95)';
    ctx.shadowColor = 'rgba(14, 165, 233, 0.55)';
    ctx.shadowBlur = 14 * dpr;

    EDGES.forEach(function (e) {
      var a = points[e[0]];
      var b = points[e[1]];
      var near = 1 - Math.min(1, Math.max(0, (a.d + b.d + 3) / 6));
      ctx.globalAlpha = 0.35 + 0.65 * near;
      ctx.beginPath();
      ctx.moveTo(a.x, a.y);
      ctx.lineTo(b.x, b.y);
      ctx.stroke();
    });

    ctx.globalAlpha = 1;
    ctx.shadowBlur = 0;
    points.forEach(function (p) {
      ctx.fillStyle = 'rgba(186, 230, 253, 0.9)';
      ctx.beginPath();
      ctx.arc(p.x, p.y, 2.2 * dpr, 0, Math.PI * 2);
      ctx.fill();
    });
  }

  function drawHud(elapsed) {
    var pad = 22 * dpr;
    ctx.textBaseline = 'top';

    ctx.fillStyle = 'rgba(236, 236, 241, 0.92)';
    ctx.font = '600 ' + (13 * dpr) + 'px "Segoe UI", system-ui, sans-serif';
    ctx.fillText('UE 4.24 HOST — DEMO PAYLOAD', pad, pad);

    ctx.fillStyle = 'rgba(139, 139, 152, 0.85)';
    ctx.font = (11 * dpr) + 'px "Segoe UI", system-ui, sans-serif';
    ctx.fillText('Placeholder engine stub · drag to orbit · wheel to zoom', pad, pad + 20 * dpr);

    ctx.fillStyle = 'rgba(139, 139, 152, 0.6)';
    var fps = (1000 / Math.max(1, elapsed)).toFixed(0);
    ctx.fillText('canvas ' + canvas.width + '×' + canvas.height + ' · ~' + fps + ' fps',
      pad, h - pad - 14 * dpr);
  }

  var lastFrame = 0;

  function frame(now) {
    raf = requestAnimationFrame(frame);
    var dt = now - lastFrame;
    lastFrame = now;

    resize();
    if (!dragging) angleY += 0.0042 * Math.min(dt, 48);

    var scale = Math.min(w, h) * 0.115 * zoom;

    ctx.fillStyle = '#08080a';
    ctx.fillRect(0, 0, w, h);

    var glow = ctx.createRadialGradient(w / 2, h * 0.42, 0, w / 2, h * 0.42, Math.max(w, h) * 0.6);
    glow.addColorStop(0, 'rgba(14, 165, 233, 0.10)');
    glow.addColorStop(1, 'rgba(8, 8, 10, 0)');
    ctx.fillStyle = glow;
    ctx.fillRect(0, 0, w, h);

    drawGrid(scale);
    drawCube(scale, now - t0);
    drawHud(dt);
  }

  function start() {
    resize();
    window.addEventListener('resize', resize);

    canvas.addEventListener('pointerdown', function (e) {
      dragging = true;
      lastX = e.clientX;
      lastY = e.clientY;
      canvas.setPointerCapture(e.pointerId);
    });
    canvas.addEventListener('pointermove', function (e) {
      if (!dragging) return;
      angleY += (e.clientX - lastX) * 0.008;
      angleX += (e.clientY - lastY) * 0.008;
      angleX = Math.max(-1.35, Math.min(0.2, angleX));
      lastX = e.clientX;
      lastY = e.clientY;
    });
    var release = function (e) {
      dragging = false;
      if (e && e.pointerId !== undefined && canvas.hasPointerCapture(e.pointerId)) {
        canvas.releasePointerCapture(e.pointerId);
      }
    };
    canvas.addEventListener('pointerup', release);
    canvas.addEventListener('pointercancel', release);
    canvas.addEventListener('wheel', function (e) {
      e.preventDefault();
      zoom = Math.max(0.5, Math.min(2.4, zoom * (e.deltaY > 0 ? 0.92 : 1.08)));
    }, { passive: false });

    if (!raf) raf = requestAnimationFrame(frame);
  }

  boot();
})();
