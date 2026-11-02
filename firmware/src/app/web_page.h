// The macro editor page, served at / (see web.cpp). Plain HTML + JS, no
// external files: the Tab5 may be on a network with no internet.
#pragma once

#include <pgmspace.h>

namespace mt {

static const char kEditorPage[] PROGMEM = R"html(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Tab5 Multitool macros</title>
<style>
:root { --bg:#f4f5f7; --panel:#fff; --text:#16191d; --dim:#69737d; --line:#d9dde2; --accent:#2f7bd8; --bad:#b03a3a; --ok:#1f8a4c; }
@media (prefers-color-scheme: dark) { :root { --bg:#101418; --panel:#1a2027; --text:#e8ecf0; --dim:#8a95a0; --line:#2c333b; --accent:#4a90e2; --bad:#e06666; --ok:#4cc38a; } }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--text); font:15px/1.4 -apple-system, system-ui, sans-serif; }
header { display:flex; flex-wrap:wrap; gap:8px; align-items:center; padding:12px 16px; border-bottom:1px solid var(--line); background:var(--panel); }
header h1 { font-size:17px; margin:0 12px 0 0; }
button, select, input, textarea { font:inherit; color:inherit; }
button { background:var(--panel); border:1px solid var(--line); border-radius:7px; padding:6px 12px; cursor:pointer; }
button.primary { background:var(--accent); border-color:var(--accent); color:#fff; }
button:disabled { opacity:.5; cursor:default; }
#status { margin-left:auto; color:var(--dim); }
#status.bad { color:var(--bad); } #status.ok { color:var(--ok); }
main { display:grid; grid-template-columns:minmax(0,1fr) 340px; gap:16px; padding:16px; max-width:1400px; margin:0 auto; }
@media (max-width: 860px) { main { grid-template-columns:1fr; } }
.panel { background:var(--panel); border:1px solid var(--line); border-radius:10px; padding:14px; }
#tabs { display:flex; flex-wrap:wrap; gap:6px; margin-bottom:10px; }
#tabs button.on { background:var(--accent); border-color:var(--accent); color:#fff; }
.row { display:flex; flex-wrap:wrap; gap:8px; align-items:center; margin-bottom:10px; }
.row label { color:var(--dim); }
input[type=number] { width:60px; padding:4px 6px; border:1px solid var(--line); border-radius:6px; background:var(--bg); }
input[type=text], textarea { width:100%; padding:6px 8px; border:1px solid var(--line); border-radius:6px; background:var(--bg); }
textarea { font-family:ui-monospace, Menlo, monospace; font-size:13px; }
#grid { display:grid; gap:8px; aspect-ratio:1280/596; background:#101418; padding:8px; border-radius:10px; }
.slot { border-radius:9px; display:flex; align-items:center; justify-content:center; text-align:center; white-space:pre-line;
        font-weight:600; cursor:pointer; padding:4px; overflow:hidden; border:2px solid transparent; min-height:40px; }
.slot.empty { border:2px dashed #2c333b; color:#5a646e; font-weight:400; }
.slot.sel { border-color:#fff; box-shadow:0 0 0 2px var(--accent); }
.help { color:var(--dim); font-size:13px; }
.help code { font-family:ui-monospace, Menlo, monospace; font-size:12px; }
#editor[hidden], #raw[hidden] { display:none; }
h2 { font-size:15px; margin:0 0 10px; }
</style></head>
<body>
<header>
  <h1>Tab5 Multitool macros</h1>
  <button class="primary" id="save">Save to Tab5</button>
  <button id="download">Download</button>
  <button id="upload">Upload&hellip;</button>
  <input type="file" id="file" accept=".json,application/json" hidden>
  <button id="rawToggle">Raw JSON</button>
  <button id="reset">Reset to example</button>
  <span id="status">Loading&hellip;</span>
</header>
<main>
  <section class="panel">
    <div id="tabs"></div>
    <div class="row">
      <label>Page name</label><input type="text" id="pageName" style="width:180px">
      <label>Columns</label><input type="number" id="cols" min="1" max="8">
      <label>Rows</label><input type="number" id="rows" min="1" max="6">
      <button id="pageLeft" title="Move page left">&larr;</button>
      <button id="pageRight" title="Move page right">&rarr;</button>
      <button id="pageAdd">+ Page</button>
      <button id="pageDel">Delete page</button>
    </div>
    <div id="grid"></div>
    <p class="help">Click a button to edit it, or an empty slot to add one. Nothing changes on the Tab5 until you press <b>Save to Tab5</b>.</p>
  </section>
  <section class="panel">
    <div id="editor">
      <h2 id="slotTitle">Button</h2>
      <div class="row"><label>Label</label></div>
      <textarea id="label" rows="2" placeholder="Shown on the button; Enter makes a new line"></textarea>
      <div class="row" style="margin-top:10px"><label>Colour</label><input type="color" id="color"></div>
      <div class="row"><label>Steps, one per line</label></div>
      <textarea id="steps" rows="7" placeholder="cmd+c"></textarea>
      <div class="row" style="margin-top:10px">
        <button id="left" title="Swap with the slot before">&larr;</button>
        <button id="right" title="Swap with the slot after">&rarr;</button>
        <button id="clear">Clear slot</button>
      </div>
      <div class="help">
        <p><code>cmd+shift+t</code> keys together: <code>ctrl shift alt cmd</code>, <code>a</code>&ndash;<code>z</code>, <code>0</code>&ndash;<code>9</code>, <code>f1</code>&ndash;<code>f24</code>, <code>enter esc tab space backspace delete home end pageup pagedown up down left right</code>, punctuation, <code>kp0</code>&ndash;<code>kp9</code></p>
        <p><code>text:Hello\n</code> types text (<code>\n</code> = Enter, <code>\t</code> = Tab)</p>
        <p><code>media:volup</code> &nbsp;<code>voldown mute play next prev stop brightup brightdown</code></p>
        <p><code>delay:300</code> waits 300 ms &nbsp; <code>click</code>, <code>click:right</code></p>
      </div>
    </div>
    <div id="raw" hidden>
      <h2>Whole layout as JSON</h2>
      <textarea id="rawText" rows="24"></textarea>
      <div class="row" style="margin-top:10px"><button id="rawApply">Use this JSON</button></div>
    </div>
  </section>
</main>
<script>
let layout = { pages: [] }, page = 0, slot = -1, dirty = false;
const $ = id => document.getElementById(id);

function status(msg, kind) { const s = $('status'); s.textContent = msg; s.className = kind || ''; }
function setDirty(d) { dirty = d; if (d) status('Unsaved changes'); }

// Steps <-> the one-per-line text the editor shows.
const esc = s => s.replace(/\\/g, '\\\\').replace(/\n/g, '\\n').replace(/\t/g, '\\t');
const unesc = s => s.replace(/\\(.)/g, (m, c) => c === 'n' ? '\n' : c === 't' ? '\t' : c);
function stepsToLines(d) {
  if (d === undefined || d === null) return [];
  const list = Array.isArray(d) ? d : [d];
  return list.map(st => {
    if (typeof st === 'string') return st.startsWith('text:') ? 'text:' + esc(st.slice(5)) : st;
    if (st.keys) return st.keys;
    if (st.text !== undefined) return 'text:' + esc(st.text);
    if (st.media) return 'media:' + st.media;
    if (st.delay !== undefined) return 'delay:' + st.delay;
    if (st.click) return 'click:' + st.click;
    return JSON.stringify(st);
  });
}
function linesToDo(lines) {
  const steps = lines.map(l => l.trim()).filter(l => l).map(l => l.startsWith('text:') ? { text: unesc(l.slice(5)) } : l);
  return steps.length === 0 ? undefined : steps.length === 1 ? steps[0] : steps;
}
function normalize(l) {
  for (const p of l.pages) {
    p.cols = p.cols || 4; p.rows = p.rows || 3; p.buttons = p.buttons || [];
    for (const b of p.buttons) b._lines = stepsToLines(b.do);
  }
  return l;
}
function serialize() {
  const out = { pages: layout.pages.map(p => {
    const buttons = p.buttons.map(b => {
      const o = { label: b.label || '' };
      if (b.color) o.color = b.color;
      const d = linesToDo(b._lines || []);
      if (d !== undefined) o.do = d;
      return o;
    });
    while (buttons.length && buttons[buttons.length - 1].do === undefined && !buttons[buttons.length - 1].label) buttons.pop();
    return { name: p.name, cols: p.cols, rows: p.rows, buttons };
  }) };
  return JSON.stringify(out, null, 2);
}

function textColor(hex) {
  const v = parseInt((hex || '#2c333b').slice(1), 16);
  const r = v >> 16, g = (v >> 8) & 255, b = v & 255;
  return r * 299 + g * 587 + b * 114 > 150000 ? '#101418' : '#fff';
}

function render() {
  const tabs = $('tabs'); tabs.innerHTML = '';
  layout.pages.forEach((p, i) => {
    const b = document.createElement('button');
    b.textContent = p.name || ('Page ' + (i + 1));
    if (i === page) b.className = 'on';
    b.onclick = () => { page = i; slot = -1; render(); };
    tabs.appendChild(b);
  });
  const p = layout.pages[page];
  $('pageName').value = p ? p.name : '';
  $('cols').value = p ? p.cols : '';
  $('rows').value = p ? p.rows : '';
  const grid = $('grid'); grid.innerHTML = '';
  if (!p) return;
  grid.style.gridTemplateColumns = `repeat(${p.cols}, 1fr)`;
  grid.style.gridTemplateRows = `repeat(${p.rows}, 1fr)`;
  for (let i = 0; i < p.cols * p.rows; i++) {
    const b = p.buttons[i];
    const used = b && (b._lines || []).some(l => l.trim());
    const d = document.createElement('div');
    d.className = 'slot' + (used ? '' : ' empty') + (i === slot ? ' sel' : '');
    if (used) { d.style.background = b.color || '#2c333b'; d.style.color = textColor(b.color); d.textContent = b.label || '(no label)'; }
    else d.textContent = b && b.label ? b.label + ' (no steps)' : '+';
    d.onclick = () => { slot = i; render(); };
    grid.appendChild(d);
  }
  const ed = $('editor');
  if (slot < 0) { $('slotTitle').textContent = 'Pick a button'; ['label','steps','color','left','right','clear'].forEach(id => $(id).disabled = true); return; }
  ['label','steps','color','left','right','clear'].forEach(id => $(id).disabled = false);
  const b = p.buttons[slot] || {};
  $('slotTitle').textContent = 'Button ' + (slot + 1) + ' (row ' + (Math.floor(slot / p.cols) + 1) + ', column ' + (slot % p.cols + 1) + ')';
  $('label').value = b.label || '';
  $('color').value = b.color || '#2c333b';
  $('steps').value = (b._lines || []).join('\n');
}

function current() {
  const p = layout.pages[page];
  while (p.buttons.length <= slot) p.buttons.push({ label: '', _lines: [] });
  return p.buttons[slot];
}
$('label').oninput = e => { current().label = e.target.value; setDirty(true); render(); };
$('color').oninput = e => { current().color = e.target.value; setDirty(true); render(); };
$('steps').oninput = e => { current()._lines = e.target.value.split('\n'); setDirty(true); renderGridOnly(); };
function renderGridOnly() { const s = $('steps').selectionStart; render(); $('steps').focus(); $('steps').selectionStart = $('steps').selectionEnd = s; }
$('clear').onclick = () => { const p = layout.pages[page]; if (p.buttons[slot]) p.buttons[slot] = { label: '', _lines: [] }; setDirty(true); render(); };
function swap(a, b) {
  const p = layout.pages[page];
  if (b < 0 || b >= p.cols * p.rows) return;
  while (p.buttons.length <= Math.max(a, b)) p.buttons.push({ label: '', _lines: [] });
  [p.buttons[a], p.buttons[b]] = [p.buttons[b], p.buttons[a]];
  slot = b; setDirty(true); render();
}
$('left').onclick = () => swap(slot, slot - 1);
$('right').onclick = () => swap(slot, slot + 1);

$('pageName').oninput = e => { layout.pages[page].name = e.target.value; setDirty(true); renderTabsOnly(); };
function renderTabsOnly() { const v = $('pageName'), s = v.selectionStart; render(); v.focus(); v.selectionStart = v.selectionEnd = s; }
$('cols').onchange = e => { layout.pages[page].cols = Math.max(1, Math.min(8, +e.target.value || 4)); slot = -1; setDirty(true); render(); };
$('rows').onchange = e => { layout.pages[page].rows = Math.max(1, Math.min(6, +e.target.value || 3)); slot = -1; setDirty(true); render(); };
$('pageAdd').onclick = () => { layout.pages.push({ name: 'Page ' + (layout.pages.length + 1), cols: 4, rows: 3, buttons: [] }); page = layout.pages.length - 1; slot = -1; setDirty(true); render(); };
$('pageDel').onclick = () => {
  if (layout.pages.length < 2) { status('A layout needs at least one page', 'bad'); return; }
  if (!confirm('Delete page "' + layout.pages[page].name + '"?')) return;
  layout.pages.splice(page, 1); page = Math.max(0, page - 1); slot = -1; setDirty(true); render();
};
function movePage(d) {
  const j = page + d; if (j < 0 || j >= layout.pages.length) return;
  [layout.pages[page], layout.pages[j]] = [layout.pages[j], layout.pages[page]]; page = j; setDirty(true); render();
}
$('pageLeft').onclick = () => movePage(-1);
$('pageRight').onclick = () => movePage(1);

async function load(url) {
  const r = await fetch(url || '/api/layout', { cache: 'no-store' });
  layout = normalize(await r.json()); page = 0; slot = -1; render();
}
$('save').onclick = async () => {
  // Slots past a page's grid would be refused; drop them first.
  for (const p of layout.pages) p.buttons.length = Math.min(p.buttons.length, p.cols * p.rows);
  const body = serialize();
  status('Saving…');
  try {
    const r = await fetch('/api/layout', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body });
    const j = await r.json();
    if (r.ok) { dirty = false; status('Saved to the Tab5', 'ok'); }
    else status('Not saved: ' + j.error, 'bad');
  } catch (e) { status('Not saved: ' + e, 'bad'); }
};
$('download').onclick = () => {
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([serialize()], { type: 'application/json' }));
  a.download = 'tab5-macros.json'; a.click(); URL.revokeObjectURL(a.href);
};
$('upload').onclick = () => $('file').click();
$('file').onchange = async e => {
  const f = e.target.files[0]; if (!f) return;
  try { layout = normalize(JSON.parse(await f.text())); page = 0; slot = -1; setDirty(true); render(); status('Loaded ' + f.name + ': press Save to Tab5 to keep it'); }
  catch (err) { status('Not JSON: ' + err.message, 'bad'); }
  e.target.value = '';
};
$('reset').onclick = async () => {
  if (!confirm('Replace the Tab5\'s macros with the example layout?')) return;
  await fetch('/api/reset', { method: 'POST' }); await load(); dirty = false; status('Reset to the example', 'ok');
};
$('rawToggle').onclick = () => {
  const raw = $('raw'), ed = $('editor');
  if (raw.hidden) { $('rawText').value = serialize(); raw.hidden = false; ed.hidden = true; }
  else { raw.hidden = true; ed.hidden = false; }
};
$('rawApply').onclick = () => {
  try { layout = normalize(JSON.parse($('rawText').value)); page = 0; slot = -1; setDirty(true); render(); }
  catch (err) { status('Not JSON: ' + err.message, 'bad'); }
};
window.onbeforeunload = () => dirty ? 'unsaved' : undefined;
load().then(() => status('Loaded from the Tab5')).catch(e => status('Could not load: ' + e, 'bad'));
</script>
</body></html>
)html";

}  // namespace mt
