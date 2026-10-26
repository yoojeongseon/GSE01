'use strict';
const $ = id => document.getElementById(id);
const labels = {active:'활성', paused:'일시 정지', warning:'주의', idle:'대기', moving:'이동', combat:'전투', dead:'사망', disabled:'비활성', unknown:'알 수 없음'};
let latest = {worlds:[], errors:[]};
function el(tag, text, cls) { const e = document.createElement(tag); if (text !== undefined) e.textContent = text; if (cls) e.className = cls; return e; }
function render() {
  const selected = $('world').value;
  const term = $('search').value.trim().toLocaleLowerCase();
  const worlds = latest.worlds.filter(w => !selected || w.worldId === selected);
  const allVillages = worlds.flatMap(w => w.villages);
  $('metrics').replaceChildren(...[
    ['수신 월드', worlds.length], ['마을 · 지역', allVillages.length],
    ['NPC', allVillages.reduce((n,v) => n + v.npcs.length, 0)],
    ['몹 (몬스터)', allVillages.reduce((n,v) => n + v.monsterCount, 0)]
  ].map(([name,count]) => {const card=el('div', undefined,'metric');card.append(el('span',name),el('strong',count.toLocaleString()));return card;}));
  $('villages').replaceChildren(); $('npcs').replaceChildren();
  let npcCount = 0;
  for (const w of worlds) for (const v of w.villages) {
    const villageMatch = v.name.toLocaleLowerCase().includes(term);
    const npcs = v.npcs.filter(n => villageMatch || n.name.toLocaleLowerCase().includes(term));
    if (!villageMatch && !npcs.length) continue;
    const card = el('article',undefined,'village');
    card.append(el('h3',v.name),el('div',`${w.name} · ${w.worldId}`,'source'),
      el('span',w.stale ? '수신 중단 · 마지막 상태' : labels[v.status],`badge${w.stale || v.status === 'warning' ? ' bad' : ''}`));
    const counts=el('div',undefined,'counts');
    for (const [label,count] of [['NPC',v.npcs.length],['몹',v.monsterCount]]) {
      const item=el('div');item.append(el('span',label+' '),el('b',count.toLocaleString()));counts.append(item);
    }
    card.append(counts);$('villages').append(card);
    for (const n of npcs) {
      npcCount++;
      const row=el('tr');
      for (const text of [`${w.name} / ${v.name}`,`${n.name} (${n.id})`,labels[n.status],`${n.x.toFixed(1)}, ${n.y.toFixed(1)}`,w.stale ? '수신 중단' : '정상']) row.append(el('td',text));
      $('npcs').append(row);
    }
  }
  if (!$('villages').children.length) $('villages').append(el('div',latest.worlds.length ? '검색 조건에 맞는 지역이 없습니다.' : '게임의 JSON 메시지를 기다리고 있습니다. 새로 빌드한 게임을 실행하거나 /api/snapshot으로 메시지를 보내세요.','empty'));
  if (!npcCount) { const row=el('tr'),cell=el('td','표시할 NPC가 없습니다.');cell.colSpan=5;row.append(cell);$('npcs').append(row); }
  $('error').textContent = latest.errors.map(e => `${e.file}: ${e.error}`).join('\n');
}
async function poll() {
  try {
    const response = await fetch('/api/worlds', {cache:'no-store',signal:AbortSignal.timeout(4000)});
    if (!response.ok) throw new Error('HTTP '+response.status);
    latest = await response.json();
    const selected=$('world').value;
    $('world').replaceChildren(new Option('전체 월드',''), ...latest.worlds.map(w => new Option(`${w.name} · ${w.worldId}`,w.worldId)));
    $('world').value=latest.worlds.some(w=>w.worldId===selected) ? selected : '';
    $('connection').textContent='수신기 연결됨';$('connection').className='';
    const received = latest.worlds.length ? Math.max(...latest.worlds.map(w=>w.receivedAt)) : null;
    $('updated').textContent=received ? '최근 메시지 '+new Date(received).toLocaleString('ko-KR') : '메시지 대기 중';
    render();
  } catch (e) {
    $('connection').textContent='수신기 연결 끊김';$('connection').className='bad';
    latest.worlds.forEach(w => {w.stale=true;});render();
    $('error').textContent='로컬 서버 연결을 확인하세요. 표시된 값은 마지막 수신 상태입니다.';
  } finally { setTimeout(poll,1000); }
}
$('world').addEventListener('change',render);$('search').addEventListener('input',render);poll();
