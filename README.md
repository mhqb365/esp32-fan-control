# ESP32 Fan Control - Điều khiển Quạt qua WiFi

Hệ thống điều khiển tốc độ quạt PWM thông minh sử dụng ESP32, có giao diện web và khả năng kết nối WiFi.

## 📋 Tính năng

- ✅ Điều khiển tốc độ quạt PWM (0-100%)
- ✅ Hiển thị tốc độ quay thời gian thực (RPM)
- ✅ Giao diện web responsive, dễ sử dụng
- ✅ Cấu hình WiFi qua web (không cần hard-code)
- ✅ Hỗ trợ cả chế độ AP và Station
- ✅ Cập nhật firmware OTA (Over The Air)
- ✅ Tự động phát hiện lỗi kết nối WiFi

## 🔧 Phần cứng cần thiết

### Linh kiện:
- **ESP32** (bất kỳ model nào: ESP32, ESP32-C3, ESP32-S3)
- **Quạt PWM 4-pin** (có tín hiệu tachometer)
- Nguồn phù hợp cho quạt (thường 12V)

### Sơ đồ đấu nối:

```
ESP32          →    Quạt PWM
─────────────────────────────
GPIO 4         →    PWM Pin (điều khiển tốc độ)
GPIO 3         →    Tachometer Pin (đo tốc độ)
GND            →    GND
```

**Lưu ý:** Quạt cần nguồn riêng 12V, KHÔNG nối trực tiếp vào ESP32!

## 📦 Cài đặt

