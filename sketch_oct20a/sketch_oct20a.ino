#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Preferences.h>

#if CONFIG_IDF_TARGET_ESP32C3
  #define LED_BUILTIN 8
#endif

WebServer httpServer(80);
Preferences prefs;

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
const int hallSensorPin = 3;
const int fanPWMPin = 4;
volatile unsigned long pulseCount = 0;
unsigned long previousMillis = 0;
const long interval = 1000;
int speed = 66;
float currentRPM = 0;

const int pwmFreq = 25000;
const int pwmResolution = 8;

// ---------- WEB UI ----------
const char *indexPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <title>ESP32 Fan Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1" />
  </head>
  <body style="font-family:sans-serif;text-align:center;">
    <h2>ESP32 Fan Control</h2>
    <a href="/wifi">WiFi Settings</a><br><br>
    <div>
      <p>Current Speed: <span id="percent">0</span>%</p>
      <p>RPM: <span id="rpm">0</span> RPM</p>
    </div>
    <div>
      <button onclick="setSpeed(0)">0%</button>
      <button onclick="setSpeed(25)">25%</button>
      <button onclick="setSpeed(50)">50%</button>
      <button onclick="setSpeed(75)">75%</button>
      <button onclick="setSpeed(100)">100%</button>
    </div>
    <script>
      async function refresh(){
        try {
          let r = await fetch("/getspeed");
          let t = await r.text();
          let [rpm,s] = t.split(",");
          document.querySelector("#rpm").innerText = rpm;
          document.querySelector("#percent").innerText = s;
        } catch(e){}
        setTimeout(refresh,1000);
      }
      async function setSpeed(v){
        await fetch("/setspeed?speed="+v);
      }
      window.onload=refresh;
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
  ledcWrite(fanPWMPin, pwmValue);
}

// ---------- OTA ----------
void handleUpdate() {
  if (!httpServer.authenticate(updateUsername, updatePassword)) {
    return httpServer.requestAuthentication();
  }
  httpServer.sendHeader("Connection", "close");
  httpServer.send(200, "text/html",
    "<form method='POST' enctype='multipart/form-data' action='/update'>"
    "<input type='file' name='update'><input type='submit' value='Update'></form>");
}

void handleDoUpdate() {
  HTTPUpload &upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) Serial.printf("Update Success: %u bytes\n", upload.totalSize);
    else Update.printError(Serial);
  }
  yield();
}

// ---------- WEB HANDLERS ----------
void handleRoot() { httpServer.send(200, "text/html", indexPage); }

void handleGetSpeed() {
  httpServer.send(200, "text/plain", String(currentRPM) + "," + String(speed));
}

void handleSetSpeed() {
  if (httpServer.hasArg("speed")) {
    speed = httpServer.arg("speed").toInt();
    setFanSpeed(speed);
    httpServer.send(200, "text/plain", "OK");
  } else {
    httpServer.send(400, "text/plain", "FAIL");
  }
}

