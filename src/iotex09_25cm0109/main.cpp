#include <M5StickCPlus.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <IOXhop_FirebaseStream.h>
#include <IOXhop_FirebaseESP32.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

const char* firebase_url = "https://iotuploadprj99-340ae-default-rtdb.firebaseio.com/";

float count = 1;






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
  M5.Lcd.setTextSize(1);
  M5.Lcd.setRotation(3);

  ConnectWiFi();

  M5.Lcd.println(ssid);
  M5.Lcd.println(WiFi.localIP());

  Firebase.begin(firebase_url);
}

void loop() {
  M5.update();
  if (M5.BtnB.wasPressed()) {
  count = count + count;

  }
if (M5.BtnA.wasPressed()) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("Count Send: %f\n", count);

    Firebase.setFloat("IoT/counter9", count);
    Serial.println("FirebaseSend Done");
}



}
