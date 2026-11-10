// dashboard_html.h — the page served at http://solar-tracker.local/
// Self-contained: no internet needed, works on the ESP32's own hotspot.
#pragma once
#include <Arduino.h>

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Solar Tracker</title>
<style>
:root{--bg:#eef2f4;--card:#fff;--fg:#15222c;--mut:#5a6a75;--line:#cdd6dc;--acc:#d98a10;--ok:#2c8553;--crit:#c0402c}
@media(prefers-color-scheme:dark){:root{--bg:#0e161c;--card:#15212a;--fg:#e2e9ee;--mut:#93a3ae;--line:#283742;--acc:#f2a93b;--ok:#4cc187;--crit:#ef6a55;color-scheme:dark}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.5 system-ui,sans-serif;padding:16px}
main{max-width:900px;margin:0 auto;display:grid;gap:12px}
header{display:flex;justify-content:space-between;align-items:center;gap:8px;flex-wrap:wrap}
h1{font-size:20px;margin:0}
.chip{padding:3px 10px;border-radius:99px;font-size:12px;font-weight:600;color:#fff;background:var(--mut)}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px}
h2{font-size:12px;text-transform:uppercase;letter-spacing:.08em;color:var(--mut);margin:0 0 10px}
.tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:1px;background:var(--line);border:1px solid var(--line);border-radius:8px;overflow:hidden}
.tiles div{background:var(--card);padding:10px}
.tiles span{display:block;font-size:11px;color:var(--mut);text-transform:uppercase;letter-spacing:.06em}
.tiles b{font:500 19px ui-monospace,Menlo,monospace;font-variant-numeric:tabular-nums}
ul{list-style:none;margin:0;padding:0;display:grid;gap:6px}
li{border-left:3px solid var(--crit);padding:5px 9px;background:color-mix(in srgb,var(--crit) 9%,transparent);border-radius:0 4px 4px 0}
li b{font:11px ui-monospace,monospace;color:var(--mut);margin-right:6px}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
button,input{font:inherit;border:1px solid var(--line);background:var(--card);color:var(--fg);border-radius:6px;padding:8px 12px}
button{font-weight:600;cursor:pointer}button.p{background:var(--acc);border-color:var(--acc);color:#1d1404}
input{min-width:0;flex:1}
#msg{font-size:13px;color:var(--mut);min-height:1.5em}
</style></head><body><main>
<header><h1>Solar Tracker</h1><span class="chip" id="chip">connecting</span></header>
<section class="card"><div class="tiles">
<div><span>Panel voltage</span><b id="v">-</b></div>
<div><span>Panel current</span><b id="i">-</b></div>
<div><span>Power</span><b id="p">-</b></div>
<div><span>Cleanliness</span><b id="soil">-</b></div>
<div><span>Light (photo)</span><b id="light">-</b></div>
<div><span>Tilt</span><b id="tilt">-</b></div>
<div><span>LDR A / B</span><b id="ldr">-</b></div>
<div><span>Panel temp</span><b id="pt">-</b></div>
<div><span>Air / humidity</span><b id="air">-</b></div>
<div><span>Supply</span><b id="sup">-</b></div>
<div><span>Current sensor</span><b id="tr">-</b></div>
<div><span>Label</span><b id="lab">-</b></div>
</div></section>
<section class="card"><h2>Faults</h2><ul id="faults"><li style="border-color:var(--ok)">none</li></ul>
<p id="st" style="margin:10px 0 0;color:var(--mut)"></p></section>
<section class="card"><h2>Control</h2>
<div class="row">
<button class="p" onclick="cmd('clean')">Clean now</button>
<button onclick="cmd('calibrate')">Calibrate (clean panel)</button>
<button id="auto" onclick="cmd(autoOn?'auto off':'auto on')">Auto-clean</button>
<button onclick="cmd('mode auto')">Track</button>
<button onclick="cmd('mode fixed 0')">Fixed flat</button>
</div>
<div class="row" style="margin-top:10px">
<input id="lbl" placeholder="label, e.g. E2_light_s1"><button onclick="cmd('label '+lbl.value)">Set label</button>
<input id="mrk" placeholder="mark, e.g. inject_ldr"><button onclick="cmd('mark '+mrk.value)">Mark</button>
</div>
<p id="msg"></p></section>
<section class="card"><h2>Tests</h2>
<div class="row">
<button onclick="cmd('test sensors')">Sensor test</button>
<button onclick="cmd('selftest')">Current sensors (relay)</button>
<button onclick="cmd('test tilt')">Tilt test</button>
<button onclick="cmd('test wiper')">Wiper test</button>
<button class="p" onclick="cmd('test full')">Full test</button>
<button onclick="cmd('stop')">Stop</button>
</div>
<p id="tst" style="margin:10px 0 0"></p></section>
</main><script>
let autoOn=false;const $=id=>document.getElementById(id);
const f=(x,d,u)=>x==null?'-':x.toFixed(d)+u;
async function cmd(c){try{const r=await fetch('/api/cmd?c='+encodeURIComponent(c),{method:'POST'});$('msg').textContent=(await r.json()).msg}catch(e){$('msg').textContent='No reply'}}
async function poll(){try{const s=await(await fetch('/api/status')).json();
$('chip').textContent=s.faults.length?s.faults.length+' fault(s)':'OK';$('chip').style.background=s.faults.length?'var(--crit)':'var(--ok)';
$('v').textContent=f(s.panel_v,2,' V');$('i').textContent=f(s.current_a*1000,0,' mA');$('p').textContent=f(s.power_w,2,' W');
$('soil').textContent=s.calibrated?f(s.soil_light*100,0,' %'):'calibrate';$('light').textContent=f(s.photo_mv,0,' mV');
$('tilt').textContent=s.tilt+'° '+s.mode;$('ldr').textContent=s.ldr_a+' / '+s.ldr_b;$('pt').textContent=f(s.panel_c,1,' °C');
$('air').textContent=f(s.air_c,1,'°C')+' / '+f(s.humidity,0,'%');$('sup').textContent=f(s.supply_v,2,' V');$('tr').textContent=s.trusted;$('lab').textContent=s.label;
autoOn=s.auto_clean;$('auto').textContent='Auto-clean: '+(autoOn?'ON':'off');
$('st').textContent='Self-test: '+s.selftest+(s.wiper?'  ·  wiper running':'')+(s.demo?'  ·  demo mode':'')+'  ·  ThingSpeak: '+s.cloud;
$('tst').textContent='Last test: '+s.test+(s.test_running?'  (running)':'');
$('faults').innerHTML=s.faults.length?s.faults.map(x=>'<li><b>'+x.code+'</b>'+x.msg+'</li>').join(''):'<li style="border-color:var(--ok)">none</li>';
}catch(e){$('chip').textContent='offline';$('chip').style.background='var(--mut)'}}
poll();setInterval(poll,1000);
</script></body></html>)rawliteral";
