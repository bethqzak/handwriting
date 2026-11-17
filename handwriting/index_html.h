// The dashboard web page. It lives in its own file because the Arduino
// build mangles JavaScript functions written inside a .ino file.

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Handwriting reader</title>
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/KaTeX/0.16.9/katex.min.css">
<script defer src="https://cdnjs.cloudflare.com/ajax/libs/KaTeX/0.16.9/katex.min.js"></script>
<script defer src="https://cdnjs.cloudflare.com/ajax/libs/KaTeX/0.16.9/contrib/auto-render.min.js"></script>
<script defer src="https://cdnjs.cloudflare.com/ajax/libs/qrcode-generator/1.4.4/qrcode.min.js"></script>
<style>
  :root{
    --bg:#f3efe6; --card:#fffdf8; --ink:#1f2933; --muted:#6b7280; --line:#d9d2c3;
    --accent:#2f5bea; --accent-ink:#fff; --rule:#cfd8ea; --margin:#e8a0a0; --ok:#1f9d6b; --warn:#d97706;
    --c1:#94a3b8; --c2:#2f5bea; --c3:#d97706; --c4:#1f9d6b;
  }
  @media (prefers-color-scheme: dark){
    :root{
      --bg:#14171c; --card:#1d2128; --ink:#e8eaed; --muted:#9aa3af; --line:#333a45;
      --accent:#7c9cff; --accent-ink:#0d1117; --rule:#2c3644; --margin:#7a4a4a; --ok:#4ade80; --warn:#fbbf24;
      --c1:#64748b; --c2:#7c9cff; --c3:#fbbf24; --c4:#4ade80;
    }
  }
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--ink);font-family:system-ui,-apple-system,"Segoe UI",sans-serif}
  .wrap{max-width:1100px;margin:0 auto;padding:20px 16px 40px}
  header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:16px;flex-wrap:wrap}
  h1{font-size:1.5rem;margin:0;font-family:Georgia,"Times New Roman",serif;letter-spacing:.01em}
  h1 span{color:var(--accent);font-style:italic}
  .pill{display:inline-flex;align-items:center;gap:8px;padding:6px 14px;border-radius:999px;border:1px solid var(--line);background:var(--card);font-size:.9rem}
  .dot{width:10px;height:10px;border-radius:50%;background:var(--muted)}
  body.reading .dot{background:var(--warn);animation:pulse .8s infinite}
  body.watching .dot{background:var(--ok);animation:pulse 1.6s infinite}
  @keyframes pulse{50%{opacity:.25}}
  .grid{display:grid;grid-template-columns:1fr;gap:16px}
  @media (min-width:860px){.grid{grid-template-columns:1fr 1fr;align-items:start}}
  .card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;box-shadow:0 1px 3px rgba(0,0,0,.06)}
  .label{font-size:.75rem;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);margin:0 0 8px}

  /* Camera viewfinder */
  .finder{position:relative;aspect-ratio:4/3;background:#0b0d10;border-radius:10px;overflow:hidden}
  .finder img{width:100%;height:100%;object-fit:cover;display:block}
  .corner{position:absolute;width:26px;height:26px;border:3px solid #fff;opacity:.85}
  .tl{top:10px;left:10px;border-right:0;border-bottom:0;border-radius:6px 0 0 0}
  .tr{top:10px;right:10px;border-left:0;border-bottom:0;border-radius:0 6px 0 0}
  .bl{bottom:10px;left:10px;border-right:0;border-top:0;border-radius:0 0 0 6px}
  .br{bottom:10px;right:10px;border-left:0;border-top:0;border-radius:0 0 6px 0}
  .scan{position:absolute;left:0;right:0;height:22%;top:-22%;display:none;
        background:linear-gradient(to bottom,transparent,rgba(124,156,255,.45));border-bottom:2px solid #9db4ff}
  body.reading .scan{display:block;animation:scan 1.4s linear infinite}
  @keyframes scan{to{top:100%}}
  #stat{color:#fff;background:rgba(180,30,30,.85);position:absolute;left:0;right:0;bottom:0;padding:6px 10px;font-size:.85rem}
  #stat:empty{display:none}

  button.main{width:100%;margin-top:12px;padding:14px;font-size:1.05rem;font-weight:600;border:0;border-radius:10px;
              background:var(--accent);color:var(--accent-ink);cursor:pointer;transition:transform .05s,opacity .2s}
  button.main:active{transform:scale(.99)}
  button.main:disabled{opacity:.55;cursor:wait}

  /* Live mode switch and meters */
  .switch{display:flex;align-items:center;gap:12px;margin-top:14px;cursor:pointer;user-select:none}
  .switch input{position:absolute;opacity:0}
  .track{flex:none;width:46px;height:26px;border-radius:999px;background:var(--line);position:relative;transition:background .2s}
  .track::after{content:"";position:absolute;top:3px;left:3px;width:20px;height:20px;border-radius:50%;background:#fff;transition:left .2s;box-shadow:0 1px 2px rgba(0,0,0,.3)}
  .switch input:checked + .track{background:var(--ok)}
  .switch input:checked + .track::after{left:23px}
  .switch input:focus-visible + .track{outline:2px solid var(--accent);outline-offset:2px}
  .switch small{display:block;color:var(--muted)}
  #meters{display:none;margin-top:12px}
  body.live #meters{display:block}
  .meter{display:grid;grid-template-columns:92px 1fr 54px;align-items:center;gap:8px;font-size:.85rem;margin-top:6px}
  .bar{position:relative;height:10px;border-radius:999px;background:var(--line);overflow:hidden}
  .fill{position:absolute;left:0;top:0;bottom:0;width:0;background:var(--accent);border-radius:999px;transition:width .3s}
  .fill.over{background:var(--ok)}
  .mark{position:absolute;top:-2px;bottom:-2px;width:2px;background:var(--ink);opacity:.6}
  .num{text-align:right;color:var(--muted);font-variant-numeric:tabular-nums}

  /* Transcription "paper" */
  .paperhead{display:flex;justify-content:space-between;align-items:center}
  button.ghost{border:1px solid var(--line);background:transparent;color:var(--ink);border-radius:8px;padding:5px 12px;font-size:.85rem;cursor:pointer}
  #out{margin:0;white-space:pre-wrap;word-break:break-word;min-height:15.3rem;padding:.2rem 14px 0 52px;border-radius:8px;
       font-family:Georgia,"Times New Roman",serif;font-size:1.15rem;line-height:1.9rem;
       background:
         linear-gradient(to right,transparent 40px,var(--margin) 40px,var(--margin) 41px,transparent 41px),
         repeating-linear-gradient(to bottom,transparent 0,transparent calc(1.9rem - 1px),var(--rule) calc(1.9rem - 1px),var(--rule) 1.9rem);
       border:1px solid var(--line)}
  #out.placeholder{color:var(--muted);font-style:italic}
  #out.fresh{animation:fresh .6s ease-out}
  @keyframes fresh{from{opacity:.2;transform:translateY(4px)}}

  /* Timing breakdown */
  #time{margin-top:12px;font-size:.85rem;color:var(--muted)}
  .tbar{display:flex;height:12px;border-radius:999px;overflow:hidden;margin:6px 0;background:var(--line)}
  .tbar div{height:100%}
  .legend{display:flex;flex-wrap:wrap;gap:4px 14px}
  .legend i{display:inline-block;width:9px;height:9px;border-radius:2px;margin-right:5px}
  /* QR code for opening the page on a phone */
  .share{display:flex;align-items:center;gap:14px;margin-top:16px}
  #qr{flex:none;width:132px;height:132px;border-radius:8px;background:#fff;image-rendering:pixelated}
  .share p{margin:0;font-size:.9rem;color:var(--muted)}
  .share a{color:var(--accent);font-weight:600;word-break:break-all}
  @media (prefers-reduced-motion: reduce){*{animation:none!important;transition:none!important}}
</style></head><body>
<div class="wrap">
  <header>
    <h1>Handwriting <span>reader</span></h1>
    <div class="pill"><span class="dot"></span><span id="pill">Ready</span></div>
  </header>
  <div class="grid">
    <section class="card">
      <p class="label">Camera</p>
      <div class="finder">
        <img id="cam" alt="camera preview">
        <span class="corner tl"></span><span class="corner tr"></span><span class="corner bl"></span><span class="corner br"></span>
        <div class="scan"></div>
        <div id="stat"></div>
      </div>
      <button id="go" class="main">Read handwriting</button>
      <label class="switch">
        <input type="checkbox" id="live"><span class="track"></span>
        <span>Live mode<small>Reads by itself when the writing changes</small></span>
      </label>
      <div id="meters">
        <div class="meter"><span>Movement</span><div class="bar"><div class="fill" id="mFill"></div><div class="mark" id="mMark"></div></div><span class="num" id="mNum"></span></div>
        <div class="meter"><span>New writing</span><div class="bar"><div class="fill" id="nFill"></div><div class="mark" id="nMark"></div></div><span class="num" id="nNum"></span></div>
      </div>
    </section>
    <section class="card">
      <div class="paperhead"><p class="label">Transcription</p><button id="copy" class="ghost">Copy</button></div>
      <pre id="out" class="placeholder">Point the camera at some handwriting and press the button.</pre>
      <div id="time"></div>
    </section>
  </div>
  <section class="card share">
    <canvas id="qr" role="img" aria-label="QR code for this page"></canvas>
    <p>Scan to open this page on a phone on the same WiFi.<br><a id="addr"></a></p>
  </section>
</div>
<script>
const $ = id => document.getElementById(id);
const cam = $('cam'), go = $('go'), out = $('out'), stat = $('stat'), live = $('live');
// Meter scales (percent) and the board's trigger levels, shown as marks.
const MOTION_MAX = 2, MOTION_LEVEL = 0.3, NEW_MAX = 1, NEW_LEVEL = 0.15;
const COLORS = ['var(--c1)', 'var(--c2)', 'var(--c3)', 'var(--c4)'];
let busy = false, seen = null, rawText = '';

$('mMark').style.left = (100 * MOTION_LEVEL / MOTION_MAX) + '%';
$('nMark').style.left = (100 * NEW_LEVEL / NEW_MAX) + '%';

function setStatus(){
  const reading = busy || document.body.classList.contains('livereading');
  document.body.classList.toggle('reading', reading);
  document.body.classList.toggle('watching', !reading && live.checked);
  document.body.classList.toggle('live', live.checked);
  $('pill').textContent = reading ? 'Reading…' : live.checked ? 'Watching for new writing' : 'Ready';
}

// Draws "Total 8.7 s: photo 0.5 s, connect 1.2 s, ..." as a proportional bar.
function showTiming(timing){
  const box = $('time');
  box.textContent = '';
  if (!timing) return;
  const parts = (timing.split(':')[1] || '').split(',').map(p => p.trim().match(/^(.*) ([\d.]+) s$/)).filter(Boolean);
  const total = timing.split(':')[0];
  if (!parts.length) { box.textContent = timing; return; }
  const sum = parts.reduce((a, p) => a + parseFloat(p[2]), 0) || 1;
  const head = document.createElement('div'); head.textContent = total.replace('Total', 'Took'); box.appendChild(head);
  const bar = document.createElement('div'); bar.className = 'tbar';
  const legend = document.createElement('div'); legend.className = 'legend';
  parts.forEach((p, i) => {
    const seg = document.createElement('div');
    seg.style.width = (100 * parseFloat(p[2]) / sum) + '%'; seg.style.background = COLORS[i % COLORS.length];
    bar.appendChild(seg);
    const item = document.createElement('span'); const sw = document.createElement('i');
    sw.style.background = COLORS[i % COLORS.length];
    item.appendChild(sw); item.appendChild(document.createTextNode(p[1] + ' ' + p[2] + ' s'));
    legend.appendChild(item);
  });
  box.appendChild(bar); box.appendChild(legend);
}

function show(text, timing){
  rawText = text;
  out.classList.remove('placeholder', 'fresh');
  out.textContent = text;
  void out.offsetWidth; out.classList.add('fresh');
  showTiming(timing);
  // Typeset the LaTeX; if KaTeX did not load, the raw text stays as is.
  if (window.renderMathInElement) renderMathInElement(out, {throwOnError: false, delimiters: [
    {left: '$$', right: '$$', display: true}, {left: '$', right: '$', display: false}]});
}

function meter(fill, num, value, max, level){
  const v = parseFloat(value) || 0;
  $(fill).style.width = Math.min(100, 100 * v / max) + '%';
  $(fill).classList.toggle('over', v > level);
  $(num).textContent = v.toFixed(2) + '%';
}

// Fetch preview frames one after another; if one fails, say why on the page.
async function previewLoop(){
  let wait = 200;
  if (!busy) {
    try {
      const r = await fetch('/preview.jpg?t=' + Date.now());
      if (!r.ok) throw new Error('HTTP ' + r.status + ' ' + await r.text());
      const b = await r.blob();
      if (!b.size) throw new Error('empty image');
      const old = cam.src;
      cam.src = URL.createObjectURL(b);
      if (old.startsWith('blob:')) URL.revokeObjectURL(old);
      stat.textContent = '';
    } catch (e) { stat.textContent = 'Preview problem: ' + e.message; wait = 1500; }
  }
  setTimeout(previewLoop, wait);
}

// In live mode, ask the board once a second whether it has read something new.
// The board cannot answer while it is reading, so a slow answer means "reading".
async function stateLoop(){
  if (live.checked && !busy) {
    const slow = setTimeout(() => { document.body.classList.add('livereading'); setStatus(); }, 1500);
    try {
      const s = await (await fetch('/state')).json();
      if (!s.live) live.checked = false;
      meter('mFill', 'mNum', s.motion, MOTION_MAX, MOTION_LEVEL);
      meter('nFill', 'nNum', s.fresh, NEW_MAX, NEW_LEVEL);
      if (seen !== null && s.count !== seen) show(s.text, s.timing);
      seen = s.count;
    } catch (e) { /* board busy; try again */ }
    clearTimeout(slow);
    document.body.classList.remove('livereading');
    setStatus();
  }
  setTimeout(stateLoop, 1000);
}

live.onchange = async () => {
  seen = null;
  try { await fetch('/live?on=' + (live.checked ? 1 : 0)); }
  catch (e) { live.checked = false; }
  setStatus();
};

go.onclick = async () => {
  busy = true; go.disabled = true; setStatus();
  try {
    const r = await fetch('/ocr');
    show(await r.text(), r.headers.get('X-Timing'));
    seen = null;
  }
  catch (e) { show('Request failed: ' + e, ''); }
  busy = false; go.disabled = false; setStatus();
};

// The page is served over plain http, where the modern clipboard API is not
// available, so copy through a temporary text box.
$('copy').onclick = () => {
  if (!rawText) return;
  const t = document.createElement('textarea');
  t.value = rawText; document.body.appendChild(t); t.select();
  const ok = document.execCommand('copy');
  t.remove();
  $('copy').textContent = ok ? 'Copied' : 'Copy failed';
  setTimeout(() => { $('copy').textContent = 'Copy'; }, 1500);
};

// QR code for the board's named address. HOSTNAME comes from handwriting.ino.
const PAGE_URL = 'http://)HTML" HOSTNAME R"HTML(.local/';
$('addr').textContent = $('addr').href = PAGE_URL;
window.addEventListener('load', () => {
  if (!window.qrcode) return;  // library did not load (no internet): the link is still shown
  const q = qrcode(0, 'M'); q.addData(PAGE_URL); q.make();
  const n = q.getModuleCount(), margin = 4, c = $('qr');
  c.width = c.height = n + 2 * margin;  // one pixel per square; CSS scales it up
  const g = c.getContext('2d');
  g.fillStyle = '#fff'; g.fillRect(0, 0, c.width, c.height);
  g.fillStyle = '#000';
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) if (q.isDark(y, x)) g.fillRect(x + margin, y + margin, 1, 1);
});

previewLoop();
stateLoop();
</script></body></html>)HTML";
