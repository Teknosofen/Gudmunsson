#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- Configuration ---
#define LED_PIN       D10        // D10 on Seeed XIAO ESP32C3
#define NUM_LEDS      1
#define EEPROM_SIZE   4        // R, G, B + validity marker
#define EEPROM_MARKER 0xAA     // Marks that stored colour is valid

const char* AP_SSID = "GudmunssonLED";

// --- Globals ---
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_RGB + NEO_KHZ800);
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
body{
  font-family:system-ui,-apple-system,sans-serif;
  background:#1a1a2e;color:#eee;
  display:flex;justify-content:center;align-items:center;
  min-height:100vh;padding:16px;
}
.card{
  background:#16213e;border-radius:16px;padding:32px;
  max-width:380px;width:100%;text-align:center;
  box-shadow:0 8px 32px rgba(0,0,0,.4);
}
h1{font-size:1.4rem;margin-bottom:8px}
.sub{font-size:.85rem;color:#888;margin-bottom:24px}
.preview{
  width:120px;height:120px;border-radius:50%;
  margin:0 auto 24px;border:3px solid #333;
  transition:background .15s;
}
label{display:block;font-size:.9rem;margin-bottom:8px;color:#aaa}
input[type=color]{
  -webkit-appearance:none;appearance:none;
  width:100%;height:50px;border:none;border-radius:10px;
  cursor:pointer;background:none;padding:0;
}
input[type=color]::-webkit-color-swatch-wrapper{padding:0}
input[type=color]::-webkit-color-swatch{border:none;border-radius:10px}
.btn{
  margin-top:24px;width:100%;padding:14px;
  font-size:1rem;font-weight:600;border:none;border-radius:10px;
  cursor:pointer;color:#fff;background:#0f3460;
  transition:background .2s;
}
.btn:hover{background:#1a4a8a}
.btn:active{background:#0b2545}
.msg{margin-top:12px;font-size:.85rem;min-height:1.2em}
.ok{color:#4ecca3}
.err{color:#e74c3c}
</style>
</head>
<body>
<div class="card">
  <h1>Gudmunsson LED</h1>
  <p class="sub">Choose a colour for your frame</p>
  <div class="preview" id="preview"></div>
  <label for="picker">Select colour</label>
  <input type="color" id="picker" value="#FF9600">
  <button class="btn" id="saveBtn">Save Colour</button>
  <p class="msg" id="msg"></p>
</div>
<script>
(function(){
  var picker=document.getElementById('picker');
  var preview=document.getElementById('preview');
  var btn=document.getElementById('saveBtn');
  var msg=document.getElementById('msg');

  function setPreview(hex){
    preview.style.background=hex;
    preview.style.boxShadow='0 0 40px '+hex+'88';
  }

  /* Load current colour from device */
  function loadCurrent(){
    var x=new XMLHttpRequest();
    x.open('GET','/color',true);
    x.onload=function(){
      if(x.status===200){
        try{
          var c=JSON.parse(x.responseText);
          var hex='#'+('0'+c.r.toString(16)).slice(-2)
                     +('0'+c.g.toString(16)).slice(-2)
                     +('0'+c.b.toString(16)).slice(-2);
          picker.value=hex;
          setPreview(hex);
        }catch(e){}
      }
    };
    x.send();
  }

  picker.addEventListener('input',function(){
    setPreview(picker.value);
  });

  btn.addEventListener('click',function(){
    var hex=picker.value.replace('#','');
    var r=parseInt(hex.substring(0,2),16);
    var g=parseInt(hex.substring(2,4),16);
    var b=parseInt(hex.substring(4,6),16);
    btn.disabled=true;
    btn.textContent='Saving...';
    var x=new XMLHttpRequest();
    x.open('POST','/color',true);
    x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
    x.onload=function(){
      btn.disabled=false;
      btn.textContent='Save Colour';
      if(x.status===200){
        msg.className='msg ok';
        msg.textContent='Colour saved!';
      }else{
        msg.className='msg err';
        msg.textContent='Error saving colour.';
      }
      setTimeout(function(){msg.textContent='';},2000);
    };
    x.onerror=function(){
      btn.disabled=false;
      btn.textContent='Save Colour';
      msg.className='msg err';
      msg.textContent='Connection failed.';
      setTimeout(function(){msg.textContent='';},2000);
    };
    x.send('r='+r+'&g='+g+'&b='+b);
  });

  setPreview(picker.value);
  loadCurrent();
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
  delay(2000);

  // WiFi Access Point
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  delay(500);
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
