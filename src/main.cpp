#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_wifi.h"

// --- Configuration ---
#define LED_PIN       D10        // D10 on Seeed XIAO ESP32C3
#define NUM_LEDS      1
#define EEPROM_SIZE   4        // R, G, B + validity marker
#define EEPROM_MARKER 0xAA     // Marks that stored colour is valid

const char* AP_SSID = "GudmunssonLED";

// --- Globals ---
// Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

uint8_t currentR = 255;
uint8_t currentG = 150;
uint8_t currentB = 0;

// --- EEPROM helpers ---
void saveColour(uint8_t r, uint8_t g, uint8_t b) {
  EEPROM.write(0, EEPROM_MARKER);
  EEPROM.write(1, r);
  EEPROM.write(2, g);
  EEPROM.write(3, b);
  EEPROM.commit();
}

void loadColour() {
  if (EEPROM.read(0) == EEPROM_MARKER) {
    currentR = EEPROM.read(1);
    currentG = EEPROM.read(2);
    currentB = EEPROM.read(3);
  }
}

// --- LED helper ---
void applyColour() {
  strip.setPixelColor(0, strip.Color(currentR, currentG, currentB));
  strip.show();
}

// --- Embedded web page ---
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Gudmunsson LED</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#eee;display:flex;justify-content:center;padding:16px}
.card{background:#16213e;border-radius:16px;padding:20px;max-width:340px;width:100%;box-shadow:0 8px 32px rgba(0,0,0,.4)}
h1{font-size:1.25rem;text-align:center;margin-bottom:14px}
canvas{display:block;width:100%;border-radius:8px;touch-action:none;cursor:crosshair}
#hue{border-radius:6px;margin-top:8px;height:20px;cursor:pointer}
.sliders{margin-top:14px}
.row{display:flex;align-items:center;gap:8px;margin-bottom:7px}
.lbl{width:12px;font-size:.85rem;font-weight:700}
.row input[type=range]{flex:1;accent-color:var(--ac);cursor:pointer}
.num{width:28px;font-size:.8rem;text-align:right;color:#aaa}
#preview{width:52px;height:52px;border-radius:50%;margin:14px auto 0;border:3px solid #333;transition:background .08s}
#msg{text-align:center;margin-top:6px;font-size:.78rem;min-height:1em;color:#e74c3c}
</style>
</head>
<body>
<div class="card">
  <h1>Gudmunsson LED</h1>
  <canvas id="matrix" height="200"></canvas>
  <canvas id="hue" height="20"></canvas>
  <div class="sliders">
    <div class="row"><span class="lbl" style="color:#f88">R</span><input type="range" id="rs" min="0" max="255" style="--ac:#f44"><span class="num" id="rv">0</span></div>
    <div class="row"><span class="lbl" style="color:#8f8">G</span><input type="range" id="gs" min="0" max="255" style="--ac:#4c4"><span class="num" id="gv">0</span></div>
    <div class="row"><span class="lbl" style="color:#88f">B</span><input type="range" id="bs" min="0" max="255" style="--ac:#44f"><span class="num" id="bv">0</span></div>
  </div>
  <div id="preview"></div>
  <p id="msg"></p>
</div>
<script>
(function(){
  var mc=document.getElementById('matrix'),
      hc=document.getElementById('hue'),
      rs=document.getElementById('rs'),gs=document.getElementById('gs'),bs=document.getElementById('bs'),
      rv=document.getElementById('rv'),gv=document.getElementById('gv'),bv=document.getElementById('bv'),
      prev=document.getElementById('preview'),msg=document.getElementById('msg');
  var H=30,S=1,V=1,drag=null,lastSent=0,sendTimer=null,pR,pG,pB;

  function hsv2rgb(h,s,v){
    function f(n){var k=(n+h/60)%6;return v-v*s*Math.max(Math.min(k,4-k,1),0);}
    return[Math.round(f(5)*255),Math.round(f(3)*255),Math.round(f(1)*255)];
  }
  function rgb2hsv(r,g,b){
    r/=255;g/=255;b/=255;
    var mx=Math.max(r,g,b),mn=Math.min(r,g,b),d=mx-mn,h=0,s=mx?d/mx:0,v=mx;
    if(d){if(mx===r)h=((g-b)/d+(g<b?6:0))*60;else if(mx===g)h=((b-r)/d+2)*60;else h=((r-g)/d+4)*60;}
    return[h,s,v];
  }

  function drawMatrix(){
    var ctx=mc.getContext('2d'),w=mc.width,h=mc.height;
    var sg=ctx.createLinearGradient(0,0,w,0);
    sg.addColorStop(0,'#fff');sg.addColorStop(1,'hsl('+H+',100%,50%)');
    ctx.fillStyle=sg;ctx.fillRect(0,0,w,h);
    var vg=ctx.createLinearGradient(0,0,0,h);
    vg.addColorStop(0,'rgba(0,0,0,0)');vg.addColorStop(1,'#000');
    ctx.fillStyle=vg;ctx.fillRect(0,0,w,h);
    var cx=S*w,cy=(1-V)*h;
    ctx.beginPath();ctx.arc(cx,cy,7,0,2*Math.PI);
    ctx.strokeStyle=V>0.4?'#000':'#fff';ctx.lineWidth=2;ctx.stroke();
  }
  function drawHue(){
    var ctx=hc.getContext('2d'),w=hc.width,h=hc.height;
    var g=ctx.createLinearGradient(0,0,w,0);
    for(var i=0;i<=12;i++)g.addColorStop(i/12,'hsl('+(i*30)+',100%,50%)');
    ctx.fillStyle=g;ctx.fillRect(0,0,w,h);
    var x=(H/360)*w;
    ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,h);
    ctx.strokeStyle='#fff';ctx.lineWidth=2;ctx.stroke();
  }

  function applyRGB(r,g,b){
    rv.textContent=r;gv.textContent=g;bv.textContent=b;
    var hex='#'+('0'+r.toString(16)).slice(-2)+('0'+g.toString(16)).slice(-2)+('0'+b.toString(16)).slice(-2);
    prev.style.background=hex;prev.style.boxShadow='0 0 24px '+hex+'99';
    send(r,g,b);
  }
  function fromHSV(){
    var rgb=hsv2rgb(H,S,V);
    rs.value=rgb[0];gs.value=rgb[1];bs.value=rgb[2];
    applyRGB(rgb[0],rgb[1],rgb[2]);
    drawMatrix();drawHue();
  }
  function fromRGB(){
    var r=+rs.value,g=+gs.value,b=+bs.value;
    var hsv=rgb2hsv(r,g,b);H=hsv[0];S=hsv[1];V=hsv[2];
    applyRGB(r,g,b);
    drawMatrix();drawHue();
  }

  function send(r,g,b){
    pR=r;pG=g;pB=b;
    var now=Date.now();
    if(now-lastSent>=100){doSend(r,g,b);}
    else{clearTimeout(sendTimer);sendTimer=setTimeout(function(){doSend(pR,pG,pB);},100);}
  }
  function doSend(r,g,b){
    lastSent=Date.now();
    var x=new XMLHttpRequest();
    x.open('POST','/color',true);
    x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
    x.onload=function(){if(x.status!==200){msg.textContent='Error';setTimeout(function(){msg.textContent='';},1500);}};
    x.onerror=function(){msg.textContent='Offline';};
    x.send('r='+r+'&g='+g+'&b='+b);
  }

  function evXY(e){return e.touches?{x:e.touches[0].clientX,y:e.touches[0].clientY}:{x:e.clientX,y:e.clientY};}
  function onMatrix(e){var p=evXY(e),r=mc.getBoundingClientRect();S=Math.max(0,Math.min(1,(p.x-r.left)/r.width));V=Math.max(0,Math.min(1,1-(p.y-r.top)/r.height));}
  function onHue(e){var p=evXY(e),r=hc.getBoundingClientRect();H=Math.max(0,Math.min(360,((p.x-r.left)/r.width)*360));}

  mc.addEventListener('mousedown',function(e){drag='m';onMatrix(e);fromHSV();});
  hc.addEventListener('mousedown',function(e){drag='h';onHue(e);fromHSV();});
  document.addEventListener('mousemove',function(e){if(drag==='m'){onMatrix(e);fromHSV();}else if(drag==='h'){onHue(e);fromHSV();}});
  document.addEventListener('mouseup',function(){drag=null;});
  mc.addEventListener('touchstart',function(e){e.preventDefault();drag='m';onMatrix(e);fromHSV();},{passive:false});
  hc.addEventListener('touchstart',function(e){e.preventDefault();drag='h';onHue(e);fromHSV();},{passive:false});
  document.addEventListener('touchmove',function(e){if(drag==='m'){e.preventDefault();onMatrix(e);fromHSV();}else if(drag==='h'){e.preventDefault();onHue(e);fromHSV();}},{passive:false});
  document.addEventListener('touchend',function(){drag=null;});
  [rs,gs,bs].forEach(function(s){s.addEventListener('input',fromRGB);});

  function init(){
    mc.width=mc.offsetWidth||292;hc.width=hc.offsetWidth||292;
    var x=new XMLHttpRequest();
    x.open('GET','/color',true);
    x.onload=function(){
      if(x.status===200){try{var c=JSON.parse(x.responseText);var hsv=rgb2hsv(c.r,c.g,c.b);H=hsv[0];S=hsv[1];V=hsv[2];rs.value=c.r;gs.value=c.g;bs.value=c.b;applyRGB(c.r,c.g,c.b);}catch(e){}}
      drawMatrix();drawHue();
    };
    x.onerror=function(){drawMatrix();drawHue();};
    x.send();
  }
  init();
})();
</script>
</body>
</html>
)rawliteral";

