const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const {createMonitor, validate} = require('./server.cjs');
const snapshot = (worldId='world-a', sequence=1) => ({schemaVersion:1,worldId,name:'테스트 월드',sequence,
  villages:[{id:'camp',name:'마을',status:'active',monsterCount:3,
    npcs:[{id:'keeper',name:'불지기',status:'idle',x:1,y:2}]}]});
test('independent worlds, ordered full replacement, and stale detection',async t=>{
  let now=10000;
  const m=createMonitor({now:()=>now});
  m.ingest(snapshot());m.ingest(snapshot('world-b'));
  assert.equal(m.ingest(snapshot()),false);
  assert.equal(m.ingest(snapshot('world-a',0)),false);
  const next=snapshot('world-a',2);next.villages=[];m.ingest(next);
  assert.equal(m.worlds.get('world-a').villages.length,0);
  await new Promise(resolve=>m.server.listen(0,'127.0.0.1',resolve));
  t.after(()=>new Promise(resolve=>m.server.close(resolve)));
  const url=`http://127.0.0.1:${m.server.address().port}`;
  now+=6000;
  const data=await (await fetch(url+'/api/worlds')).json();
  assert.equal(data.worlds.length,2);assert.ok(data.worlds.every(w=>w.stale));
  const post=body=>fetch(url+'/api/snapshot',{method:'POST',headers:{'Content-Type':'application/json'},body});
  assert.equal((await post('{')).status,400);
  assert.equal((await post(JSON.stringify(snapshot('world-c')))).status,202);
  assert.equal((await post(JSON.stringify(snapshot('world-c')))).status,409);
  assert.equal((await post(' '.repeat(1024*1024+1))).status,413);
  assert.equal((await fetch(url+'/api/snapshot',{method:'POST',headers:{Origin:'https://example.com'}})).status,403);
  assert.equal((await fetch(url+'/')).status,200);
});
test('reject invalid counts, duplicate entities, unknown states and numeric IDs',()=>{
  for (const change of [s=>s.villages[0].monsterCount=-1,s=>s.worldId=22,
    s=>s.villages[0].npcs.push(s.villages[0].npcs[0]),s=>s.villages[0].status='garbage',
    s=>s.villages[0].npcs[0].x=Infinity]) {
    const s=snapshot();change(s);assert.throws(()=>validate(s));
  }
});
test('file collection does not refresh an old message; malformed update preserves state',async t=>{
  const dir=await fs.mkdtemp(path.join(os.tmpdir(),'gse-monitor-'));
  t.after(()=>fs.rm(dir,{recursive:true,force:true}));
  const file=path.join(dir,'world.json');
  await fs.writeFile(file,JSON.stringify(snapshot()));await fs.utimes(file,1,1);
  const m=createMonitor({directories:[dir],now:()=>100000});
  await m.scan();assert.equal(m.worlds.get('world-a').receivedAt,1000);
  await m.scan();assert.equal(m.worlds.get('world-a').receivedAt,1000);
  await fs.writeFile(file,'{');await m.scan();assert.equal(m.worlds.size,1);
});