// ---------- WIFI CONFIG ----------
void handleWifiPage() {
  int n = WiFi.scanNetworks();
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>WiFi Configuration</title></head><body>";
  html += "<h2>Select WiFi Network</h2><form action='/setwifi' method='get'>";
  html += "<select name='ssid'>";
  for (int i = 0; i < n; ++i) {
    html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  html += "</select><br><br>Password:<br><input name='password' type='password'><br><br>";
  html += "<input type='submit' value='Connect'></form>";
  html += "</body></html>";
  httpServer.send(200, "text/html", html);
}

void handleSetWifi() {
  if (!httpServer.hasArg("ssid")) {
    httpServer.send(400, "text/plain", "Missing SSID");
    return;
  }

  String ssid = httpServer.arg("ssid");
  String pass = httpServer.arg("password");

  // Gửi response ngay để không bị timeout
  httpServer.send(200, "text/html", 
    "<html><head><meta charset='utf-8'></head><body>"
    "<h3>Testing WiFi connection...</h3>"
    "<p>Please wait...</p>"
    "<script>setTimeout(function(){window.location.href='/wifistatus';}, 8000);</script>"
    "</body></html>");

  // Ngắt kết nối hiện tại
  WiFi.disconnect();
  delay(100);

  // Thử kết nối WiFi mới
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  Serial.println("Testing WiFi: " + ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnection successful!");
    // Lưu WiFi vào bộ nhớ
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putString("status", "success");
    prefs.end();
  } else {
    Serial.println("\nConnection failed!");
    // Lưu trạng thái lỗi
    prefs.begin("wifi", false);
    prefs.putString("status", "failed");
    prefs.end();
    
    // Quay lại AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssidWifiAP, passwordWifiAP);
    WiFi.softAPConfig(ip, gateway, subnet);
  }
}

void handleWifiStatus() {
  prefs.begin("wifi", true);
  String status = prefs.getString("status", "unknown");
  prefs.end();

  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>WiFi Status</title></head><body style='font-family:sans-serif;text-align:center;'>";
  
  if (status == "success") {
    html += "<h2 style='color:green;'>✓ WiFi Connected Successfully!</h2>";
    html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
    html += "<p>Signal Strength: " + String(WiFi.RSSI()) + " dBm</p>";
    html += "<p>Device will restart in 3 seconds...</p>";
    html += "<script>setTimeout(function(){window.location.href='/';}, 3000);</script>";
    html += "</body></html>";
    httpServer.send(200, "text/html", html);
    
    // Xóa status và restart
    delay(3000);
    prefs.begin("wifi", false);
    prefs.remove("status");
    prefs.end();
    ESP.restart();
  } else if (status == "failed") {
    html += "<h2 style='color:red;'>✗ Connection Failed!</h2>";
    html += "<p>Unable to connect to WiFi network.</p>";
    html += "<p>Please check:</p>";
    html += "<ul style='text-align:left;display:inline-block;'>";
    html += "<li>WiFi password is correct</li>";
    html += "<li>Router is powered on</li>";
    html += "<li>Signal strength is good</li>";
    html += "</ul>";
    html += "<br><a href='/wifi'><button>Try Again</button></a>";
    html += "</body></html>";
    httpServer.send(200, "text/html", html);
    
    // Xóa status
    prefs.begin("wifi", false);
    prefs.remove("status");
    prefs.end();
  } else {
    html += "<h2>Unknown Status</h2>";
    html += "<a href='/wifi'><button>Back to WiFi Settings</button></a>";
    html += "</body></html>";
    httpServer.send(200, "text/html", html);
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Booting...");

  pinMode(hallSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(hallSensorPin), countPulse, FALLING);

  // Cấu hình PWM với API mới của ESP32 Arduino Core 3.x
  ledcAttach(fanPWMPin, pwmFreq, pwmResolution);
  setFanSpeed(speed);

  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  // Đặt tên hostname cho ESP32
  WiFi.setHostname("ESP32-Fan-Control");

  if (ssid != "") {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("Connecting to saved WiFi...");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
    } else {
      Serial.println("\nConnection failed → Starting AP");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ssidWifiAP, passwordWifiAP);
      WiFi.softAPConfig(ip, gateway, subnet);
    }
  } else {
    Serial.println("No WiFi configuration → Starting AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssidWifiAP, passwordWifiAP);
    WiFi.softAPConfig(ip, gateway, subnet);
  }

  if (MDNS.begin(updateHost)) Serial.println("MDNS responder started");

  httpServer.on("/", handleRoot);
  httpServer.on("/wifi", handleWifiPage);
  httpServer.on("/setwifi", handleSetWifi);
  httpServer.on("/wifistatus", handleWifiStatus);
  httpServer.on("/getspeed", handleGetSpeed);
  httpServer.on("/setspeed", handleSetSpeed);
  httpServer.on("/update", HTTP_GET, handleUpdate);
  httpServer.on(
      "/update", HTTP_POST,
      []() {
        httpServer.sendHeader("Connection", "close");
        httpServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
      },
      handleDoUpdate);

  httpServer.begin();
  Serial.println("HTTP server started");
}

// ---------- LOOP ----------
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    float rpm = (pulseCount / 2.0) * (60000.0 / interval);
    currentRPM = rpm;
    pulseCount = 0;
  }
  httpServer.handleClient();
}