#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "driver/ledc.h"

#if CONFIG_IDF_TARGET_ESP32C3
  #define LED_BUILTIN 8  // Built-in LED on ESP32-C3
#endif

WebServer httpServer(80);

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
const char *indexPage = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
  <head>
    <meta charset="utf-8" />
    <title>ESP32 Fan Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style>
      body {
        font-family: system-ui;
        text-align: center;
        background: #f0f0f0;
      }
      .knob-container {
        position: relative;
        width: 200px;
        height: 200px;
        margin: 20px auto;
      }
      .knob {
        width: 200px;
        height: 200px;
        border-radius: 50%;
        background: #000;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.1);
        position: relative;
        cursor: grab;
        user-select: none;
        -webkit-user-select: none;
        overflow: hidden;
      }
      .knob:active {
        cursor: grabbing;
      }
      .knob::before {
        content: "";
        position: absolute;
        width: 200%;
        height: 200%;
        left: 50%;
        top: 50%;
        transform: translate(-25%, -50%);
        border-radius: 50%;
        background: conic-gradient(
          from 180deg,
          #4caf50 0%,
          #8bc34a 25%,
          #2196f3 50%,
          #9c27b0 75%,
          #4caf50 100%
        );
        mask: radial-gradient(
          circle at 25% 50%,
          transparent 62%,
          black 65%,
          black 70%,
          transparent 73%
        );
        -webkit-mask: radial-gradient(
          circle at 25% 50%,
          transparent 62%,
          black 65%,
          black 70%,
          transparent 73%
        );
      }
      .indicator {
        position: absolute;
        width: 6px;
        height: 50px;
        background: #fff;
        left: 50%;
        bottom: 50%;
        transform: translateX(-50%);
        border-radius: 3px;
        transform-origin: bottom center;
        box-shadow: 0 0 10px rgba(0, 0, 0, 0.3);
      }
      .indicator::after {
        content: "";
        position: absolute;
        width: 16px;
        height: 16px;
        background: #fff;
        border-radius: 50%;
        left: 50%;
        bottom: -8px;
        transform: translateX(-50%);
        box-shadow: 0 0 10px rgba(0, 0, 0, 0.3);
      }
      .value-display {
        position: absolute;
        top: 65%;
        left: 50%;
        transform: translate(-50%, -50%);
        font-size: 2em;
        font-weight: bold;
        color: #0f0;
        text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.1);
      }
      .rpm-display {
        position: absolute;
        top: 85%;
        left: 50%;
        transform: translate(-50%, -50%);
        font-size: 1.2em;
        color: #0f0;
        text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.1);
      }
      .speed-buttons {
        margin-top: 20px;
        display: flex;
        flex-direction: column;
        gap: 10px;
        align-items: center;
      }
      .speed-buttons .row {
        display: flex;
        justify-content: center;
        gap: 10px;
      }
      .speed-buttons button {
        padding: 10px 20px;
        font-size: 1.1em;
        border: none;
        border-radius: 5px;
        background: #2196f3;
        color: white;
        cursor: pointer;
        transition: background 0.3s;
        min-width: 100px;
      }
      .speed-buttons button:hover {
        background: #1976d2;
      }
    </style>
  </head>
  <body>
    <div class="knob-container">
      <div class="knob" id="knob">
        <div class="indicator"></div>
      </div>
      <div class="value-display"><span id="percent">0</span>%</div>
      <div class="rpm-display"><span id="rpm">0</span> RPM</div>
    </div>
    <div class="speed-buttons">
      <button onclick="setSpeed(0)">0%</button>
      <button onclick="setSpeed(100)">100%</button>
      <button onclick="setSpeed(25)">25%</button>
      <button onclick="setSpeed(50)">50%</button>
      <button onclick="setSpeed(75)">75%</button>
    </div>
    <script>
      const knob = document.querySelector(".knob");
      const indicator = document.querySelector(".indicator");
      let isDragging = false;
      let startAngle = 0;
      let currentRotation = -90;
      let lastRotation = -90;

      const MIN_ROTATION = -90; // Góc bắt đầu
      const MAX_ROTATION = 180; // Góc tối đa (270 độ từ min)

      function getAngle(event, element) {
        const rect = element.getBoundingClientRect();
        const center = {
          x: rect.left + rect.width / 2,
          y: rect.top + rect.height / 2,
        };
        return (
          (Math.atan2(event.clientY - center.y, event.clientX - center.x) *
            180) /
          Math.PI
        );
      }

      function updateKnob(rotation) {
        // Giới hạn góc xoay
        rotation = Math.max(MIN_ROTATION, Math.min(MAX_ROTATION, rotation));

        // Cập nhật giao diện
        indicator.style.transform = `rotate(${rotation}deg)`;

        // Tính giá trị speed (0-100%)
        const range = MAX_ROTATION - MIN_ROTATION;
        const speed = Math.round(((rotation - MIN_ROTATION) / range) * 100);
        setSpeed(speed);

        return rotation;
      }

      knob.addEventListener("mousedown", function (e) {
        isDragging = true;
        startAngle = getAngle(e, knob) - currentRotation;
        knob.style.cursor = "grabbing";
        e.preventDefault();
      });

      document.addEventListener("mousemove", function (e) {
        if (!isDragging) return;

        let rotation = getAngle(e, knob) - startAngle;

        // Thêm "quán tính" cho cảm giác mượt mà hơn
        rotation = lastRotation + (rotation - lastRotation) * 0.7;

        currentRotation = updateKnob(rotation);
        lastRotation = currentRotation;
      });

      document.addEventListener("mouseup", function () {
        if (isDragging) {
          isDragging = false;
          knob.style.cursor = "grab";
          lastRotation = currentRotation;
        }
      });

      // Hỗ trợ touch events
      knob.addEventListener("touchstart", function (e) {
        isDragging = true;
        startAngle = getAngle(e.touches[0], knob) - currentRotation;
        e.preventDefault();
      });

      document.addEventListener("touchmove", function (e) {
        if (!isDragging) return;

        let rotation = getAngle(e.touches[0], knob) - startAngle;
        rotation = lastRotation + (rotation - lastRotation) * 0.7;

        currentRotation = updateKnob(rotation);
        lastRotation = currentRotation;

        e.preventDefault();
      });

      document.addEventListener("touchend", function () {
        if (isDragging) {
          isDragging = false;
          lastRotation = currentRotation;
        }
      });

      async function refresh() {
        try {
          let r = await fetch("/getspeed");
          let t = await r.text();
          let [rpm, s] = t.split(",");
          document.querySelector("#rpm").innerText = rpm;
          document.querySelector("#percent").innerText = s;
          // Cập nhật góc xoay của núm
          const speed = parseInt(s);
          const rotation =
            MIN_ROTATION + ((MAX_ROTATION - MIN_ROTATION) * speed) / 100;
          currentRotation = rotation;
          lastRotation = rotation;
          indicator.style.transform = `rotate(${rotation}deg)`;
        } catch (e) {
          console.log("Lỗi kết nối:", e);
        }
        setTimeout(refresh, 1000);
      }

      async function setSpeed(v) {
        v = Math.max(0, Math.min(100, v)); // Giới hạn giá trị từ 0-100
        document.querySelector("#percent").innerText = v;
        await fetch("/setspeed?speed=" + v);
      }

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
  httpServer.on("/update", HTTP_POST, [&httpServer]() {
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
