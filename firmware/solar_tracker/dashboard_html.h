// dashboard_html.h — the page served at http://solar-tracker.local/
// Self-contained: no internet needed, works on the fallback access point too.
#pragma once
#include <Arduino.h>

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Solar Tracker</title>
<style>
:root{--bg:#eef2f4;--card:#fff;--fg:#15222c;--mut:#5a6a75;--line:#cdd6dc;--acc:#d98a10;--ok:#2c8553;--info:#3a6ca6;--warn:#b8730a;--crit:#c0402c}
@media(prefers-color-scheme:dark){:root{--bg:#0e161c;--card:#15212a;--fg:#e2e9ee;--mut:#93a3ae;--line:#283742;--acc:#f2a93b;--ok:#4cc187;--info:#77a7de;--warn:#e9a63a;--crit:#ef6a55;color-scheme:dark}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.5 system-ui,sans-serif;padding:16px}
main{max-width:900px;margin:0 auto;display:grid;gap:12px}
header{display:flex;justify-content:space-between;align-items:center;gap:8px;flex-wrap:wrap}
h1{font-size:20px;margin:0;letter-spacing:.02em}
.chip{padding:3px 10px;border-radius:99px;font-size:12px;font-weight:600;color:#fff;background:var(--mut)}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px}
h2{font-size:12px;text-transform:uppercase;letter-spacing:.08em;color:var(--mut);margin:0 0 10px}
.tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:1px;background:var(--line);border:1px solid var(--line);border-radius:8px;overflow:hidden}
.tiles div{background:var(--card);padding:10px}
.tiles span{display:block;font-size:11px;color:var(--mut);text-transform:uppercase;letter-spacing:.06em}
.tiles b{font:500 20px ui-monospace,Menlo,monospace;font-variant-numeric:tabular-nums}
.tiles small{color:var(--mut);font:12px ui-monospace,monospace}
canvas{width:100%;height:120px;display:block}
ul{list-style:none;margin:0;padding:0;display:grid;gap:6px}
.f{border-left:3px solid var(--c);padding:5px 9px;border-radius:0 4px 4px 0;background:color-mix(in srgb,var(--c) 9%,transparent)}
.f b{font-size:11px;font-family:ui-monospace,monospace;color:var(--mut);margin-right:6px}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
button{font:inherit;font-weight:600;border:1px solid var(--line);background:var(--card);color:var(--fg);border-radius:6px;padding:8px 12px;cursor:pointer}
button.p{background:var(--acc);border-color:var(--acc);color:#1d1404}
button:disabled{opacity:.5}
input[type=range]{flex:1;min-width:160px;accent-color:var(--acc)}
label{display:flex;gap:6px;align-items:center}
#log{font:12px/1.7 ui-monospace,monospace;color:var(--mut)}
#msg{font-size:13px;color:var(--mut);min-height:1.5em}
</style></head><body><main>
<header><h1>Solar Tracker</h1><span class="chip" id="chip">connecting</span></header>
<section class="card"><div class="tiles">
<div><span>Voltage</span><b id="v">-</b></div>
<div><span>Current</span><b id="i">-</b></div>
<div><span>Power</span><b id="p">-</b> <small id="pe"></small></div>
<div><span>Panel temp</span><b id="t">-</b></div>
<div><span>Tilt</span><b id="a">-</b> <small id="st"></small></div>
<div><span>LDR E / W</span><b id="l">-</b></div>
<div><span>Cleanliness</span><b id="s">-</b></div>
<div><span>Battery</span><b id="b">-</b> <small id="bs"></small></div>
</div></section>
<section class="card"><h2>Power, last 5 minutes</h2><canvas id="cv"></canvas></section>
<section class="card"><h2>Diagnostics</h2><ul id="fd"></ul></section>
<section class="card"><h2>Controls</h2>
<div class="row"><button class="p" id="clean">Run cleaning cycle</button>
<button id="cal">Calibrate clean baseline</button>
<label><input type="checkbox" id="auto"> Auto-clean</label></div>
<div class="row" style="margin-top:12px"><button id="mode">Switch to manual</button>
<input type="range" id="tilt" min="-60" max="60" value="0" disabled><b id="tv">0&deg;</b></div>
<div id="msg"></div></section>
<section class="card"><h2>Events</h2><p style="margin:0 0 8px;font-size:12px;color:var(--mut)" id="sleepnote"></p><ul id="log"></ul></section>
</main><script>
const $=id=>document.getElementById(id),hist=[];let mode='auto';
const col={ok:'var(--ok)',info:'var(--info)',warn:'var(--warn)',crit:'var(--crit)'};
const esc=s=>String(s).replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
function fmt(v,d,u){return v==null?'-':v.toFixed(d)+' '+u}
async function post(url){try{const r=await fetch(url,{method:'POST'});const j=await r.json();$('msg').textContent=j.message;}catch(e){$('msg').textContent='Request failed. Is the tracker still on Wi-Fi?';}poll();}
function draw(){const c=$('cv'),r=c.getBoundingClientRect(),dpr=devicePixelRatio||1;c.width=r.width*dpr;c.height=r.height*dpr;
 const g=c.getContext('2d');g.scale(dpr,dpr);const w=r.width,h=r.height,cs=getComputedStyle(document.documentElement);
 const max=Math.max(1,...hist)*1.15;g.strokeStyle=cs.getPropertyValue('--line');g.lineWidth=1;g.fillStyle=cs.getPropertyValue('--mut');g.font='11px system-ui';
 for(let k=0;k<=2;k++){const y=h-16-(h-24)*k/2;g.beginPath();g.moveTo(34,y);g.lineTo(w,y);g.stroke();g.fillText((max*k/2).toFixed(1)+'W',0,y+4);}
 if(hist.length<2)return;const x=i=>34+(w-34)*i/149,y=v=>h-16-(h-24)*v/max;
 g.beginPath();hist.forEach((v,i)=>i?g.lineTo(x(i),y(v)):g.moveTo(x(i),y(v)));g.strokeStyle=cs.getPropertyValue('--acc');g.lineWidth=2;g.stroke();
 g.lineTo(x(hist.length-1),h-16);g.lineTo(x(0),h-16);g.closePath();g.globalAlpha=.12;g.fillStyle=cs.getPropertyValue('--acc');g.fill();g.globalAlpha=1;}
async function poll(){try{const d=await(await fetch('/api/status',{cache:'no-store'})).json();
 $('v').textContent=fmt(d.voltage,2,'V');$('i').textContent=d.current==null?'-':(d.current*1000).toFixed(0)+' mA';
 $('p').textContent=fmt(d.power,2,'W');$('pe').textContent=d.expected_power==null?'':'of '+d.expected_power.toFixed(2)+' W';
 $('t').textContent=fmt(d.panel_temp,1,'°C');$('a').textContent=(d.tilt<0?'E ':d.tilt>0?'W ':'')+Math.abs(d.tilt)+'°';
 $('st').textContent=d.wiper_busy?'cleaning':d.state.replace('_',' ');$('l').textContent=d.ldr_east+' / '+d.ldr_west;
 if(!d.battery_enabled){$('b').textContent='-';$('bs').textContent='bench mode';}else{$('b').textContent=fmt(d.battery_voltage,2,'V');$('bs').textContent=d.battery_percent<0?'not connected':(d.battery_full?'full':'~'+d.battery_percent+'%');}
 $('s').textContent=d.soiling_ratio==null?(d.calibrated?'-':'not calibrated'):Math.round(Math.min(1,d.soiling_ratio)*100)+'%';
 const ch=$('chip');ch.textContent=d.worst==='ok'?'healthy':d.worst;ch.style.background=col[d.worst];
 $('fd').innerHTML=d.findings.map(f=>'<li class="f" style="--c:'+col[f.sev]+'"><b>'+esc(f.code)+'</b>'+esc(f.msg)+'</li>').join('');
 $('log').innerHTML=d.log.map(l=>'<li>'+esc(l)+'</li>').join('');
 $('clean').disabled=d.wiper_busy;$('auto').checked=d.auto_clean;mode=d.mode;
 $('mode').textContent=mode==='auto'?'Switch to manual':'Back to auto tracking';$('tilt').disabled=mode!=='manual';
 if(mode!=='manual'){$('tilt').value=d.tilt;$('tv').textContent=d.tilt+'°';}
 $('sleepnote').textContent=d.night_sleep&&d.battery_enabled?'At night the tracker sleeps in 15-minute naps, so this page goes offline until sunrise.':'';
 hist.push(d.power||0);if(hist.length>150)hist.shift();draw();
}catch(e){const ch=$('chip');ch.textContent='offline';ch.style.background='var(--crit)';}}
$('clean').onclick=()=>post('/api/clean');$('cal').onclick=()=>post('/api/calibrate');
$('auto').onchange=e=>post('/api/autoclean?on='+(e.target.checked?1:0));
$('mode').onclick=()=>post('/api/mode?m='+(mode==='auto'?'manual':'auto'));
$('tilt').oninput=e=>$('tv').textContent=e.target.value+'°';
$('tilt').onchange=e=>post('/api/tilt?deg='+e.target.value);
addEventListener('resize',draw);poll();setInterval(poll,2000);
</script></body></html>)rawliteral";