// --- Request handlers ---
void handleRoot() {
  server.send(200, "text/html", PAGE_HTML);
}

void handleGetColour() {
  String json = "{\"r\":" + String(currentR) +
                ",\"g\":" + String(currentG) +
                ",\"b\":" + String(currentB) + "}";
  server.send(200, "application/json", json);
}

void handleSetColour() {
  if (!server.hasArg("r") || !server.hasArg("g") || !server.hasArg("b")) {
    server.send(400, "text/plain", "Missing r, g, b parameters");
    return;
  }
  currentR = (uint8_t)server.arg("r").toInt();
  currentG = (uint8_t)server.arg("g").toInt();
  currentB = (uint8_t)server.arg("b").toInt();

  applyColour();
  saveColour(currentR, currentG, currentB);

  server.send(200, "text/plain", "OK");
  Serial.printf("Colour set: R=%d G=%d B=%d\n", currentR, currentG, currentB);
}

// --- Setup & Loop ---
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brownout detector

  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Gudmunsson LED ===");

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadColour();
  Serial.printf("Loaded colour: R=%d G=%d B=%d\n", currentR, currentG, currentB);

  // NeoPixel
  strip.begin();
  strip.setBrightness(255);
  applyColour();

  // Let power rail stabilise before WiFi (draws ~200mA)
  delay(500);

  // WiFi Access Point
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, nullptr, 6);  // channel 6 — mid-band, less crowded than ch1
  delay(200);

  // Lower beacon interval so phones discover the AP faster (default 100ms → 40ms)
  wifi_config_t ap_cfg;
  esp_wifi_get_config(WIFI_IF_AP, &ap_cfg);
  ap_cfg.ap.beacon_interval = 40;
  esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
  Serial.print("AP started: ");
  Serial.println(AP_SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());

  // Web server routes
  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/color",  HTTP_GET,  handleGetColour);
  server.on("/color",  HTTP_POST, handleSetColour);
  server.begin();
  Serial.println("Web server running on port 80");
}

void loop() {
  server.handleClient();
}
