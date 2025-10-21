#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "driver/ledc.h"

#if CONFIG_IDF_TARGET_ESP32C3
  #define LED_BUILTIN 8  // Built-in LED on ESP32-C3
#endif

// ---------- WIFI AP MODE ----------
const char *ssidWifiAP = "ESP32";
const char *passwordWifiAP = "12345678";
IPAddress ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// ---------- OTA UPDATE ----------
const char *updateHost = "esp32";
const char *updateUsername = "admin";
const char *updatePassword = "admin";

// ---------- FAN CONTROL ----------
const int hallSensorPin = 3;  // GPIO3 for ESP32-C3
const int fanPWMPin = 4;      // GPIO4 for ESP32-C3
volatile unsigned long pulseCount = 0;
unsigned long previousMillis = 0;
const long interval = 1000;
int speed = 66;
float currentRPM = 0;

// PWM config cho ESP32
const int pwmChannel = 0;
const int pwmFreq = 25000;
const int pwmResolution = 8;

// ---------- WEB ----------
WebServer httpServer(80);

const char *indexPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <title>ESP32 Fan Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style>
      body { font-family: system-ui; text-align:center; }
      input[type=range] { width:80%; }
    </style>
  </head>
  <body>
    <h1>Fan Speed: <span id="rpm">0</span> RPM</h1>
    <input type="range" id="slider" min="0" max="100" value="0" onchange="setSpeed(this.value)">
    <p><span id="percent">0</span>%</p>
    <script>
      async function refresh() {
        let r = await fetch("/getspeed");
        let t = await r.text();
        let [rpm, s] = t.split(",");
        document.querySelector("#rpm").innerText = rpm;
        document.querySelector("#percent").innerText = s;
        document.querySelector("#slider").value = s;
        setTimeout(refresh, 1000);
      }
      async function setSpeed(v){ await fetch("/setspeed?speed="+v); }
      window.onload = refresh;
    </script>
  </body>
</html>
)rawliteral";

// ---------- INTERRUPT ----------
void IRAM_ATTR countPulse() {
  pulseCount++;
}

// ---------- FAN SPEED ----------
void setFanSpeed(int percentage) {
  percentage = constrain(percentage, 0, 100);
  uint32_t pwmValue = map(percentage, 0, 100, 0, 255);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pwmValue);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// ---------- OTA HANDLER ----------
void handleUpdate() {
  if (!httpServer.authenticate(updateUsername, updatePassword)) {
    return httpServer.requestAuthentication();
  }
  httpServer.sendHeader("Connection", "close");
  httpServer.send(200, "text/html",
    "<form method='POST' enctype='multipart/form-data' action='/update'>"
    "<input type='file' name='update'>"
    "<input type='submit' value='Update'></form>");
}

void handleDoUpdate() {
  HTTPUpload &upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %u bytes\nRebooting...\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
  yield();
}

// ---------- WIFI ----------
void handleSetWifi() {
  if (!httpServer.hasArg("ssid")) {
    httpServer.send(400, "text/plain", "FAIL");
    return;
  }

  String ssid = httpServer.arg("ssid");
  String pass = httpServer.arg("password");

  Serial.println("Connecting to " + ssid);
  WiFi.begin(ssid.c_str(), pass.c_str());

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 10) {
    delay(1000);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    httpServer.send(200, "text/plain", WiFi.localIP().toString());
  } else {
    httpServer.send(400, "text/plain", "FAIL");
  }
}

// ---------- HTTP HANDLERS ----------
void handleRoot() { httpServer.send(200, "text/html", indexPage); }

void handleGetSpeed() {
  httpServer.send(200, "text/plain", String(currentRPM) + "," + String(speed));
}

void handleSetSpeed() {
  if (httpServer.hasArg("speed")) {
    speed = httpServer.arg("speed").toInt();
    setFanSpeed(speed);
    httpServer.send(200, "text/plain", "SUCCESS");
  } else {
    httpServer.send(400, "text/plain", "FAIL");
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Booting...");

  pinMode(hallSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(hallSensorPin), countPulse, FALLING);

  // PWM setup
  ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = pwmFreq,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {
    .gpio_num = fanPWMPin,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&ledc_channel);
  
  setFanSpeed(speed);

  // WIFI AP
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssidWifiAP, passwordWifiAP);
  WiFi.softAPConfig(ip, gateway, subnet);

  // MDNS
  if (MDNS.begin(updateHost)) {
    Serial.println("MDNS responder started");
  }

  // Web routes
  httpServer.on("/", handleRoot);
  httpServer.on("/getspeed", handleGetSpeed);
  httpServer.on("/setspeed", handleSetSpeed);
  httpServer.on("/setwifi", handleSetWifi);
  httpServer.on("/update", HTTP_GET, handleUpdate);
  httpServer.on("/update", HTTP_POST, []() {
    httpServer.sendHeader("Connection", "close");
    httpServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, handleDoUpdate);

  httpServer.begin();
  Serial.println("HTTP server started");
}

// ---------- LOOP ----------
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    float rpm = (pulseCount / 2) * (60000 / interval);
    currentRPM = rpm;
    pulseCount = 0;
  }
  httpServer.handleClient();
  MDNS.begin(updateHost);  // Update MDNS
}
