#include "esp_camera.h"
#include "img_converters.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ---------- EDIT THESE ----------
const char* ssid     = "*******";
const char* password = "************";
String BOT_TOKEN = "****************************";
String CHAT_ID   = "*********";
// --------------------------------

#define TRIGGER_PIN 13   // wired to main ESP32 D23

// AI Thinker pins
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

unsigned long lastShot = 0;

bool initCamera() {
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM; c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM; c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM; c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM; c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 10000000;            // RHYX M21-45
  c.pixel_format = PIXFORMAT_RGB565;    // converted to JPEG before sending
  c.frame_size = FRAMESIZE_QVGA;        // 320x240
  c.jpeg_quality = 12;
  c.fb_count = 1;
  c.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) { Serial.printf("Camera init failed 0x%x\n", err); return false; }
  return true;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(ssid, password);
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  Serial.println(WiFi.status() == WL_CONNECTED ? "WiFi connected" : "WiFi FAILED");
}

bool sendPhoto(uint8_t* buf, size_t len, const String& caption) {
  WiFiClientSecure client;
  client.setInsecure();
  if (!client.connect("api.telegram.org", 443)) { Serial.println("Connect failed"); return false; }

  String b = "----ESP32CAMBOUNDARY";
  String head = "--" + b + "\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + CHAT_ID + "\r\n"
              + "--" + b + "\r\nContent-Disposition: form-data; name=\"caption\"\r\n\r\n" + caption + "\r\n"
              + "--" + b + "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"visitor.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + b + "--\r\n";
  size_t total = head.length() + len + tail.length();

  client.println("POST /bot" + BOT_TOKEN + "/sendPhoto HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Content-Length: " + String(total));
  client.println("Content-Type: multipart/form-data; boundary=" + b);
  client.println("Connection: close");
  client.println();
  client.print(head);
  for (size_t i = 0; i < len; i += 1024) {
    size_t n = (len - i < 1024) ? (len - i) : 1024;
    client.write(buf + i, n);
  }
  client.print(tail);

  String resp; unsigned long t = millis();
  while (millis() - t < 10000 && (client.connected() || client.available())) {
    while (client.available()) resp += (char)client.read();
    delay(10);
  }
  client.stop();
  return resp.indexOf("\"ok\":true") >= 0;
}

void takeAndSend() {
  Serial.println("Trigger received - taking photo");
  camera_fb_t* fb = esp_camera_fb_get();     // flush old frame
  if (fb) esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();
  if (!fb) { Serial.println("Capture failed"); return; }

  uint8_t* jpg = NULL; size_t jpgLen = 0;
  bool ok = frame2jpg(fb, 80, &jpg, &jpgLen);
  esp_camera_fb_return(fb);
  if (!ok) { Serial.println("JPEG convert failed"); return; }

  connectWiFi();
  bool sent = (WiFi.status() == WL_CONNECTED) && sendPhoto(jpg, jpgLen, "🔔 Doorbell pressed - visitor at the door");
  Serial.println(sent ? "Photo sent to Telegram" : "Photo send FAILED");
  free(jpg);
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, INPUT_PULLDOWN);
  if (!initCamera()) { Serial.println("CAMERA FAILED"); return; }
  connectWiFi();
  Serial.println("Camera ready, waiting for doorbell");
}

void loop() {
  if (digitalRead(TRIGGER_PIN) == HIGH && millis() - lastShot > 5000) {
    lastShot = millis();
    takeAndSend();
  }
  delay(20);
}
