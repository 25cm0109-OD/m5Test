#include <M5StickCPlus.h>
#include <WiFi.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";


void ConnectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
    M5.Lcd.print(".");
  }
  Serial.println("Connected to WiFi");
  M5.Lcd.println("Connected to WiFi!");
}

void setup() {
  M5.begin();
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  M5.Lcd.setTextSize(2);
  M5.Lcd.setRotation(3);


  M5.Lcd.println(ssid);
  ConnectWiFi();

  M5.Lcd.println(WiFi.localIP());
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    WiFi.disconnect();
    Serial.println("Disconnected");
    M5.Lcd.println("Disconnected");
  }
  if (M5.BtnB.wasPressed()) {
    ConnectWiFi();
  }


}
