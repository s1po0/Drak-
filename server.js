#!/usr/bin/env node
'use strict';

/*
 * Static host for the Unreal Engine 4 HTML5 output.
 *
 * Serves ./public over HTTP with the headers and MIME types a packaged UE4
 * WebAssembly build needs (application/wasm, cross-origin isolation for
 * SharedArrayBuffer / -sUSE_PTHREADS builds, no-store for the bootstrap HTML).
 */

const http = require('http');
const fs = require('fs');
const path = require('path');
const { URL } = require('url');

const PORT = Number(process.env.PORT || 3000);
const HOST = process.env.HOST || '0.0.0.0';
const ROOT = path.join(__dirname, 'public');

// Cross-origin isolation (COOP/COEP) is only needed by pthread builds that use
// SharedArrayBuffer. It is off by default because it can break embedding in
// preview/portal iframes; enable with CROSS_ORIGIN_ISOLATION=1.
const CROSS_ORIGIN_ISOLATION = process.env.CROSS_ORIGIN_ISOLATION === '1';

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.htm': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.wasm': 'application/wasm',
  '.data': 'application/octet-stream',
  '.mem': 'application/octet-stream',
  '.symbols': 'application/octet-stream',
  '.js.mem': 'application/octet-stream',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.gif': 'image/gif',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
  '.webp': 'image/webp',
  '.mp3': 'audio/mpeg',
  '.ogg': 'audio/ogg',
  '.wav': 'audio/wav',
  '.mp4': 'video/mp4',
  '.webm': 'video/webm',
  '.txt': 'text/plain; charset=utf-8',
  '.map': 'application/json; charset=utf-8',
  '.ttf': 'font/ttf',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2'
};

function baseHeaders(ext) {
  const headers = {
    'Content-Type': MIME[ext] || 'application/octet-stream',
    'Access-Control-Allow-Origin': '*',
    'Cross-Origin-Resource-Policy': 'cross-origin',
    'X-Content-Type-Options': 'nosniff'
  };

  if (CROSS_ORIGIN_ISOLATION) {
    // Required so pthread-enabled UE4 builds can use SharedArrayBuffer.
    headers['Cross-Origin-Opener-Policy'] = 'same-origin';
    headers['Cross-Origin-Embedder-Policy'] = 'require-corp';
  }
  if (ext === '.html' || ext === '.htm' || ext === '.js') {
    headers['Cache-Control'] = 'no-store';
  } else {
    headers['Cache-Control'] = 'public, max-age=0, must-revalidate';
  }
  return headers;
}

function safePath(urlPath) {
  const decoded = decodeURIComponent(urlPath.split('?')[0].split('#')[0]);
  const clean = path.normalize(decoded).replace(/^(\.\.[/\\])+/, '');
  const full = path.join(ROOT, clean);
  if (!full.startsWith(ROOT)) return null;
  return full;
}

function sendError(res, code, message) {
  const body = `<!doctype html><meta charset="utf-8"><title>${code}</title>` +
    `<body style="background:#101012;color:#e8e8ea;font:14px/1.5 system-ui,sans-serif;padding:40px">` +
    `<h1 style="font-size:18px;font-weight:600;margin:0 0 8px">${code}</h1><p style="opacity:.7;margin:0">${message}</p>`;
  res.writeHead(code, {
    'Content-Type': 'text/html; charset=utf-8',
    'Content-Length': Buffer.byteLength(body),
    'Cache-Control': 'no-store'
  });
  res.end(body);
}

function serveFile(req, res, filePath) {
  const ext = path.extname(filePath).toLowerCase();

  fs.stat(filePath, (err, stat) => {
    if (err || !stat.isFile()) {
      if (!err && stat.isDirectory()) {
        return serveFile(req, res, path.join(filePath, 'index.html'));
      }
      return sendError(res, 404, 'Not found: ' + req.url);
    }

    const headers = baseHeaders(ext);
    headers['Accept-Ranges'] = 'bytes';
    headers['Last-Modified'] = stat.mtime.toUTCString();

    const range = req.headers.range;
    if (range && /^bytes=\d*-\d*$/.test(range)) {
      const [startRaw, endRaw] = range.replace('bytes=', '').split('-');
      let start = startRaw === '' ? stat.size - Number(endRaw) : Number(startRaw);
      let end = startRaw === '' || endRaw === '' ? stat.size - 1 : Number(endRaw);
      if (Number.isNaN(start) || Number.isNaN(end) || start < 0 || start > end || start >= stat.size) {
        res.writeHead(416, { 'Content-Range': `bytes */${stat.size}` });
        return res.end();
      }
      end = Math.min(end, stat.size - 1);
      headers['Content-Range'] = `bytes ${start}-${end}/${stat.size}`;
      headers['Content-Length'] = end - start + 1;
      res.writeHead(206, headers);
      if (req.method === 'HEAD') return res.end();
      fs.createReadStream(filePath, { start, end }).pipe(res);
      return;
    }

    headers['Content-Length'] = stat.size;
    res.writeHead(200, headers);
    if (req.method === 'HEAD') return res.end();
    fs.createReadStream(filePath).pipe(res);
  });
}

const server = http.createServer((req, res) => {
  if (req.method !== 'GET' && req.method !== 'HEAD') {
    return sendError(res, 405, 'Method not allowed');
  }

  let pathname;
  try {
    pathname = new URL(req.url, 'http://localhost').pathname;
  } catch (_) {
    return sendError(res, 400, 'Bad request');
  }

  if (pathname === '/healthz') {
    res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8', 'Cache-Control': 'no-store' });
    return res.end('ok\n');
  }

  let filePath = safePath(pathname);
  if (!filePath) return sendError(res, 400, 'Bad request');
  if (pathname.endsWith('/')) filePath = path.join(filePath, 'index.html');

  serveFile(req, res, filePath);
});

server.listen(PORT, HOST, () => {
  console.log(`UE4 host serving ${ROOT}`);
  console.log(`http://${HOST}:${PORT}`);
  console.log(`cross-origin isolation: ${CROSS_ORIGIN_ISOLATION ? 'on' : 'off (set CROSS_ORIGIN_ISOLATION=1 for pthread builds)'}`);
});
