// Weboberfläche für Einrichtung und spätere Konfiguration (eine Seite, Daten per JSON-API).
// Geheimnisse werden nie an den Browser zurückgegeben; leere Geheimnis-Felder bedeuten „unverändert“.
#pragma once

#include <pgmspace.h>

static const char kPortalPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HomeFlightTracker</title>
<style>
:root{--bg:#f4f6f5;--card:#fff;--line:#c9d3cf;--text:#1d2622;--muted:#6a7a74;--acc:#2a6f99;--ok:#2e8b57;--err:#c0392b;--head:#0b1820}
@media (prefers-color-scheme:dark){:root{--bg:#0e1418;--card:#162026;--line:#2c3a40;--text:#e3ebe7;--muted:#8fa39b;--acc:#4a9fd0;--ok:#3ddc84;--err:#ff6b5b}}
*{box-sizing:border-box}body{margin:0;font:15px/1.4 -apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;background:var(--bg);color:var(--text)}
header{background:var(--head);color:#fff;padding:16px 20px}header b{font-size:18px}header div{color:#8fb3a0;font-size:13px}
main{max-width:520px;margin:0 auto;padding:12px 16px 40px}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px 16px;margin:14px 0}
h2{font-size:15px;margin:0 0 10px}label{display:block;font-size:13px;color:var(--muted);margin:10px 0 4px}
input,select{width:100%;padding:10px;border:1px solid var(--line);border-radius:6px;font-size:15px;background:var(--card);color:var(--text)}
.row{display:flex;gap:8px}.row>*{flex:1}
button{padding:10px 14px;border-radius:6px;border:1px solid var(--acc);background:transparent;color:var(--acc);font-size:14px;cursor:pointer}
button.primary{width:100%;background:var(--ok);border-color:var(--ok);color:#fff;font-size:16px;padding:14px;margin-top:6px}
button:disabled{opacity:.5}
.seg{display:flex;gap:6px}.seg button{flex:1}.seg button.on{background:var(--acc);color:#fff}
.hint{font-size:12.5px;color:var(--muted);margin-top:6px}.ok{color:var(--ok)}.err{color:var(--err)}
.box{margin-top:10px;padding:10px;border-radius:6px;border:1px solid var(--line);font-size:13px}
.status{font-size:13px;color:var(--muted)}
</style></head><body>
<header><b>HomeFlightTracker</b><div id="mode">Konfiguration</div></header>
<main>
<div class="status" id="status"></div>

<section><h2>1 · WLAN</h2>
<label for="ssid">Netzwerk</label>
<div class="row"><input id="ssid" list="ssids" maxlength="32" autocomplete="off"><button id="scan" type="button" style="flex:0 0 auto">Suchen</button></div>
<datalist id="ssids"></datalist>
<label for="wpass">Passwort</label>
<input id="wpass" type="password" maxlength="63" autocomplete="new-password">
<label style="display:flex;gap:8px;align-items:center;color:var(--text)"><input id="wopen" type="checkbox" style="width:auto"> offenes Netz ohne Passwort</label>
<div id="testrow"><button id="wtest" type="button">Verbindung testen</button> <span id="wtestmsg" class="hint"></span></div>
</section>

<section><h2>2 · Radarstandort</h2>
<div class="row"><div><label for="lat">Breite (−90 … 90)</label><input id="lat" inputmode="decimal" placeholder="z. B. 52.3100"></div>
<div><label for="lon">Länge (−180 … 180)</label><input id="lon" inputmode="decimal" placeholder="z. B. 13.1000"></div></div>
<label for="lname">Bezeichnung (optional)</label><input id="lname" maxlength="47" placeholder="z. B. Zuhause">
<div class="hint">Dezimalgrad mit Punkt. Koordinaten z. B. per Langdruck in einer Karten-App ermitteln.</div>
</section>

<section><h2>3 · OpenSky-Zugang</h2>
<label for="cid">Client-ID</label><input id="cid" maxlength="95" autocomplete="off">
<label for="csec">Client-Secret</label><input id="csec" type="password" maxlength="127" autocomplete="new-password">
<label style="display:flex;gap:8px;align-items:center;color:var(--text)"><input id="cclear" type="checkbox" style="width:auto"> Zugang entfernen (anonymer Betrieb)</label>
<div id="otestrow"><button id="otest" type="button">Zugang testen</button> <span id="otestmsg" class="hint"></span></div>
<div class="hint">API-Client im OpenSky-Konto anlegen. Ohne Zugang nur stark gedrosselter anonymer Betrieb.</div>
</section>

<section><h2>4 · Radar</h2>
<label>Reichweite</label><div class="seg" id="ranges"></div>
<label for="intv">Abrufintervall (Sekunden)</label><input id="intv" type="number" min="30" max="900" step="5">
<div class="box" id="quota">–</div>
</section>

<section><h2>5 · Geräte-Passwort</h2>
<label for="web">Passwort für diese Weboberfläche im Heimnetz (Benutzer: admin)</label>
<input id="web" type="password" maxlength="63" autocomplete="new-password">
<div class="hint" id="webhint">Mindestens 8 Zeichen.</div>
</section>

<button class="primary" id="save" type="button">Speichern &amp; neu starten</button>
<div id="savemsg" class="hint" style="text-align:center"></div>
<p class="hint" style="text-align:center">Geheimnisse werden nach dem Speichern nie wieder angezeigt.<br>Flugdaten: OpenSky Network (opensky-network.org)</p>
</main>
<script>
const $=id=>document.getElementById(id);let st={},range=50;
const H={'Content-Type':'application/json','X-HFT':'1'};
async function api(path,body){const r=await fetch(path,body===undefined?{}:{method:'POST',headers:H,body:JSON.stringify(body)});const j=await r.json().catch(()=>({}));if(!r.ok&&!j.error)j.error='HTTP '+r.status;return j}
function setRange(km){range=km;[...$('ranges').children].forEach(b=>b.classList.toggle('on',+b.dataset.km===km));estimate()}
async function load(){
 st=await api('/api/state');
 $('mode').textContent=st.mode==='setup'?'Einrichtung (Setup-Modus)':'Konfiguration im Heimnetz';
 $('status').textContent='Firmware '+st.version+(st.wifi.connected?' · WLAN verbunden ('+st.wifi.ip+')':'');
 $('ssid').value=st.wifi.ssid||'';
 $('wpass').placeholder=st.wifi.hasPassword?'gespeichert – leer lassen = unverändert':'';
 $('testrow').style.display=st.mode==='setup'?'':'none';
 $('otestrow').style.display=st.mode==='lan'?'':'none';
 if(st.location.set){$('lat').value=st.location.lat.toFixed(5);$('lon').value=st.location.lon.toFixed(5)}
 $('lname').value=st.location.name||'';
 $('cid').value=st.opensky.clientId||'';
 $('csec').placeholder=st.opensky.hasSecret?'gespeichert – leer lassen = unverändert':'';
 $('web').placeholder=st.hasWebPassword?'gespeichert – leer lassen = unverändert':'erforderlich';
 $('ranges').innerHTML='';st.ranges.forEach(km=>{const b=document.createElement('button');b.type='button';b.textContent=km+' km';b.dataset.km=km;b.onclick=()=>setRange(km);$('ranges').appendChild(b)});
 $('intv').value=st.intervalS;setRange(st.rangeKm);
}
function authenticated(){if($('cclear').checked)return false;return !!($('cid').value.trim()&&($('csec').value||st.opensky.hasSecret))}
let et;function estimate(){clearTimeout(et);et=setTimeout(async()=>{
 const lat=parseFloat($('lat').value),lon=parseFloat($('lon').value);
 if(isNaN(lat)||isNaN(lon)){$('quota').textContent='Für die Credit-Schätzung Standort eingeben.';return}
 const q=await api('/api/estimate?lat='+lat+'&lon='+lon+'&range='+range+'&interval='+($('intv').value||0)+'&auth='+(authenticated()?1:0));
 if(q.error){$('quota').innerHTML='<span class="err">'+q.error+'</span>';return}
 $('quota').innerHTML=(q.allowed?'<span class="ok">':'<span class="err">')+'≈ '+q.dailyCredits.toLocaleString('de-DE')+' Credits pro Tag ('+q.percent+' % von '+q.budget.toLocaleString('de-DE')+')</span><br>'+
  'Abfragefläche '+q.area.toFixed(1)+' Quadratgrad → '+q.creditsPerRequest+' Credit(s) je Abruf<br>Kürzestes zulässiges Intervall: '+q.minInterval+' s'+(authenticated()?'':' (anonym)');
},250)}
['lat','lon','intv','cid','csec','cclear'].forEach(id=>$(id).addEventListener('input',estimate));
$('scan').onclick=async()=>{$('scan').disabled=true;$('scan').textContent='…';
 for(let i=0;i<30;i++){const r=await api('/api/scan');if(r.state==='done'){$('ssids').innerHTML='';r.networks.forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.label=n.rssi+' dBm'+(n.secure?'':' · offen');$('ssids').appendChild(o)});$('ssid').focus();break}await new Promise(r=>setTimeout(r,700))}
 $('scan').disabled=false;$('scan').textContent='Suchen'};
$('wtest').onclick=async()=>{$('wtestmsg').className='hint';$('wtestmsg').textContent='Teste … (bis 20 s)';
 const s=await api('/api/wifi/test',{ssid:$('ssid').value.trim(),password:$('wpass').value,open:$('wopen').checked});
 if(s.error){$('wtestmsg').className='hint err';$('wtestmsg').textContent=s.error;return}
 for(let i=0;i<40;i++){await new Promise(r=>setTimeout(r,1000));const r=await api('/api/wifi/test');
  if(r.state!=='running'){$('wtestmsg').className='hint '+(r.state==='ok'?'ok':'err');$('wtestmsg').textContent=(r.state==='ok'?'✓ ':'✗ ')+r.message;return}}};
$('otest').onclick=async()=>{$('otestmsg').className='hint';$('otestmsg').textContent='Teste …';
 const s=await api('/api/opensky/test',{clientId:$('cid').value.trim(),clientSecret:$('csec').value});
 if(s.error){$('otestmsg').className='hint err';$('otestmsg').textContent=s.error;return}
 for(let i=0;i<30;i++){await new Promise(r=>setTimeout(r,1000));const r=await api('/api/opensky/test');
  if(r.state!=='running'){$('otestmsg').className='hint '+(r.state==='ok'?'ok':'err');$('otestmsg').textContent=(r.state==='ok'?'✓ ':'✗ ')+r.message;return}}};
$('save').onclick=async()=>{const lat=parseFloat($('lat').value),lon=parseFloat($('lon').value);
 const body={wifiSsid:$('ssid').value.trim(),wifiPassword:$('wpass').value,wifiOpen:$('wopen').checked,lat:isNaN(lat)?null:lat,lon:isNaN(lon)?null:lon,
  locationName:$('lname').value.trim(),clientId:$('cid').value.trim(),clientSecret:$('csec').value,clearOpensky:$('cclear').checked,
  rangeKm:range,intervalS:parseInt($('intv').value,10)||0,webPassword:$('web').value};
 $('save').disabled=true;const r=await api('/api/config',body);
 if(r.ok){$('savemsg').className='hint ok';$('savemsg').textContent='Gespeichert. Das Gerät startet neu.'+(st.mode==='setup'?' Das Smartphone wieder mit dem Heimnetz verbinden.':'')}
 else{$('save').disabled=false;$('savemsg').className='hint err';$('savemsg').textContent=r.error||'Speichern fehlgeschlagen'}};
load().catch(e=>{$('status').textContent='Fehler beim Laden: '+e});
</script></body></html>)HTML";
