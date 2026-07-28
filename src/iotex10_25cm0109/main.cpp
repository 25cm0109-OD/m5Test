#include <M5StickCPlus.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define ssid "YOUR_WIFI_SSID"
#define password "YOUR_WIFI_PASSWORD"

#define MQTT_BROKER "public.cloud.shiftr.io"
#define MQTT_PORT 1883
#define MQTT_CLIENT_NAME "***JEC-M5-09"
#define MQTT_USER "public"
#define MQTT_PASS "public"
#define MQTT_TOPIC "m5test/09"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

double count = 1;

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

void connectMqtt(void) {
  M5.Lcd.println("Connecting to MQTT...");
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(MQTT_CLIENT_NAME, MQTT_USER, MQTT_PASS)) {
      Serial.println("connected");
      M5.Lcd.println("Connected to MQTT!");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      M5.Lcd.printf("failed, rc=%d\n", mqttClient.state());
      delay(5000);
    }
  }
}



void setup() {
  M5.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setRotation(3);

  ConnectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
}

void loop() {
  M5.update();
if (M5.BtnB.wasPressed()) {
  connectMqtt();

  String message = "count:" + String(count);
  int len = message.length() + 1;
  char result[len];
  message.toCharArray(result, len);

  mqttClient.publish(MQTT_TOPIC, result);
  Serial.printf("Publish Message! %s \n", result);

  M5.Lcd.setCursor(0, 20);
  M5.Lcd.printf("Count Send: %d", count);

}
if (M5.BtnA.wasPressed()) {

  count = count + count;
  Serial.printf("Count: %d \n", count);
  M5.Lcd.setCursor(0, 40);
  M5.Lcd.printf("Count: %d", count);

}

if (M5.BtnA.wasPressed() && M5.BtnB.wasPressed()) {
    count = 114514;
  }

}