### 1. Cài đặt Arduino IDE
- Tải Arduino IDE từ [arduino.cc](https://www.arduino.cc/en/software)
- Cài đặt ESP32 board package:
  - Vào **File → Preferences**
  - Thêm URL vào "Additional Board Manager URLs":
    ```
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    ```
  - Vào **Tools → Board → Boards Manager**
  - Tìm và cài "ESP32 by Espressif Systems"

### 2. Cài đặt thư viện
Các thư viện sau đã có sẵn trong ESP32 core, không cần cài thêm:
- WiFi
- WebServer
- ESPmDNS
- Update
- Preferences

### 3. Upload code
1. Mở file `.ino` trong Arduino IDE
2. Chọn board: **Tools → Board → ESP32 Arduino → ESP32 Dev Module** (hoặc model của bạn)
3. Chọn COM port: **Tools → Port → COMX**
4. Nhấn **Upload** ⬆️

## 🚀 Hướng dẫn sử dụng

### Lần đầu sử dụng (Chưa có WiFi):

1. **Bật nguồn ESP32**
   - ESP32 sẽ tự động tạo WiFi AP với tên: `ESP32`
   - Mật khẩu: `12345678`

2. **Kết nối vào WiFi của ESP32**
   - Dùng điện thoại/laptop kết nối WiFi `ESP32`
   - Nhập mật khẩu: `12345678`

3. **Truy cập giao diện web**
   - Mở trình duyệt, truy cập: `http://192.168.1.1`
   - Hoặc: `http://esp32-fan-control.local` (nếu hỗ trợ mDNS)

4. **Cấu hình WiFi**
   - Nhấn vào link **"WiFi Settings"**
   - Chọn WiFi nhà bạn từ danh sách
   - Nhập mật khẩu WiFi
   - Nhấn **"Connect"**

5. **Chờ kết nối**
   - ESP32 sẽ thử kết nối WiFi (khoảng 10 giây)
   - Nếu **thành công**: Hiển thị IP mới → Tự động khởi động lại
   - Nếu **thất bại**: Hiển thị lỗi → Nhấn "Try Again"

### Sử dụng hàng ngày:

1. **Tìm địa chỉ IP của ESP32**
   
   **Cách 1:** Dùng Serial Monitor
   - Mở Arduino IDE → **Tools → Serial Monitor**
   - Tốc độ: 115200 baud
   - Reset ESP32, xem IP hiển thị

   **Cách 2:** Dùng IP Scanner
   - Tải [Advanced IP Scanner](https://www.advanced-ip-scanner.com/)
   - Scan dải IP `192.168.1.1-254`
   - Tìm thiết bị có tên: `ESP32-Fan-Control`

   **Cách 3:** Dùng mDNS (nếu hỗ trợ)
   - Truy cập: `http://esp32-fan-control.local`

2. **Điều khiển quạt**
   - Truy cập IP của ESP32 trên trình duyệt
   - Nhấn các nút: **0%, 25%, 50%, 75%, 100%**
   - Xem tốc độ quay (RPM) cập nhật theo thời gian thực

## 🎛️ Giao diện Web

### Trang chính:
```
┌─────────────────────────────┐
│   ESP32 Fan Control         │
│   WiFi Settings             │
├─────────────────────────────┤
│  Current Speed: 66%         │
│  RPM: 1250 RPM              │
├─────────────────────────────┤
│ [0%] [25%] [50%] [75%] [100%]│
└─────────────────────────────┘
```

### Trang cấu hình WiFi:
```
┌─────────────────────────────┐
│   Select WiFi Network       │
├─────────────────────────────┤
│  ▼ [Chọn WiFi...]           │
│    - MyWiFi (-45 dBm)       │
│    - Office WiFi (-60 dBm)  │
├─────────────────────────────┤
│  Password: [.............]  │
│  [Connect]                  │
└─────────────────────────────┘
```

## 🔄 Cập nhật Firmware (OTA)

1. Truy cập: `http://[IP_ESP32]/update`
2. Đăng nhập:
   - Username: `admin`
   - Password: `admin`
3. Chọn file `.bin` đã compile
4. Nhấn **Upload**
5. Chờ ESP32 tự động khởi động lại

**Lưu ý:** Đổi username/password mặc định trong code để bảo mật hơn!

## ⚙️ Tùy chỉnh

### Đổi tên hostname:
```cpp
WiFi.setHostname("ESP32-Fan-Control");  // Đổi tên này
```

### Đổi thông tin AP mặc định:
```cpp
const char *ssidWifiAP = "ESP32";        // Tên WiFi AP
const char *passwordWifiAP = "12345678"; // Mật khẩu AP
```

### Đổi chân GPIO:
```cpp
const int hallSensorPin = 3;  // Chân đọc tachometer
const int fanPWMPin = 4;      // Chân điều khiển PWM
```

### Đổi tần số PWM:
```cpp
const int pwmFreq = 25000;    // 25kHz (tốt cho hầu hết quạt)
```

### Đổi tốc độ khởi động:
```cpp
int speed = 66;  // Tốc độ mặc định 66% khi bật nguồn
```

## 🐛 Xử lý lỗi thường gặp

### 1. ESP32 không tạo WiFi AP
- Kiểm tra đã upload code thành công chưa
- Reset ESP32 bằng nút Reset
- Kiểm tra Serial Monitor xem có lỗi gì

### 2. Không kết nối được WiFi
- Kiểm tra mật khẩu WiFi đúng chưa
- Đảm bảo router bật và trong tầm phát
- Thử xóa WiFi đã lưu (xem phần Factory Reset)

### 3. Quạt không quay
- Kiểm tra nguồn 12V cho quạt
- Kiểm tra đấu dây GPIO 4 đúng chưa
- Thử tăng tốc độ lên 100%

### 4. Không đo được RPM
- Kiểm tra đấu dây GPIO 3 (tachometer)
- Một số quạt rẻ không có tín hiệu tachometer
- Kiểm tra quạt có 4 dây không (2 dây: nguồn, 1 dây: PWM, 1 dây: tachometer)

### 5. Compile lỗi `ledcSetup not declared`
- Bạn đang dùng ESP32 Arduino Core 3.x
- Code đã được cập nhật cho Core 3.x
- Nếu vẫn lỗi, cập nhật ESP32 board package lên bản mới nhất

## 🔐 Factory Reset

Để xóa WiFi đã lưu và reset về mặc định:

**Cách 1:** Thêm code vào `setup()`:
```cpp
void setup() {
  // Xóa toàn bộ Preferences
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  
  // Code còn lại...
}
```

**Cách 2:** Upload lại code mới

## 📊 Thông số kỹ thuật

| Tham số | Giá trị |
|---------|---------|
| Vi điều khiển | ESP32 (tất cả model) |
| Điện áp hoạt động | 3.3V (ESP32), 12V (Quạt) |
| Tần số PWM | 25kHz |
| Độ phân giải PWM | 8-bit (0-255) |
| Phạm vi điều khiển | 0-100% |
| Cập nhật RPM | 1 giây/lần |
| Chế độ WiFi | AP + Station |
| Giao thức web | HTTP |

## 📝 License

Dự án này được phát hành dưới MIT License - sử dụng tự do!

## 🤝 Đóng góp

Mọi đóng góp đều được chào đón! Hãy tạo Pull Request hoặc Issue nếu bạn có ý tưởng cải tiến.

**Chúc bạn điều khiển quạt vui vẻ! 🌬️💨**