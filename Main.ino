#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ---------- EDIT THESE ----------
const char* ssid     = "******";
const char* password = "*******";
String BOT_TOKEN = "**************************";
String CHAT_ID   = "*******";
// --------------------------------

#define TRIG_PIN 5
#define ECHO_PIN 18
#define BLUE_PIN 25
#define GREEN_PIN 26
#define RED_PIN 27
#define BUTTON_PIN 33
#define BUZZER_PIN 32
#define CAM_TRIGGER_PIN 23   // wired to ESP32-CAM IO13

enum Zone { ZONE_NONE, ZONE_NEAR, ZONE_FAR };
Zone lastSent = ZONE_NONE, candidate = ZONE_NONE;
int candCount = 0;
unsigned long lastMsg = 0;
const unsigned long MSG_COOLDOWN = 10000;  // ms between distance messages

// Common-anode RGB: LOW = ON
void setRGB(bool r, bool g, bool b) {
  digitalWrite(RED_PIN, r ? LOW : HIGH);
  digitalWrite(GREEN_PIN, g ? LOW : HIGH);
  digitalWrite(BLUE_PIN, b ? LOW : HIGH);
}

float getDistance() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long d = pulseIn(ECHO_PIN, HIGH, 30000);
  if (d == 0) return -1;
  return d * 0.0343 / 2;
}

String urlEncode(const String& s) {
  String out; char buf[4];
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else { snprintf(buf, sizeof(buf), "%%%02X", c); out += buf; }
  }
  return out;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(ssid, password);
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  Serial.println(WiFi.status() == WL_CONNECTED ? "WiFi connected" : "WiFi FAILED");
}

bool sendTelegram(const String& text) {
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST("chat_id=" + CHAT_ID + "&text=" + urlEncode(text));
  http.end();
  Serial.print("Telegram code: "); Serial.println(code);
  return code == 200;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(CAM_TRIGGER_PIN, OUTPUT);
  digitalWrite(CAM_TRIGGER_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);   // buzzer is LOW-triggered
  setRGB(false, false, false);
  connectWiFi();
  Serial.println("SMART DOOR SYSTEM STARTED");
}

void loop() {
  // ---------- DOORBELL ----------
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("DOORBELL PRESSED");
    setRGB(false, true, false);                 // green
    digitalWrite(CAM_TRIGGER_PIN, HIGH);        // tell camera: take photo now
    delay(300);
    digitalWrite(CAM_TRIGGER_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);              // buzzer on
    delay(2000);
    digitalWrite(BUZZER_PIN, HIGH);             // buzzer off
    while (digitalRead(BUTTON_PIN) == LOW) delay(10);  // wait for release
  }

  // ---------- DISTANCE ----------
  float distance = getDistance();
  Zone zone;
  if (distance < 0 || distance > 150) { zone = ZONE_NONE; setRGB(false, false, false); }
  else if (distance <= 50)            { zone = ZONE_NEAR; setRGB(true, false, false); }   // red
  else                                { zone = ZONE_FAR;  setRGB(false, false, true); }   // blue

  Serial.print("Distance: ");
  if (distance < 0) Serial.println("No object"); else { Serial.print(distance); Serial.println(" cm"); }

  // Send only after 3 identical readings, only on change, with cooldown
  if (zone == candidate) candCount++; else { candidate = zone; candCount = 1; }

  if (candCount >= 3 && candidate != lastSent) {
    if (candidate == ZONE_NONE) {
      lastSent = ZONE_NONE;                     // area clear, re-arm
    } else if (lastMsg == 0 || millis() - lastMsg > MSG_COOLDOWN) {
      String msg;
      if (candidate == ZONE_NEAR)
        msg = "🔴 Someone is very NEAR the door. Distance: " + String(distance, 0) + " cm";
      else
        msg = "🔵 Someone is FAR from the door. Distance: " + String(distance, 0) + " cm";
      if (sendTelegram(msg)) { lastSent = candidate; lastMsg = millis(); }
      else lastMsg = millis() - MSG_COOLDOWN + 3000;   // retry in 3 s
    }
  }
  delay(300);
}
