// WebServerManager.cpp  –  ESP32-RC-Sound v1.22
// Teil 2: HTML als PROGMEM, API-Endpunkte aus v1.00 übernommen
// buildPage() entfernt → 60 KB weniger RAM-Allokation pro Request
#include "WebServerManager.h"
#include "config.h"
#include <Arduino.h>
#include "XT_I2S_Audio.h"
#include "sport_lipo.h"

// ── PROGMEM HTML ─────────────────────────────────────────────────────────
// Das komplette UI liegt im Flash (PROGMEM). server.send_P() streamt es
// direkt ohne RAM-Kopie. Keine String-Allokation mehr pro Request.
static const char RCSOUND_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 RC-Sound</title>
<style>
:root{
  --bg:#0d1117;--surf:#161b22;--surf2:#21262d;
  --border:#30363d;--accent:#4493f8;--green:#3fb950;
  --yellow:#d29922;--red:#f85149;--text:#e6edf3;
  --sub:#8b949e;--r:8px;--mono:'Courier New',monospace;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;font-size:14px}
header{background:var(--surf);border-bottom:1px solid var(--border);padding:12px 16px;display:flex;align-items:center;position:sticky;top:0;z-index:50}
header h1{font-size:16px;font-weight:700;flex:1}
.hv{font-size:11px;color:var(--sub)}
nav{background:var(--surf);border-bottom:1px solid var(--border);display:flex;overflow-x:auto;scrollbar-width:none;position:sticky;top:45px;z-index:40}
nav::-webkit-scrollbar{display:none}
.tab{flex-shrink:0;padding:10px 14px;font-size:12px;font-weight:600;color:var(--sub);cursor:pointer;border-bottom:2px solid transparent;transition:.2s;white-space:nowrap}
.tab:hover{color:var(--text)}
.tab.active{color:var(--accent);border-bottom-color:var(--accent)}
main{max-width:560px;margin:0 auto;padding:14px 14px 60px}
.card{background:var(--surf);border:1px solid var(--border);border-radius:var(--r);padding:14px;margin-bottom:10px}
.card-title{font-weight:600;font-size:11px;color:var(--sub);text-transform:uppercase;letter-spacing:.05em;margin-bottom:10px}
.row{display:flex;align-items:center;gap:8px;margin-bottom:8px}
.lbl{font-size:13px;color:var(--sub);flex:0 0 130px}
select,input[type=text],input[type=password],input[type=number]{
  width:100%;background:var(--surf2);border:1px solid var(--border);
  border-radius:var(--r);color:var(--text);padding:8px 10px;font-size:13px;outline:none;transition:border .2s}
select:focus,input:focus{border-color:var(--accent)}
.sl-wrap{margin-bottom:12px}
.sl-head{display:flex;justify-content:space-between;font-size:12px;color:var(--sub);margin-bottom:5px}
input[type=range]{width:100%;-webkit-appearance:none;height:4px;border-radius:2px;background:var(--surf2);outline:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:var(--accent);cursor:pointer}
input[type=range]::-moz-range-thumb{width:18px;height:18px;border-radius:50%;background:var(--accent);cursor:pointer;border:none}
.btn{display:inline-flex;align-items:center;justify-content:center;padding:9px 16px;border-radius:var(--r);font-size:13px;font-weight:600;cursor:pointer;border:none;transition:.15s;width:100%;margin-top:6px}
.btn-p{background:var(--accent);color:#fff}.btn-p:hover{opacity:.85}
.btn-g{background:var(--green);color:#fff}.btn-g:hover{opacity:.85}
.btn-r{background:var(--red);color:#fff}.btn-r:hover{opacity:.85}
.btn-s{background:var(--surf2);color:var(--text);border:1px solid var(--border)}.btn-s:hover{border-color:var(--accent);color:var(--accent)}
.g2{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.badge{display:inline-block;padding:2px 8px;border-radius:20px;font-size:11px;font-weight:600}
.bg-g{background:#1a3a1a;color:var(--green)}
.bg-r{background:#3a1a1a;color:var(--red)}
.bg-s{background:var(--surf2);color:var(--sub)}
.stabs{display:flex;gap:4px;flex-wrap:wrap;margin-bottom:12px}
.stab{padding:5px 10px;border-radius:var(--r);font-size:12px;font-weight:600;cursor:pointer;background:var(--surf2);color:var(--sub);border:1px solid var(--border);transition:.15s}
.stab.active{background:var(--accent);color:#fff;border-color:var(--accent)}
.cell-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin-top:8px}
.cell-box{background:var(--surf2);border-radius:var(--r);padding:8px;text-align:center}
.cell-v{font-size:17px;font-weight:700}
.cell-l{font-size:10px;color:var(--sub);margin-top:2px}
.dbg-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:4px}
.dbg-item{background:var(--surf2);border-radius:4px;padding:6px 8px}
.dbg-k{font-size:10px;color:var(--sub)}
.dbg-v{font-size:13px;font-weight:600;margin-top:1px}
hr{border:none;border-top:1px solid var(--border);margin:10px 0}
.mono{font-family:var(--mono);font-size:12px}
.hidden{display:none!important}
/* Toast */
#toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%);
  background:var(--green);color:#fff;padding:10px 20px;border-radius:var(--r);
  font-size:13px;font-weight:600;opacity:0;transition:opacity .3s;pointer-events:none;z-index:99}
#toast.err{background:var(--red)}
#toast.show{opacity:1}
</style>
</head>
<body>
<header>
  <h1>&#9654; ESP32 RC-Sound</h1>
  <span class="hv" id="hv">laden...</span>
</header>
<nav>
  <div class="tab active" onclick="showTab('motor')">Motor</div>
  <div class="tab" onclick="showTab('sounds')">Sounds</div>
  <div class="tab" onclick="showTab('settings')">Einstellung</div>
  <div class="tab" onclick="showTab('wifi')">WiFi</div>
  <div class="tab" onclick="showTab('lipo')">LiPo</div>
  <div class="tab" onclick="showTab('debug')">Debug</div>
</nav>
<div id="toast"></div>
<main>

<!-- ═══ MOTOR ═══════════════════════════════════════════════════════════ -->
<div id="tab-motor">
  <div class="card">
    <div class="card-title">Motor Mode</div>
    <select id="m-mode">
      <option value="0">Eine Richtung</option>
      <option value="1">Zwei Richtungen</option>
    </select>
  </div>
  <div class="card">
    <div class="card-title">Motor EIN Modus</div>
    <select id="m-toggle">
      <option value="0">Normal</option>
      <option value="1">Toggle</option>
    </select>
  </div>
  <div class="card">
    <div class="card-title">Quelle Einschalten Motor</div>
    <select id="m-src"></select>
  </div>
  <div class="card">
    <div class="card-title">Quelle Motorspeed</div>
    <select id="m-spd"></select>
  </div>
  <div class="card">
    <div class="card-title">Lautst&auml;rke &amp; Drehzahl</div>
    <div class="sl-wrap">
      <div class="sl-head"><span>Volumen</span><span id="lv-m-vol">-</span></div>
      <input type="range" id="m-vol" min="0" max="200" step="5" oninput="slUpd('m-vol','lv-m-vol')">
    </div>
    <div class="sl-wrap">
      <div class="sl-head"><span>Drehzahl min %</span><span id="lv-m-rmin">-</span></div>
      <input type="range" id="m-rmin" min="0" max="200" step="5" oninput="slUpd('m-rmin','lv-m-rmin')">
    </div>
    <div class="sl-wrap">
      <div class="sl-head"><span>Drehzahl max %</span><span id="lv-m-rmax">-</span></div>
      <input type="range" id="m-rmax" min="100" max="600" step="5" oninput="slUpd('m-rmax','lv-m-rmax')">
    </div>
    <div class="sl-wrap">
      <div class="sl-head"><span>Standgas Verz&ouml;gerung s</span><span id="lv-m-sdly">-</span></div>
      <input type="range" id="m-sdly" min="0" max="60" step="2" oninput="slUpd('m-sdly','lv-m-sdly')">
    </div>
    <div class="sl-wrap">
      <div class="sl-head"><span>Motor Rampe %/s</span><span id="lv-m-ramp">-</span></div>
      <input type="range" id="m-ramp" min="0" max="50" step="2" oninput="slUpd('m-ramp','lv-m-ramp')">
    </div>
    <div class="sl-wrap">
      <div class="sl-head"><span>Standgas Totband %</span><span id="lv-m-dead">-</span></div>
      <input type="range" id="m-dead" min="0" max="50" step="2" oninput="slUpd('m-dead','lv-m-dead')">
    </div>
  </div>
  <div class="g2">
    <button class="btn btn-p" onclick="saveMotor()">Speichern</button>
    <button class="btn btn-s" onclick="loadConfig()">Zur&uuml;cksetzen</button>
  </div>
  <div class="card" style="margin-top:10px">
    <div class="card-title">Voreinstellungen</div>
    <div class="g2">
      <button class="btn btn-s" onclick="xget('/setsbus',0,function(){loadConfig();})">SBUS</button>
      <button class="btn btn-s" onclick="xget('/setpwm',0,function(){loadConfig();})">PWM</button>
    </div>
    <button class="btn btn-s" style="margin-top:6px" onclick="xget('/setpin',0,function(){loadConfig();})">PIN</button>
    <button class="btn btn-r" style="margin-top:6px" onclick="if(confirm('Werkseinstellung laden?'))xget('/reset',0,function(){loadConfig();})">Werkseinstellung</button>
  </div>
</div>

<!-- ═══ SOUNDS ══════════════════════════════════════════════════════════ -->
<div id="tab-sounds" class="hidden">
  <div class="stabs" id="stabs"></div>
  <div class="card">
    <div class="card-title">Quelle Einschalten</div>
    <select id="s-src"></select>
  </div>
  <div class="card">
    <div class="card-title">Lautst&auml;rke</div>
    <div class="sl-wrap">
      <div class="sl-head"><span>Volumen</span><span id="lv-s-vol">-</span></div>
      <input type="range" id="s-vol" min="0" max="200" step="5" oninput="slUpd('s-vol','lv-s-vol')">
    </div>
  </div>
  <div class="card">
    <div class="card-title">Wiedergabe Modus</div>
    <select id="s-mode">
      <option value="0">Normal</option>
      <option value="1">Loop</option>
      <option value="2">Tippbetrieb</option>
    </select>
  </div>
  <div class="g2">
    <button class="btn btn-p" onclick="saveSound()">Speichern</button>
    <button class="btn btn-g" onclick="testSound()">&#9654; Test Sound</button>
  </div>
</div>

<!-- ═══ EINSTELLUNGEN ═══════════════════════════════════════════════════ -->
<div id="tab-settings" class="hidden">
  <div class="card">
    <div class="card-title">RC-System</div>
    <select id="rc-sys" onchange="onRcSys()">
      <option value="0">FrSky</option>
      <option value="1">FlySky</option>
      <option value="2">ELRS (SBUS)</option>
      <option value="3">Hott</option>
      <option value="4">ELRS (CRSF)</option>
    </select>
  </div>
  <div class="card" id="c-ekch">
    <div class="card-title">Einkanal Kanal</div>
    <select id="ek-ch"></select>
  </div>
  <div class="card" id="c-ekmo">
    <div class="card-title">Einkanal-Mode</div>
    <select id="ek-mode">
      <option value="0">Normal</option>
      <option value="10">SBUS WM Adr 0</option>
      <option value="11">SBUS WM Adr 1</option>
      <option value="12">SBUS WM Adr 2</option>
      <option value="13">SBUS WM Adr 3</option>
    </select>
  </div>
  <div class="card hidden" id="c-madr">
    <div class="card-title">Modul Adresse (CRSF)</div>
    <select id="mod-adr"></select>
  </div>
  <hr>
  <div class="card" id="c-pwm">
    <div class="card-title">PWM Einstellungen</div>
    <div class="row" style="margin-bottom:10px">
      <span class="lbl">PWM min (&micro;s)</span>
      <input type="number" id="pwm-min" min="0" max="4095" style="width:120px">
    </div>
    <div class="row">
      <span class="lbl">PWM max (&micro;s)</span>
      <input type="number" id="pwm-max" min="0" max="4095" style="width:120px">
    </div>
  </div>

  <hr>
  <div class="card">
    <div class="card-title">Hardware Config</div>
    <select id="hw-cfg" onchange="onHwCfg()">
      <option value="0">V1 (GPIO 16,17,22,0,2,4)</option>
      <option value="1">V2 (GPIO 16,17,14,27,32,33)</option>
      <option value="2">V3 (nur BUS+Einkanal)</option>
      <option value="3">V4 (BUS+Einkanal+S.Port GPIO32/33)</option>
    </select>
  </div>
  <button class="btn btn-p" onclick="saveSettings()">Speichern</button>
</div>

<!-- ═══ WIFI ════════════════════════════════════════════════════════════ -->
<div id="tab-wifi" class="hidden">
  <div class="card">
    <div class="card-title">WiFi Einstellungen</div>
    <div class="sl-wrap">
      <div class="sl-head"><span>SSID</span></div>
      <input type="text" id="w-ssid" maxlength="31">
    </div>
    <div class="sl-wrap">
      <div class="sl-head"><span>Passwort</span></div>
      <input type="password" id="w-pass" maxlength="63">
    </div>
    <div class="sl-wrap">
      <div class="sl-head"><span>AP IP-Adresse</span></div>
      <input type="text" id="w-ip" maxlength="15" placeholder="192.168.1.1">
      <div style="font-size:11px;color:var(--sub);margin-top:3px">(Neue IP gilt nach Neustart)</div>
    </div>
    <div class="sl-wrap">
      <div class="sl-head"><span>Ger&auml;tename (TBS Agent)</span></div>
      <input type="text" id="w-dev" maxlength="23">
      <div style="font-size:11px;color:var(--sub);margin-top:3px">(Im Agent sichtbar als Name@Adresse)</div>
    </div>
    <button class="btn btn-p" onclick="saveWifi()">Speichern</button>
  </div>
</div>

<!-- ═══ LIPO ════════════════════════════════════════════════════════════ -->
<div id="tab-lipo" class="hidden">
  <div class="card">
    <div class="card-title">Sensor Adresse (Poll-ID)</div>
    <div class="row" style="margin-bottom:4px">
      <span class="lbl">Sensor 1 (Pack 1)</span>
      <input type="text" id="pid0" maxlength="2" style="width:80px;font-family:var(--mono);text-transform:uppercase">
    </div>
    <div style="font-size:11px;color:var(--sub);margin-bottom:10px">Werkseinstellung: A1 (Physical ID 0x02)</div>
    <div class="row" style="margin-bottom:4px">
      <span class="lbl">Sensor 2 (Pack 2)</span>
      <input type="text" id="pid1" maxlength="2" style="width:80px;font-family:var(--mono);text-transform:uppercase">
    </div>
    <div style="font-size:11px;color:var(--sub);margin-bottom:10px">Werkseinstellung: 22 (Physical ID 0x03)</div>
    <button class="btn btn-p" onclick="saveLipo()">Speichern</button>
  </div>
  <div class="card">
    <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:10px">
      <span class="card-title" style="margin:0">Live Sensorwerte</span>
      <button class="btn btn-s" style="width:auto;padding:4px 12px;margin:0;font-size:11px" onclick="loadDebug()">&#8635; Aktualisieren</button>
    </div>
    <div id="lipo-packs"><div style="color:var(--sub);font-size:12px">Wird geladen...</div></div>
  </div>
</div>

<!-- ═══ DEBUG ════════════════════════════════════════════════════════════ -->
<div id="tab-debug" class="hidden">
  <div class="card">
    <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:10px">
      <span class="card-title" style="margin:0">BUS Kan&auml;le</span>
      <button class="btn btn-s" style="width:auto;padding:4px 12px;margin:0;font-size:11px" onclick="loadDebug()">&#8635; Aktualisieren</button>
    </div>
    <div class="dbg-grid" id="dbg-ch"></div>
  </div>
  <div class="card">
    <div class="card-title">PWM Pins</div>
    <div id="dbg-pwm"></div>
  </div>
  <div class="card">
    <div class="card-title">WAV Dateien</div>
    <div id="dbg-wav"></div>
  </div>
  <div class="card">
    <div class="card-title">Konfiguration</div>
    <div id="dbg-cfg" class="mono" style="line-height:1.9;color:var(--sub)"></div>
  </div>
</div>

</main>
<script>
var cfg={}, dbg={}, curSnd=1, lipoTmr=0;
var TABS=['motor','sounds','settings','wifi','lipo','debug'];

// ─── Toast ────────────────────────────────────────────────────────────────
var toastTmr=0;
function toast(msg,err){
  var el=document.getElementById('toast');
  el.textContent=msg; el.className=err?'err show':'show';
  clearTimeout(toastTmr);
  toastTmr=setTimeout(function(){el.className=err?'err':'';},2200);
}

// ─── Tab-Navigation ───────────────────────────────────────────────────────
function showTab(t){
  TABS.forEach(function(n){
    var el=document.getElementById('tab-'+n);
    if(el) el.className=(n===t?'':'hidden');
  });
  document.querySelectorAll('.tab').forEach(function(el,i){
    el.className='tab'+(TABS[i]===t?' active':'');
  });
  if(t==='debug'||t==='lipo') loadDebug();
  clearInterval(lipoTmr); lipoTmr=0;
  if(t==='lipo') lipoTmr=setInterval(loadDebug,3000);
}

// ─── XHR-Helfer ──────────────────────────────────────────────────────────
function xget(url,_,cb){
  var x=new XMLHttpRequest();
  x.open('GET',url,true);
  x.onload=function(){if(cb)cb(x.responseText);};
  x.send();
}
function xpost(url,data,cb){
  var x=new XMLHttpRequest();
  x.open('POST',url,true);
  x.setRequestHeader('Content-Type','application/json');
  x.onload=function(){if(cb)cb(x.responseText);};
  x.onerror=function(){toast('Verbindungsfehler',1);};
  x.send(JSON.stringify(data));
}

// ─── Dropdown-Optionen ───────────────────────────────────────────────────
// Dropdown-Gruppen – aufgeteilt damit V3/V4 filtern kann
var G_BUS_LOW ={l:'BUS Kanal Low', o:function(){var a=[];for(var i=0;i<16;i++)a.push([i,'BUS Kanal Low '+(i<9?'0':'')+(i+1)]);return a;}()};
var G_BUS_HIGH={l:'BUS Kanal High',o:function(){var a=[];for(var i=0;i<16;i++)a.push([20+i,'BUS Kanal High '+(i<9?'0':'')+(i+1)]);return a;}()};
var G_PWM_LOW ={l:'PWM Pin Low',   o:function(){var a=[];for(var i=0;i<6;i++)a.push([40+i,'PWM Pin Low '+(i+1)]);return a;}()};
var G_PWM_HIGH={l:'PWM Pin High',  o:function(){var a=[];for(var i=0;i<6;i++)a.push([50+i,'PWM Pin High '+(i+1)]);return a;}()};
var G_PIN     ={l:'Eingang Pin',   o:function(){var a=[];for(var i=0;i<6;i++)a.push([60+i,'Eingang Pin '+(i+1)]);return a;}()};
var G_EK      ={l:'Einkanal',      o:function(){var a=[];for(var i=0;i<8;i++)a.push([70+i,'Einkanal '+(i<9?'0':'')+(i+1)]);return a;}()};
var G_OPT     ={l:'Optionen',      o:[[200,'Dauerbetrieb an'],[999,'Deaktiviert']]};
var G_OPT_D   ={l:'Optionen',      o:[[999,'Deaktiviert']]};
var G_PWM_SPD ={l:'PWM Pin',       o:function(){var a=[];for(var i=0;i<6;i++)a.push([20+i,'PWM Pin '+(i+1)]);return a;}()};
var G_BUS_SPD ={l:'BUS Kanal',     o:function(){var a=[];for(var i=0;i<16;i++)a.push([i,'BUS Kanal '+(i<9?'0':'')+(i+1)]);return a;}()};

// Gruppen je nach HW-Config zusammenstellen
// hw < 2 (V1/V2): alle; hw >= 2 (V3/V4): nur BUS + Einkanal
function srcGroups(hw) {
  var g = [G_BUS_LOW, G_BUS_HIGH];
  if (hw < 2) { g.push(G_PWM_LOW, G_PWM_HIGH, G_PIN); }
  g.push(G_EK);
  g.push(G_OPT);
  return g;
}
function spdGroups(hw) {
  var g = [G_BUS_SPD];
  if (hw < 2) g.push(G_PWM_SPD);
  g.push(G_OPT_D);
  return g;
}

function fillSel(id,groups){
  var sel=document.getElementById(id); if(!sel)return;
  sel.innerHTML='';
  groups.forEach(function(g){
    var og=document.createElement('optgroup'); og.label=g.l;
    g.o.forEach(function(o){
      var opt=document.createElement('option');
      opt.value=o[0]; opt.textContent=o[1]; og.appendChild(opt);
    });
    sel.appendChild(og);
  });
}
function fillEkCh(){
  var sel=document.getElementById('ek-ch'); sel.innerHTML='';
  var og=document.createElement('optgroup'); og.label='SBUS Kanal';
  for(var i=0;i<16;i++){var o=document.createElement('option');o.value=i;o.textContent='SBUS Kanal '+(i<9?'0':'')+(i+1);og.appendChild(o);}
  sel.appendChild(og);
  var og2=document.createElement('optgroup'); og2.label='Optionen';
  var o2=document.createElement('option');o2.value=999;o2.textContent='Deaktiviert';og2.appendChild(o2);sel.appendChild(og2);
}
function fillModAdr(){
  var sel=document.getElementById('mod-adr'); sel.innerHTML='';
  for(var i=0;i<=20;i++){var o=document.createElement('option');o.value=i;o.textContent=i;sel.appendChild(o);}
}

// ─── Hilfsfunktionen ────────────────────────────────────────────────────
function sv(id,v){var el=document.getElementById(id);if(el)el.value=v;}
function gv(id){var el=document.getElementById(id);return el?el.value:'';}
function gi(id){return parseInt(gv(id))||0;}
function slUpd(sid,lid){var v=gv(sid);document.getElementById(lid).textContent=v;}
function slSet(sid,lid,v){sv(sid,v);if(lid)document.getElementById(lid).textContent=v;}

// ─── RC-System / HW-Config bedingte Felder ──────────────────────────────
function onRcSys(){
  var crsf=(gi('rc-sys')===4);
  document.getElementById('c-ekch').className='card'+(crsf?' hidden':'');
  document.getElementById('c-ekmo').className='card'+(crsf?' hidden':'');
  document.getElementById('c-madr').className='card'+(crsf?'':' hidden');
}
function onHwCfg(){
  var hw = gi('hw-cfg');
  document.getElementById('c-pwm').className = 'card'+(hw<2?'':' hidden');
  // Quell-Dropdowns neu befüllen – V3/V4 ohne PWM/Pin-Gruppen
  var curMSrc = gv('m-src');
  var curMSpd = gv('m-spd');
  var curSSrc = gv('s-src');
  fillSel('m-src', srcGroups(hw)); sv('m-src', curMSrc);
  fillSel('m-spd', spdGroups(hw));       sv('m-spd', curMSpd);
  fillSel('s-src', srcGroups(hw)); sv('s-src', curSSrc);
}

// ─── Sound-Tabs ──────────────────────────────────────────────────────────
function buildSndTabs(){
  var c=document.getElementById('stabs'); c.innerHTML='';
  for(var i=1;i<=8;i++){
    var b=document.createElement('div');
    b.className='stab'+(i===1?' active':'');
    b.textContent='Sound '+i; b.setAttribute('data-s',i);
    b.onclick=(function(n){return function(){selSnd(n);};})(i);
    c.appendChild(b);
  }
}
function selSnd(n){
  curSnd=n;
  document.querySelectorAll('.stab').forEach(function(el){
    el.className='stab'+(parseInt(el.getAttribute('data-s'))===n?' active':'');
  });
  if(!cfg.sounds)return;
  var s=cfg.sounds[n-1];
  sv('s-src',s.source); slSet('s-vol','lv-s-vol',s.vol); sv('s-mode',s.mode);
}

// ─── Config laden ─────────────────────────────────────────────────────────
function loadConfig(){
  xget('/api/config',0,function(resp){
    try{cfg=JSON.parse(resp);}catch(e){toast('Ladefehler',1);return;}
    document.getElementById('hv').textContent='v'+cfg.version;
    var m=cfg.motor;
    sv('m-mode',m.mode); sv('m-toggle',m.toggle);
    sv('m-src',m.source); sv('m-spd',m.speed_src);
    slSet('m-vol','lv-m-vol',m.vol);
    slSet('m-rmin','lv-m-rmin',m.rpm_min);
    slSet('m-rmax','lv-m-rmax',m.rpm_max);
    slSet('m-sdly','lv-m-sdly',m.shutdown_s);
    slSet('m-ramp','lv-m-ramp',m.ramp);
    slSet('m-dead','lv-m-dead',m.deadband);
    if(cfg.sounds) selSnd(curSnd);
    var s=cfg.settings;
    sv('rc-sys',s.rc_system); sv('ek-ch',s.ek_channel);
    sv('ek-mode',s.ek_mode); sv('mod-adr',s.modul_adr);
    sv('pwm-min',s.pwm_min); sv('pwm-max',s.pwm_max);
    sv('hw-cfg',s.hw_config);
    onRcSys(); onHwCfg();
    var w=cfg.wifi;
    sv('w-ssid',w.ssid); sv('w-pass',w.pass||'');
    sv('w-ip',w.ip); sv('w-dev',w.device);
    var sp=cfg.sport;
    sv('pid0',sp.poll_id0); sv('pid1',sp.poll_id1);
  });
}

// ─── Debug / LiPo laden ─────────────────────────────────────────────────
function loadDebug(){
  xget('/api/debug',0,function(resp){
    try{dbg=JSON.parse(resp);}catch(e){return;}
    renderDbg(); renderLipo();
  });
}
function renderDbg(){
  var ch=document.getElementById('dbg-ch');
  if(ch&&dbg.channels){
    ch.innerHTML='';
    dbg.channels.forEach(function(v,i){
      ch.innerHTML+='<div class="dbg-item"><div class="dbg-k">K'+(i+1)+'</div><div class="dbg-v">'+v+'</div></div>';
    });
  }
  var pw=document.getElementById('dbg-pwm');
  if(pw&&dbg.pwm){
    pw.innerHTML='';
    dbg.pwm.forEach(function(p,i){
      pw.innerHTML+='<div class="row"><span class="lbl">PWM '+(i+1)+'</span><span style="font-size:13px;font-family:var(--mono)">'+p.us+' &micro;s &rarr; '+p.pct+' %</span></div>';
    });
  }
  var wv=document.getElementById('dbg-wav');
  if(wv&&dbg.wav){
    var w=dbg.wav,h='';
    ['loop','shut','start'].forEach(function(n){
      h+='<div class="row"><span class="lbl">'+n+'.wav</span><span class="badge '+(w[n]?'bg-g':'bg-r')+'">'+(w[n]?'OK':'fehlt')+'</span></div>';
    });
    if(w.s)w.s.forEach(function(v,i){
      h+='<div class="row"><span class="lbl">sound'+(i+1)+'.wav</span><span class="badge '+(v?'bg-g':'bg-r')+'">'+(v?'OK':'fehlt')+'</span></div>';
    });
    wv.innerHTML=h;
  }
  var dc=document.getElementById('dbg-cfg');
  if(dc&&cfg.motor&&cfg.settings){
    var m=cfg.motor,s=cfg.settings;
    dc.innerHTML=
      'HW:'+s.hw_config+' RC-Sys:'+s.rc_system+' EK-Ch:'+s.ek_channel+'<br>'+
      'Motor-Start:'+m.source+' Speed:'+m.speed_src+'<br>'+
      'Rampe:'+m.ramp+' Totband:'+m.deadband+'<br>'+
      'S.Port:0x'+(cfg.sport?cfg.sport.poll_id0:'?')+'/0x'+(cfg.sport?cfg.sport.poll_id1:'?');
  }
}
function renderLipo(){
  var el=document.getElementById('lipo-packs');
  if(!el)return;
  if(!dbg.sport){el.innerHTML='<div style="color:var(--sub);font-size:12px">Keine Daten empfangen</div>';return;}
  if(dbg.hw_config!==undefined&&dbg.hw_config!==3){
    el.innerHTML='<div style="color:var(--yellow);font-size:12px">&#9888; Hardware Config ist nicht V4 &ndash; S.Port LiPo-Telemetrie inaktiv.<br>Auf V4 umstellen und neu starten.</div>';
    return;
  }
  var sp=dbg.sport,h='';
  sp.packs.forEach(function(p,i){
    h+='<div style="margin-bottom:14px">';
    h+='<div class="row"><span style="font-weight:700">Pack '+(i+1)+'</span>';
    h+='<span class="badge '+(p.online?'bg-g':'bg-s')+'">'+(p.online?'online':'offline')+'</span></div>';
    if(p.online){
      h+='<div class="row"><span class="lbl">Gesamt</span><span style="font-weight:700">'+p.total.toFixed(2)+' V</span></div>';
      h+='<div class="row"><span class="lbl">SoC</span><span style="font-weight:700">'+sp.soc+' %</span></div>';
      h+='<div class="row"><span class="lbl">Min-Zelle</span><span style="font-weight:700">'+p.min.toFixed(3)+' V</span></div>';
      h+='<div class="cell-grid">';
      for(var c=0;c<p.cells;c++){
        var v=p.cv[c];
        var col=v<3.5?'var(--red)':v<3.7?'var(--yellow)':'var(--green)';
        h+='<div class="cell-box"><div class="cell-v" style="color:'+col+'">'+v.toFixed(3)+'</div><div class="cell-l">Z'+(c+1)+'</div></div>';
      }
      h+='</div>';
    }else{
      h+='<div style="color:var(--sub);font-size:12px;padding:6px 0">Kein Signal &ndash; Sensor angeschlossen und Poll-ID korrekt?</div>';
    }
    h+='</div>';
  });
  el.innerHTML=h||'<div style="color:var(--sub)">Keine Sensordaten</div>';
}

// ─── Speichern ────────────────────────────────────────────────────────────
function saveMotor(){
  xpost('/api/sound',{
    id:0,
    source:gi('m-src'), speed_src:gi('m-spd'),
    vol:gi('m-vol'), motor_mode:gi('m-mode'), toggle:gi('m-toggle'),
    rpm_min:gi('m-rmin'), rpm_max:gi('m-rmax'),
    shutdown_s:gi('m-sdly'), ramp:gi('m-ramp'), deadband:gi('m-dead')
  },function(r){
    try{var d=JSON.parse(r);if(d.ok){xget('/save',0,function(){toast('Motor gespeichert');});return;}}catch(e){}
    toast('Fehler',1);
  });
}
function saveSound(){
  xpost('/api/sound',{
    id:curSnd,
    source:gi('s-src'), vol:gi('s-vol'), mode:gi('s-mode')
  },function(r){
    try{var d=JSON.parse(r);if(d.ok){xget('/save',0,function(){toast('Sound '+curSnd+' gespeichert');});return;}}catch(e){}
    toast('Fehler',1);
  });
}
function testSound(){
  xpost('/api/sound',{id:curSnd,test:true},function(){toast('Sound '+curSnd+' wird gespielt');});
}
function saveSettings(){
  var d={
    rc_system:gi('rc-sys'),   ek_channel:gi('ek-ch'),
    ek_mode:gi('ek-mode'),    modul_adr:gi('mod-adr'),
    hw_config:gi('hw-cfg')
  };
  // PWM nur bei V1/V2 senden (bei V3/V4 nicht vorhanden)
  if(gi('hw-cfg')<2){d.pwm_min=gi('pwm-min');d.pwm_max=gi('pwm-max');}
  xpost('/api/config',d,function(r){
    try{var d=JSON.parse(r);if(d.ok){toast('Einstellungen gespeichert');onHwCfg();return;}}catch(e){}
    toast('Fehler',1);
  });
}
function saveWifi(){
  xpost('/api/config',{
    ssid:gv('w-ssid'), pass:gv('w-pass'),
    ip:gv('w-ip'),     device:gv('w-dev')
  },function(r){
    try{var d=JSON.parse(r);if(d.ok){toast('WiFi gespeichert');return;}}catch(e){}
    toast('Fehler',1);
  });
}
function saveLipo(){
  xpost('/api/config',{
    poll_id0:gv('pid0'), poll_id1:gv('pid1')
  },function(r){
    try{var d=JSON.parse(r);if(d.ok){toast('S.Port gespeichert');return;}}catch(e){}
    toast('Fehler',1);
  });
}

// ─── Init ────────────────────────────────────────────────────────────────
// Dropdowns initial mit hw=0 befüllen; nach loadConfig() mit echtem hw-Wert neu befüllen
fillSel('m-src', srcGroups(0, false));
fillSel('m-spd', spdGroups(0));
fillSel('s-src', srcGroups(0));
fillEkCh(); fillModAdr(); buildSndTabs();
loadConfig();
</script>
</body>
</html>)HTML";


WebServer  WebServerManager::server(80);
int        WebServerManager::Menu        = 0;
String     WebServerManager::valueString = "";

extern bool          Sound_on_web[9];
extern uint16_t      channel_output[16];
extern volatile unsigned int PWM_pulse_width[6];
extern XT_Wav_Class  Sound_loop;
extern XT_Wav_Class  Sound_shut;
extern XT_Wav_Class  Sound_start;
extern XT_Wav_Class  Sound1, Sound2, Sound3, Sound4;
extern XT_Wav_Class  Sound5, Sound6, Sound7, Sound8;
extern char          versionString[6];

// URL-Dekodierung
String WebServerManager::urlDecode(const String& s) {
  String result = "";
  result.reserve(s.length());
  for (int i = 0; i < (int)s.length(); i++) {
    if (s[i] == '+') { result += ' '; }
    else if (s[i] == '%' && i+2 < (int)s.length()) {
      char hex[3] = { s[i+1], s[i+2], 0 };
      result += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else { result += s[i]; }
  }
  return result;
}

// ── AP starten + Routen registrieren ────────────────────────────────────
void WebServerManager::begin(const char* apSsid, const char* apPassword) {
  WiFi.softAP(apSsid, apPassword);
  IPAddress apIP, gw, subnet(255,255,255,0);
  if (!apIP.fromString(config.WiFi_IP)) apIP.fromString("192.168.1.1");
  gw = apIP;
  WiFi.softAPConfig(apIP, gw, subnet);
  Serial.print("AP gestartet, IP: ");
  Serial.println(WiFi.softAPIP());

  // Alle Requests auf einen Handler
  server.onNotFound([]() { WebServerManager::handleRequest(); });
  server.on("/", []()    { WebServerManager::handleRequest(); });
  server.on("/sport",         []() { WebServerManager::handleSport();     });
  server.on("/api/config", HTTP_GET,  []() { WebServerManager::handleApiConfig(); });
  server.on("/api/config", HTTP_POST, []() { WebServerManager::handleApiConfigPost(); });
  server.on("/api/debug",     []() { WebServerManager::handleApiDebug();  });
  server.on("/api/sound",     []() { WebServerManager::handleApiSound();  });
  server.begin();
}

// ── handleClient(): non-blocking, kehrt sofort zurueck ──────────────────
void WebServerManager::Webpage() {
  server.handleClient();
}

// ── /sport JSON-Endpunkt (LiPo Live-Daten) ──────────────────────────────
void WebServerManager::handleSport() {
  char json[512]; int pos = 0;
  pos += snprintf(json+pos, sizeof(json)-pos, "{");
  for (uint8_t i = 0; i < 2; i++) {
    pos += snprintf(json+pos, sizeof(json)-pos,
      "\"s%u\":{\"online\":%s,\"cells\":%u,\"total\":%.2f,\"min\":%.3f,\"soc\":%u,\"cv\":[",
      i, lipoSensor[i].online ? "true" : "false",
      lipoSensor[i].cellCount, lipoSensor[i].totalVoltage,
      lipoSensor[i].minCell, sportCalcSoC(lipoSensor[i].minCell));
    for (uint8_t c = 0; c < 6; c++)
      pos += snprintf(json+pos, sizeof(json)-pos, "%.3f%s",
        lipoSensor[i].cellVoltage[c], c < 5 ? "," : "");
    pos += snprintf(json+pos, sizeof(json)-pos, "]}%s", i < 1 ? "," : "");
  }
  pos += snprintf(json+pos, sizeof(json)-pos,
    ",\"pid0\":\"%02X\",\"pid1\":\"%02X\"}",
    config.sport_poll_id[0], config.sport_poll_id[1]);
  server.send(200, "application/json", json);
}


// ── JSON-Hilfsfunktionen ──────────────────────────────────────────────────
static void sendJson(WebServer& sv, const String& json) {
  sv.sendHeader("Cache-Control", "no-cache");
  sv.send(200, "application/json", json);
}
static void sendOk(WebServer& sv)  { sendJson(sv, "{\"ok\":true}"); }
static void sendErr(WebServer& sv) { sv.send(400, "application/json", "{\"error\":\"bad request\"}"); }

static bool jsonGetInt(const String& body, const char* key, int& out) {
  String k = "\"" + String(key) + "\":";
  int pos = body.indexOf(k);
  if (pos < 0) return false;
  out = body.substring(pos + k.length()).toInt();
  return true;
}
static bool jsonGetStr(const String& body, const char* key, char* out, size_t maxlen) {
  String k = "\"" + String(key) + "\":\"";
  int pos = body.indexOf(k);
  if (pos < 0) return false;
  int start = pos + k.length();
  int end   = body.indexOf('"', start);
  if (end < 0) return false;
  String val = body.substring(start, end);
  strncpy(out, val.c_str(), maxlen - 1);
  out[maxlen - 1] = '\0';
  return true;
}


// ── POST /api/config ─────────────────────────────────────────────────────
// Schreibt Settings, WiFi und S.Port Poll-IDs.
// Body: JSON mit beliebigen der folgenden Felder (alle optional):
//   settings: { rc_system, ek_channel, ek_mode, modul_adr,
//               hw_config, pwm_min, pwm_max }
//   wifi:     { ssid, pass, ip, device }
//   sport:    { poll_id0, poll_id1 }  (Hex-String, z.B. "A1")
void WebServerManager::handleApiConfigPost() {
  if (!server.hasArg("plain")) { sendErr(server); return; }
  const String& body = server.arg("plain");
  bool changed = false;
  int  v;

  // Settings
  if (jsonGetInt(body, "rc_system",  v)) { config.Einkanal_RC_System        = constrain(v,0,4);    changed=true; }
  if (jsonGetInt(body, "ek_channel", v)) { config.Einkanal_Channel           = (v==999)?999:constrain(v,0,15); changed=true; }
  if (jsonGetInt(body, "ek_mode",    v)) { config.Einkanal_mode              = v;                   changed=true; }
  if (jsonGetInt(body, "modul_adr",  v)) { config.modul_adress               = constrain(v,0,20);   changed=true; }
  if (jsonGetInt(body, "hw_config",  v)) { config.Hardware_Config            = constrain(v,0,3);    changed=true;
    Serial.printf("[API] Hardware_Config -> %d\n", config.Hardware_Config); }
  if (jsonGetInt(body, "pwm_min",    v)) { config.PWM_scale_min              = constrain(v,0,4095); changed=true; }
  if (jsonGetInt(body, "pwm_max",    v)) { config.PWM_scale_max              = constrain(v,0,4095); changed=true; }

  // WiFi
  { char tmp[64];
    if (jsonGetStr(body, "ssid",   config.WiFi_SSID,     sizeof(config.WiFi_SSID)))     changed=true;
    if (jsonGetStr(body, "pass",   config.WiFi_Password, sizeof(config.WiFi_Password))) changed=true;
    if (jsonGetStr(body, "ip",     config.WiFi_IP,       sizeof(config.WiFi_IP)))       changed=true;
    if (jsonGetStr(body, "device", config.Device_Name,   sizeof(config.Device_Name)))   changed=true;
    (void)tmp;
  }

  // S.Port Poll-IDs (Hex-String "A1" → uint8_t 0xA1)
  { char tmp[8];
    if (jsonGetStr(body, "poll_id0", tmp, sizeof(tmp)))
      { config.sport_poll_id[0]=(uint8_t)strtol(tmp,nullptr,16); changed=true; }
    if (jsonGetStr(body, "poll_id1", tmp, sizeof(tmp)))
      { config.sport_poll_id[1]=(uint8_t)strtol(tmp,nullptr,16); changed=true; }
  }

  if (changed) { markDirty(); saveConfigForce(); }
  sendOk(server);
}

// ── GET /api/config ───────────────────────────────────────────────────────
// Alle Konfigurationswerte als JSON.
// Felder: motor{}, sounds[], settings{}, sport{}, wifi{}, version
void WebServerManager::handleApiConfig() {
  String j = "{";

  // Motor (Sound 0)
  j += "\"motor\":{";
  j += "\"source\":"     + String(config.Source_Start_Sound[0]) + ",";
  j += "\"speed_src\":"  + String(config.Source_Speed_Sound_0)  + ",";
  j += "\"vol\":"        + String(config.Volumen_Sound[0])       + ",";
  j += "\"mode\":"       + String(config.throttle_mode)          + ",";
  j += "\"toggle\":"     + String(config.engine_on_toggle)       + ",";
  j += "\"rpm_min\":"    + String(config.Min_Speed_Sound_0)      + ",";
  j += "\"rpm_max\":"    + String(config.Max_Speed_Sound_0)      + ",";
  j += "\"shutdown_s\":" + String(config.shutdowndelay)          + ",";
  j += "\"ramp\":"       + String(config.throttle_ramp)          + ",";
  j += "\"deadband\":"   + String(config.throttle_dead_band)     + "},";

  // Sounds 1-8
  j += "\"sounds\":[";
  for (int i = 1; i <= 8; i++) {
    if (i > 1) j += ",";
    j += "{\"id\":"     + String(i)                            + ",";
    j += "\"source\":"  + String(config.Source_Start_Sound[i]) + ",";
    j += "\"vol\":"     + String(config.Volumen_Sound[i])       + ",";
    j += "\"mode\":"    + String(config.Mode_Sound[i])          + "}";
  }
  j += "],";

  // Einstellungen
  j += "\"settings\":{";
  j += "\"hw_config\":"  + String(config.Hardware_Config)         + ",";
  j += "\"rc_system\":"  + String(config.Einkanal_RC_System)      + ",";
  j += "\"ek_channel\":" + String(config.Einkanal_Channel)        + ",";
  j += "\"ek_mode\":"    + String(config.Einkanal_mode)           + ",";
  j += "\"modul_adr\":"  + String(config.modul_adress)            + ",";
  j += "\"pwm_min\":"    + String(config.PWM_scale_min)           + ",";
  j += "\"pwm_max\":"    + String(config.PWM_scale_max)           + "},";

  // S.Port Poll-IDs
  char p0[4], p1[4];
  snprintf(p0, sizeof(p0), "%02X", config.sport_poll_id[0]);
  snprintf(p1, sizeof(p1), "%02X", config.sport_poll_id[1]);
  j += "\"sport\":{\"poll_id0\":\"" + String(p0) + "\",\"poll_id1\":\"" + String(p1) + "\"},";

  // WiFi
  j += "\"wifi\":{";
  j += "\"pass\":\""   + String(config.WiFi_Password)+ "\",";
  j += "\"ssid\":\""   + String(config.WiFi_SSID)   + "\",";
  j += "\"ip\":\""     + String(config.WiFi_IP)     + "\",";
  j += "\"device\":\"" + String(config.Device_Name) + "\"},";

  j += "\"version\":\"" + String(versionString) + "\"";
  j += "}";
  sendJson(server, j);
}

// ── GET /api/debug ────────────────────────────────────────────────────────
// Live-Daten: BUS-Kanäle, PWM, WAV-Status, S.Port.
void WebServerManager::handleApiDebug() {
  String j = "{";

  // BUS-Kanäle 1-16
  j += "\"channels\":[";
  for (int i = 0; i < 16; i++) { if (i) j += ","; j += String(channel_output[i]); }
  j += "],";

  // PWM Pins 1-6
  j += "\"pwm\":[";
  for (int i = 0; i < 6; i++) {
    if (i) j += ",";
    j += "{\"us\":"  + String(PWM_pulse_width[i]) + ",";
    j += "\"pct\":"  + String(map(PWM_pulse_width[i],
                                   config.PWM_scale_min,
                                   config.PWM_scale_max, 0, 100)) + "}";
  }
  j += "],";

  // WAV-Dateistatus
  j += "\"wav\":{";
  j += "\"loop\":"  + String(Sound_loop.FileOK)  + ",";
  j += "\"shut\":"  + String(Sound_shut.FileOK)  + ",";
  j += "\"start\":" + String(Sound_start.FileOK) + ",";
  j += "\"s\":[";
  int fok[8] = {
    Sound1.FileOK, Sound2.FileOK, Sound3.FileOK, Sound4.FileOK,
    Sound5.FileOK, Sound6.FileOK, Sound7.FileOK, Sound8.FileOK
  };
  for (int i = 0; i < 8; i++) { if (i) j += ","; j += String(fok[i]); }
  j += "]},";

  // S.Port Live-Daten
  j += "\"hw_config\":"  + String(config.Hardware_Config) + ",";
  j += "\"sport\":{";
  j += "\"total_v\":"  + String(sportGetTotalVoltage(), 2)      + ",";
  j += "\"min_cell\":" + String(sportGetMinCell(), 3)           + ",";
  j += "\"cells\":"    + String(sportGetTotalCells())           + ",";
  j += "\"soc\":"      + String(sportCalcSoC(sportGetMinCell()))+ ",";
  j += "\"packs\":[";
  for (uint8_t i = 0; i < 2; i++) {
    if (i) j += ",";
    j += "{\"online\":"  + String(lipoSensor[i].online ? "true" : "false") + ",";
    j += "\"cells\":"    + String(lipoSensor[i].cellCount)      + ",";
    j += "\"total\":"    + String(lipoSensor[i].totalVoltage, 2)+ ",";
    j += "\"min\":"      + String(lipoSensor[i].minCell, 3)     + ",";
    j += "\"cv\":[";
    for (uint8_t c = 0; c < 6; c++) {
      if (c) j += ",";
      j += String(lipoSensor[i].cellVoltage[c], 3);
    }
    j += "]}";
  }
  j += "]}";

  j += "}";
  sendJson(server, j);
}

// ── POST /api/sound ───────────────────────────────────────────────────────
// Sound-Test:      {"id":1,"test":true}
// Config schreiben: {"id":1,"source":70,"vol":100,"mode":0}
// Motor (id=0):    {"id":0,"speed_src":1,"motor_mode":0,"rpm_min":100,...}
void WebServerManager::handleApiSound() {
  if (!server.hasArg("plain")) { sendErr(server); return; }
  const String& body = server.arg("plain");

  int id = -1;
  if (!jsonGetInt(body, "id", id) || id < 0 || id > 8) { sendErr(server); return; }

  bool changed = false;
  int  v;

  if (body.indexOf("\"test\":true") >= 0) Sound_on_web[id] = true;

  if (jsonGetInt(body, "source", v)) { config.Source_Start_Sound[id] = v;                    changed = true; }
  if (jsonGetInt(body, "vol",    v)) { config.Volumen_Sound[id]       = constrain(v,0,200);  changed = true; }
  if (jsonGetInt(body, "mode",   v)) { config.Mode_Sound[id]          = constrain(v,0,2);    changed = true; }

  if (id == 0) {  // Motor-Parameter
    if (jsonGetInt(body, "speed_src",  v)) { config.Source_Speed_Sound_0  = v;                     changed = true; }
    if (jsonGetInt(body, "motor_mode", v)) { config.throttle_mode          = constrain(v,0,1);     changed = true; }
    if (jsonGetInt(body, "toggle",     v)) { config.engine_on_toggle       = constrain(v,0,1);     changed = true; }
    if (jsonGetInt(body, "rpm_min",    v)) { config.Min_Speed_Sound_0      = constrain(v,0,200);   changed = true; }
    if (jsonGetInt(body, "rpm_max",    v)) { config.Max_Speed_Sound_0      = constrain(v,100,600); changed = true; }
    if (jsonGetInt(body, "shutdown_s", v)) { config.shutdowndelay          = constrain(v,0,60);    changed = true; }
    if (jsonGetInt(body, "ramp",       v)) { config.throttle_ramp          = constrain(v,0,50);    changed = true; }
    if (jsonGetInt(body, "deadband",   v)) { config.throttle_dead_band     = constrain(v,0,50);    changed = true; }
  }

  if (changed) markDirty();
  sendOk(server);
}

// ── Haupt-Request-Handler ────────────────────────────────────────────────
// Alle GET-Requests landen hier (onNotFound + on("/"))
void WebServerManager::handleRequest() {
  const String uri = server.uri();

  // ── Navigation ────────────────────────────────────────────────────────
  if (uri == "/next" || server.hasArg("next"))      { Menu++; }
  if (uri == "/back" || server.hasArg("back"))      { Menu--; }
  if (server.hasArg("menu"))  { Menu = server.arg("menu").toInt(); }
  if (uri == "/Sound/on")  { Sound_on_web[Menu] = true; }
  if (uri == "/save")      { markDirty(); saveConfigForce(); }
  if (uri == "/reset")     { Reset_all(); }
  if (uri == "/setsbus")   { set_sbus(); }
  if (uri == "/setpwm")    { set_pwm(); }
  if (uri == "/setpin")    { set_pin(); }

  // ── PWM Skalierung (eigene Route /setPWM?pwmMin=X&pwmMax=Y) ──────────
  if (uri == "/setPWM") {
    if (server.hasArg("pwmMin")) { config.PWM_scale_min = server.arg("pwmMin").toInt(); markDirty(); }
    if (server.hasArg("pwmMax")) { config.PWM_scale_max = server.arg("pwmMax").toInt(); markDirty(); }
  }

  // ── Numerische Parameter (alle via /?KEY=VAL& ) ───────────────────────
  // WebServer.h: server.arg("KEY") liefert den Wert, server.hasArg() prüft Existenz
  if (server.hasArg("VolumenSound"))         { config.Volumen_Sound[Menu]          = server.arg("VolumenSound").toInt();          markDirty(); }
  if (server.hasArg("Drehzahlmin"))          { config.Min_Speed_Sound_0            = server.arg("Drehzahlmin").toInt();           markDirty(); }
  if (server.hasArg("Drehzahlmax"))          { config.Max_Speed_Sound_0            = server.arg("Drehzahlmax").toInt();           markDirty(); }
  if (server.hasArg("shutdowndelay"))        { config.shutdowndelay                = server.arg("shutdowndelay").toInt();         markDirty(); }
  if (server.hasArg("MotorMODE"))            { config.throttle_mode                = server.arg("MotorMODE").toInt();             markDirty(); }
  if (server.hasArg("MotorOnToggle"))        { config.engine_on_toggle             = server.arg("MotorOnToggle").toInt();         markDirty(); }
  if (server.hasArg("SoundON"))              { config.Source_Start_Sound[Menu]     = server.arg("SoundON").toInt();               markDirty(); }
  if (server.hasArg("SoundSPEED"))           { config.Source_Speed_Sound_0         = server.arg("SoundSPEED").toInt();            markDirty(); }
  if (server.hasArg("RCSytem"))              { config.Einkanal_RC_System           = server.arg("RCSytem").toInt();               markDirty(); }
  if (server.hasArg("SbuschannelEinkanal"))  { config.Einkanal_Channel             = server.arg("SbuschannelEinkanal").toInt();   markDirty(); }
  if (server.hasArg("MODULADRESSE"))         { config.modul_adress                 = server.arg("MODULADRESSE").toInt();          markDirty(); }
  if (server.hasArg("SportPollID0"))         { config.sport_poll_id[0]             = (uint8_t)strtol(server.arg("SportPollID0").c_str(), nullptr, 16); markDirty(); }
  if (server.hasArg("SportPollID1"))         { config.sport_poll_id[1]             = (uint8_t)strtol(server.arg("SportPollID1").c_str(), nullptr, 16); markDirty(); }
  if (server.hasArg("EinkanalMode"))         { config.Einkanal_mode                = server.arg("EinkanalMode").toInt();          markDirty(); }
  if (server.hasArg("SoundMODE"))            { config.Mode_Sound[Menu]             = server.arg("SoundMODE").toInt();             markDirty(); }
  if (server.hasArg("ThrottleRamp"))         { config.throttle_ramp                = server.arg("ThrottleRamp").toInt();          markDirty(); }
  if (server.hasArg("ThrottleDeadBand"))     { config.throttle_dead_band           = server.arg("ThrottleDeadBand").toInt();      markDirty(); }
  if (server.hasArg("HardwareConfig"))       { config.Hardware_Config              = server.arg("HardwareConfig").toInt();        markDirty(); }

  // ── String-Parameter (URL-dekodiert) ─────────────────────────────────
  if (server.hasArg("WiFi_SSID")) {
    valueString = urlDecode(server.arg("WiFi_SSID"));
    strncpy(config.WiFi_SSID, valueString.c_str(), sizeof(config.WiFi_SSID)-1);
    config.WiFi_SSID[sizeof(config.WiFi_SSID)-1] = '\0'; markDirty();
  }
  if (server.hasArg("WiFi_Password")) {
    valueString = urlDecode(server.arg("WiFi_Password"));
    strncpy(config.WiFi_Password, valueString.c_str(), sizeof(config.WiFi_Password)-1);
    config.WiFi_Password[sizeof(config.WiFi_Password)-1] = '\0'; markDirty();
  }
  if (server.hasArg("WiFi_IP")) {
    valueString = urlDecode(server.arg("WiFi_IP"));
    strncpy(config.WiFi_IP, valueString.c_str(), sizeof(config.WiFi_IP)-1);
    config.WiFi_IP[sizeof(config.WiFi_IP)-1] = '\0'; markDirty();
  }
  if (server.hasArg("Device_Name")) {
    valueString = urlDecode(server.arg("Device_Name"));
    strncpy(config.Device_Name, valueString.c_str(), sizeof(config.Device_Name)-1);
    config.Device_Name[sizeof(config.Device_Name)-1] = '\0'; markDirty();
  }

  Menu = constrain(Menu, 0, 12);

  // HTML direkt aus Flash senden – 0 Bytes RAM, sofortiger TTFB
  server.send_P(200, "text/html", RCSOUND_HTML);
}

// ── HTML-Seitenaufbau ────────────────────────────────────────────────────
