// No npm dependencies. Local JSON receiver and game snapshot collector.
const http = require('node:http');
const fs = require('node:fs/promises');
const path = require('node:path');
const ROOT = path.resolve(__dirname, '..');
const LIMIT = 1024 * 1024;
const states = new Set(['active', 'paused', 'warning']);
const npcStates = new Set(['idle', 'moving', 'combat', 'dead', 'disabled', 'unknown']);

function validate(value) {
  const fail = () => { throw new Error('Invalid snapshot: see Monitoring/README.md'); };
  const string = x => typeof x === 'string' && x.length > 0 && x.length <= 160;
  const count = x => Number.isSafeInteger(x) && x >= 0;
  if (!value || value.schemaVersion !== 1 || !string(value.worldId) || !string(value.name) ||
      !count(value.sequence) || !Array.isArray(value.villages) || value.villages.length > 1000) fail();
  const villages = new Set();
  let total = 0;
  for (const v of value.villages) {
    if (!v || !string(v.id) || villages.has(v.id) || !string(v.name) || !states.has(v.status) ||
        !count(v.monsterCount) || !Array.isArray(v.npcs)) fail();
    villages.add(v.id);
    const ids = new Set();
    total += v.npcs.length;
    if (total > 10000) fail();
    for (const n of v.npcs) {
      if (!n || !string(n.id) || ids.has(n.id) || !string(n.name) || !npcStates.has(n.status) ||
          !Number.isFinite(n.x) || !Number.isFinite(n.y)) fail();
      ids.add(n.id);
    }
  }
  // Retain only the documented fields, never arbitrary producer metadata.
  return { schemaVersion: 1, worldId: value.worldId, name: value.name, sequence: value.sequence,
    villages: value.villages.map(v => ({id: v.id, name: v.name, status: v.status,
      monsterCount: v.monsterCount, npcs: v.npcs.map(n => ({id: n.id, name: n.name,
        status: n.status, x: n.x, y: n.y}))})) };
}

function createMonitor({ directories = [], now = Date.now } = {}) {
  const worlds = new Map();
  const errors = new Map();
  let scanning = false;
  function ingest(raw, receivedAt = now()) {
    const snapshot = validate(raw);
    const old = worlds.get(snapshot.worldId);
    if (old && snapshot.sequence <= old.sequence) return false;
    if (!old && worlds.size >= 256) throw new Error('World limit reached (256); restart collector');
    worlds.set(snapshot.worldId, {...snapshot, receivedAt});
    return true;
  }
  async function scan() {
    if (scanning) return;
    scanning = true;
    try {
      for (const directory of directories) {
        let files;
        try { files = await fs.readdir(directory); errors.delete(directory); }
        catch (e) { if (e.code !== 'ENOENT') errors.set(directory, e.message); continue; }
        for (const name of files.filter(n => n.endsWith('.json')).slice(0, 256)) {
          const filename = path.join(directory, name);
          try {
            const stat = await fs.stat(filename);
            if (stat.size > LIMIT) throw new Error('Snapshot exceeds 1 MiB');
            ingest(JSON.parse(await fs.readFile(filename, 'utf8')), Math.min(stat.mtimeMs, now()));
            errors.delete(filename);
          } catch (e) { errors.set(filename, e.message); }
        }
      }
    } finally { scanning = false; }
  }
  const server = http.createServer(async (req, res) => {
    const reply = (status, body, type = 'application/json; charset=utf-8') => {
      res.writeHead(status, {'Content-Type': type, 'Cache-Control': 'no-store',
        'X-Content-Type-Options': 'nosniff'});
      res.end(type.startsWith('application/json') ? JSON.stringify(body) : body);
    };
    try {
      // Prevent browser cross-origin writes and DNS rebinding to a loopback service.
      const expectedHost = `127.0.0.1:${server.address().port}`;
      if (req.headers.host !== expectedHost || (req.headers.origin && req.headers.origin !== `http://${expectedHost}`)) {
        return reply(403, {error: 'Use the displayed loopback URL'});
      }
      if (req.method === 'GET' && req.url === '/api/worlds') {
        return reply(200, {worlds: [...worlds.values()].map(w => ({...w, stale: now() - w.receivedAt > 5000})),
          errors: [...errors].map(([file, error]) => ({file, error})), serverTime: now()});
      }
      if (req.method === 'POST' && req.url === '/api/snapshot') {
        if (!/^application\/json(?:;|$)/i.test(req.headers['content-type'] || '')) return reply(415, {error:'Use application/json'});
        let size = 0;
        const chunks = [];
        for await (const chunk of req) {
          size += chunk.length;
          if (size > LIMIT) { reply(413, {error:'Maximum message size is 1 MiB'}); return; }
          chunks.push(chunk);
        }
        const accepted = ingest(JSON.parse(Buffer.concat(chunks).toString('utf8')));
        return reply(accepted ? 202 : 409, {accepted});
      }
      const staticFiles = {'/': ['index.html', 'text/html; charset=utf-8'],
        '/app.js': ['app.js', 'text/javascript; charset=utf-8'], '/style.css': ['style.css', 'text/css; charset=utf-8']};
      if (req.method === 'GET' && Object.hasOwn(staticFiles, req.url)) {
        const [file, type] = staticFiles[req.url];
        return reply(200, await fs.readFile(path.join(__dirname, file), 'utf8'), type);
      }
      reply(404, {error:'Not found'});
    } catch (e) { reply(400, {error:e.message}); }
  });
  server.requestTimeout = 10000;
  return {server, scan, ingest, worlds};
}

if (require.main === module) {
  const directories = ['Debug', 'Release', 'x64/Debug', 'x64/Release', 'SimpleGame/Debug',
    'SimpleGame/Release', 'SimpleGame/x64/Debug', 'SimpleGame/x64/Release'].map(p => path.join(ROOT, p, 'Monitoring'));
  directories.push(...process.argv.slice(2).map(p => path.resolve(p)));
  const monitor = createMonitor({directories});
  const port = Number(process.env.MONITOR_PORT || 8765);
  monitor.server.on('error', e => { console.error(e.message); process.exitCode = 1; });
  monitor.server.listen(port, '127.0.0.1', () => {
    console.log(`Game monitor: http://127.0.0.1:${monitor.server.address().port}`);
    monitor.scan();
    const timer = setInterval(() => monitor.scan(), 1000);
    monitor.server.on('close', () => clearInterval(timer));
  });
}
module.exports = {createMonitor, validate};
