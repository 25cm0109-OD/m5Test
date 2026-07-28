#include <M5StickCPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
// const char* spread =
// "https://script.google.com/macros/s/AKfycbymzVUSa-Vj_G6e3RJTuyY2e4j3q3xmbwr8RSQmT4eRjBkw0lGT-q2ZiIVTJSqMWXld1Q/exec ";

const String url = "https://app-cm-jec.lolipop.io/iot/test.php?";
const String parameter = "age=20";


HTTPClient http;

void sendData() {
  StaticJsonDocument<255> json_request;
  char buffer[255];

  json_request["studentno"] = "25cm0109";
  json_request["data1"] = "19";
  serializeJson(json_request, buffer, sizeof(buffer));
  serializeJson(json_request, Serial);
  Serial.println();
// http.begin(spread);
http.addHeader("Content-Type", "application/json");
// POST送信
int status_code = http.POST((uint8_t*)buffer, strlen(buffer));
Serial.println(status_code);
if ( status_code > 0 ) {
  if ( status_code == HTTP_CODE_FOUND ) {
    String payload = http.getString();
    Serial.println(payload);
  }
} else {
  Serial.printf("error %s\n", http.errorToString(status_code).c_str());
}
http.end();
}





void ConnectWiFi() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
    M5.Lcd.print(".");
  }
  Serial.println("Connected to WiFi");
  M5.Lcd.println("Connected to WiFi!");

  //
  int httpResponseCode = http.GET();
  M5.Lcd.println(http.getLocation());

}


void setup() {
  M5.begin();


  Serial.begin(115200);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setRotation(3);

  ConnectWiFi();
  http.begin(url + parameter);

  M5.Lcd.println(ssid);
  M5.Lcd.println(WiFi.localIP());
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    M5.Lcd.println("sending data...");
    sendData();
  }
  if (M5.BtnB.wasPressed()) {
    WiFi.disconnect();
    Serial.println("Disconnected");
    M5.Lcd.println("Disconnected");
  }
  delay(100);


}
