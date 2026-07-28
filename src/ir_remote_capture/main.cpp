#include <Arduino.h>
#include <M5StickCPlus.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>

#include <vector>

#include "upload_config.h"

namespace {

// -----------------------------------------------------------------------------
// Hardware configuration
// -----------------------------------------------------------------------------
constexpr uint16_t kIrReceivePin = 36;
constexpr uint16_t kIrSendPin = 26;
// Panasonic/Kaseikyo uses an atypical carrier near 36.7 kHz. sendRaw() takes
// the frequency in kHz, so 37 is the closest integer setting.
constexpr uint16_t kIrCarrierFrequencyKhz = 37;

// A larger-than-default receive buffer makes it possible to capture long air-
// conditioner messages. The timeout marks the end of an incoming IR message.
constexpr uint16_t kReceiveBufferSize = 1024;
constexpr uint8_t kReceiveTimeoutMs = 50;

// LCD layout and colours.
constexpr uint8_t kLcdRotation = 1;
constexpr uint8_t kTextSize = 2;
constexpr int16_t kStatusX = 8;
constexpr int16_t kStatusY = 8;
constexpr uint32_t kMessageDisplayMs = 700;
// Most consumer remotes repeat a held key at roughly 100--120 ms intervals.
constexpr uint32_t kTransmitRepeatPeriodMs = 110;
constexpr uint32_t kDebugRefreshMs = 250;
constexpr int16_t kDebugAreaY = 42;
constexpr int16_t kDebugAreaHeight = 80;

IRrecv irReceiver(kIrReceivePin, kReceiveBufferSize, kReceiveTimeoutMs, true);
IRsend irSender(kIrSendPin);
decode_results receiveResults;

// Counts electrical level changes from the IR receiver module. This monitor is
// independent of protocol decoding, so it also reacts to malformed/unknown IR.
volatile uint32_t irInputEdgeCount = 0;

void IRAM_ATTR onIrInputChange() {
  ++irInputEdgeCount;
}

// All captured information lives only in RAM. A vector is used because RAW
// messages have different lengths. Each entry is a mark/space time in us.
struct CapturedSignal {
  bool valid = false;
  decode_type_t protocol = UNKNOWN;
  uint16_t bits = 0;
  uint64_t value = 0;
  uint32_t address = 0;
  uint32_t command = 0;
  std::vector<uint16_t> rawData;
};

CapturedSignal capturedSignal;
uint32_t lastCommandPollMs = 0;
uint32_t pendingResultCommandId = 0;
bool pendingResultSucceeded = false;
String pendingResultError;

enum class ScreenState {
  kReady,
  kReceiving,
  kCaptured,
  kUploading,
  kUploadDone,
  kUploadFailed,
  kSending,
  kDone,
};

ScreenState currentScreen = ScreenState::kReady;

void drawScreen(const ScreenState state) {
  if (state == currentScreen && state == ScreenState::kReady) {
    return;
  }

  currentScreen = state;
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(kStatusX, kStatusY);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(kTextSize);

  switch (state) {
    case ScreenState::kReady:
      M5.Lcd.println("Ready");
      M5.Lcd.setTextSize(1);
      M5.Lcd.println();
      M5.Lcd.printf("Signal: %s\n", capturedSignal.valid ? "Saved" : "None");
      M5.Lcd.println("Hold B: Receive");
      M5.Lcd.println("Press A: Send");
      break;
    case ScreenState::kReceiving:
      M5.Lcd.println("Receiving...");
      break;
    case ScreenState::kCaptured:
      M5.Lcd.setTextColor(GREEN, BLACK);
      M5.Lcd.println("Captured!");
      break;
    case ScreenState::kUploading:
      M5.Lcd.println("Uploading...");
      break;
    case ScreenState::kUploadDone:
      M5.Lcd.setTextColor(GREEN, BLACK);
      M5.Lcd.println("Uploaded!");
      break;
    case ScreenState::kUploadFailed:
      M5.Lcd.setTextColor(RED, BLACK);
      M5.Lcd.println("Upload failed");
      break;
    case ScreenState::kSending:
      M5.Lcd.println("Sending...");
      break;
    case ScreenState::kDone:
      M5.Lcd.setTextColor(GREEN, BLACK);
      M5.Lcd.println("Done");
      break;
  }
}

void printCapturedSignalToSerial() {
  Serial.println();
  Serial.println(F("--- Captured IR signal ---"));
  Serial.print(F("Protocol: "));
  Serial.println(typeToString(capturedSignal.protocol));
  Serial.print(F("Address: 0x"));
  Serial.println(capturedSignal.address, HEX);
  Serial.print(F("Command: 0x"));
  Serial.println(capturedSignal.command, HEX);
  Serial.print(F("Bits: "));
  Serial.println(capturedSignal.bits);
  Serial.print(F("Raw Length: "));
  Serial.println(capturedSignal.rawData.size());

  // Print in a form that can be copied directly into an Arduino sketch.
  Serial.print(F("uint16_t rawData[] = {"));
  for (size_t index = 0; index < capturedSignal.rawData.size(); ++index) {
    if (index != 0) {
      Serial.print(F(", "));
    }
    Serial.print(capturedSignal.rawData[index]);
  }
  Serial.println(F("};"));
  Serial.println(F("--------------------------"));
}

void updateReceiveDebugMonitor(const uint32_t edgesSinceButtonPress) {
  const int inputLevel = digitalRead(kIrReceivePin);
  const bool electricalSignalSeen = edgesSinceButtonPress > 0;

  // Only redraw the lower part of the display so "Receiving..." remains shown.
  M5.Lcd.fillRect(0, kDebugAreaY, M5.Lcd.width(), kDebugAreaHeight, BLACK);
  M5.Lcd.setCursor(kStatusX, kDebugAreaY);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(electricalSignalSeen ? GREEN : WHITE, BLACK);
  M5.Lcd.printf("GPIO36: %s\n", inputLevel == HIGH ? "HIGH" : "LOW");
  M5.Lcd.printf("Edges: %lu\n", static_cast<unsigned long>(edgesSinceButtonPress));
  M5.Lcd.printf("IR input: %s\n", electricalSignalSeen ? "DETECTED" : "waiting");

  Serial.print(F("[IR MONITOR] GPIO36="));
  Serial.print(inputLevel == HIGH ? F("HIGH") : F("LOW"));
  Serial.print(F("  Edges="));
  Serial.print(edgesSinceButtonPress);
  Serial.print(F("  Input="));
  Serial.println(electricalSignalSeen ? F("DETECTED") : F("waiting"));
}

void saveReceivedSignal(const decode_results &results) {
  capturedSignal.protocol = results.decode_type;
  capturedSignal.bits = results.bits;
  capturedSignal.value = results.value;
  capturedSignal.address = results.address;
  capturedSignal.command = results.command;

  // resultToRawArray() converts the receiver's tick-based buffer into an array
  // of microseconds suitable for sendRaw(). It also preserves UNKNOWN signals.
  const uint16_t rawLength = getCorrectedRawLength(&results);
  uint16_t *convertedRawData = resultToRawArray(&results);

  capturedSignal.rawData.clear();
  if (convertedRawData != nullptr && rawLength > 0) {
    capturedSignal.rawData.assign(convertedRawData,
                                  convertedRawData + rawLength);
  }
  delete[] convertedRawData;

  // A decoded result without replayable RAW timings is not considered valid.
  // Normally this cannot happen, but the check avoids an unsafe empty replay.
  capturedSignal.valid = !capturedSignal.rawData.empty();
  printCapturedSignalToSerial();
}

bool connectToWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(uploadConfig::kWifiSsid, uploadConfig::kWifiPassword);
  const uint32_t startedMs = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedMs < uploadConfig::kWifiTimeoutMs) {
    M5.update();
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool maintainWifiConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  // Background polling must not block the physical A/B buttons for the full
  // connection timeout when the access point is temporarily unavailable.
  static uint32_t lastReconnectAttemptMs = 0;
  const uint32_t nowMs = millis();
  if (lastReconnectAttemptMs == 0 ||
      nowMs - lastReconnectAttemptMs >= 5000) {
    lastReconnectAttemptMs = nowMs;
    WiFi.mode(WIFI_STA);
    WiFi.begin(uploadConfig::kWifiSsid, uploadConfig::kWifiPassword);
  }
  return false;
}

String buildUploadJson() {
  String json;
  json.reserve(256 + capturedSignal.rawData.size() * 7);
  json += F("{\"device_id\":\"");
  json += uploadConfig::kDeviceId;
  json += F("\",\"protocol\":\"");
  json += typeToString(capturedSignal.protocol);
  json += F("\",\"value\":\"");
  char valueHex[19];
  snprintf(valueHex, sizeof(valueHex), "0x%08lX%08lX",
           static_cast<unsigned long>(capturedSignal.value >> 32),
           static_cast<unsigned long>(capturedSignal.value));
  json += valueHex;
  json += F("\",\"address\":");
  json += String(capturedSignal.address);
  json += F(",\"command\":");
  json += String(capturedSignal.command);
  json += F(",\"bits\":");
  json += String(capturedSignal.bits);
  json += F(",\"carrier_khz\":");
  json += String(kIrCarrierFrequencyKhz);
  json += F(",\"raw_data\":[");
  for (size_t index = 0; index < capturedSignal.rawData.size(); ++index) {
    if (index != 0) {
      json += ',';
    }
    json += String(capturedSignal.rawData[index]);
  }
  json += F("]}");
  return json;
}

bool uploadCapturedSignal() {
  if (!capturedSignal.valid || !connectToWifi()) {
    Serial.println(F("Upload skipped: Wi-Fi is not connected."));
    return false;
  }

  drawScreen(ScreenState::kUploading);
  WiFiClientSecure secureClient;
  // HTTPS encryption is used, but certificate verification is disabled for
  // broad compatibility with shared-hosting certificates. See README.md for
  // the production recommendation.
  secureClient.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(uploadConfig::kHttpTimeoutMs);
  http.setTimeout(uploadConfig::kHttpTimeoutMs);
  const String uploadUrl =
      String(uploadConfig::kApiBaseUrl) + F("/upload.php");
  if (!http.begin(secureClient, uploadUrl)) {
    Serial.println(F("Upload failed: invalid HTTPS URL."));
    return false;
  }

  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-Key"), uploadConfig::kApiKey);
  const String payload = buildUploadJson();
  const int statusCode = http.POST(payload);
  const String response = http.getString();
  http.end();

  Serial.printf("Upload HTTP status: %d\n", statusCode);
  if (!response.isEmpty()) {
    Serial.println(response);
  }
  return statusCode == HTTP_CODE_CREATED;
}

bool reportCommandResult() {
  if (pendingResultCommandId == 0 || !maintainWifiConnection()) {
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(uploadConfig::kHttpTimeoutMs);
  http.setTimeout(uploadConfig::kHttpTimeoutMs);
  const String resultUrl =
      String(uploadConfig::kApiBaseUrl) + F("/command-result.php");
  if (!http.begin(secureClient, resultUrl)) {
    return false;
  }

  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-Key"), uploadConfig::kApiKey);

  String payload;
  payload.reserve(180 + pendingResultError.length());
  payload += F("{\"device_id\":\"");
  payload += uploadConfig::kDeviceId;
  payload += F("\",\"command_id\":");
  payload += String(pendingResultCommandId);
  payload += F(",\"status\":\"");
  payload += pendingResultSucceeded ? F("completed") : F("failed");
  payload += F("\",\"error\":\"");
  // Locally generated error messages contain only ASCII letters/underscores.
  payload += pendingResultError;
  payload += F("\"}");

  const int statusCode = http.POST(payload);
  const String response = http.getString();
  http.end();

  Serial.printf("Command result HTTP status: %d\n", statusCode);
  if (!response.isEmpty()) {
    Serial.println(response);
  }
  return statusCode == HTTP_CODE_OK;
}

void queueCommandResult(const uint32_t commandId,
                        const bool succeeded,
                        const String &error = String()) {
  pendingResultCommandId = commandId;
  pendingResultSucceeded = succeeded;
  pendingResultError = error;

  if (reportCommandResult()) {
    pendingResultCommandId = 0;
    pendingResultError = String();
  }
}

void transmitRemoteRawSignal(const std::vector<uint16_t> &rawData,
                             const uint16_t carrierKhz) {
  drawScreen(ScreenState::kSending);
  irReceiver.disableIRIn();
  irSender.sendRaw(rawData.data(), rawData.size(), carrierKhz);
  irReceiver.enableIRIn();
  drawScreen(ScreenState::kDone);
}

void pollRemoteCommand() {
  // Completion reports take priority. This prevents retransmitting a command
  // merely because the acknowledgement request temporarily failed.
  if (pendingResultCommandId != 0) {
    if (reportCommandResult()) {
      pendingResultCommandId = 0;
      pendingResultError = String();
    }
    return;
  }

  if (!maintainWifiConnection()) {
    Serial.println(F("Command poll skipped: Wi-Fi is not connected."));
    return;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(uploadConfig::kHttpTimeoutMs);
  http.setTimeout(uploadConfig::kHttpTimeoutMs);
  const String commandUrl =
      String(uploadConfig::kApiBaseUrl) + F("/command.php?device_id=") +
      uploadConfig::kDeviceId;
  if (!http.begin(secureClient, commandUrl)) {
    Serial.println(F("Command poll failed: invalid HTTPS URL."));
    return;
  }

  http.addHeader(F("Accept"), F("application/json"));
  http.addHeader(F("X-API-Key"), uploadConfig::kApiKey);
  const int statusCode = http.GET();

  if (statusCode == HTTP_CODE_NO_CONTENT) {
    http.end();
    return;
  }

  const String response = http.getString();
  http.end();
  if (statusCode != HTTP_CODE_OK) {
    Serial.printf("Command poll HTTP status: %d\n", statusCode);
    if (!response.isEmpty()) {
      Serial.println(response);
    }
    return;
  }

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(response);
  if (!root.success() || !root["ok"] || !root["command"].is<JsonObject &>()) {
    Serial.println(F("Command poll failed: invalid JSON response."));
    return;
  }

  JsonObject &command = root["command"];
  const uint32_t commandId = command["id"];
  const char *action = command["action"];
  const uint16_t carrierKhz = command["carrier_khz"];
  JsonArray &rawJson = command["raw_data"];

  if (commandId == 0 || action == nullptr ||
      strcmp(action, "send_ir") != 0 ||
      carrierKhz < 20 || carrierKhz > 100 ||
      !rawJson.success() || rawJson.size() < 1 ||
      rawJson.size() > kReceiveBufferSize * 2) {
    Serial.println(F("Command rejected: invalid command fields."));
    if (commandId != 0) {
      queueCommandResult(commandId, false, F("invalid_command_fields"));
    }
    return;
  }

  std::vector<uint16_t> rawData;
  rawData.reserve(rawJson.size());
  for (JsonArray::iterator item = rawJson.begin(); item != rawJson.end();
       ++item) {
    const uint32_t duration = item->as<uint32_t>();
    if (duration < 1 || duration > 65535) {
      queueCommandResult(commandId, false, F("invalid_raw_data"));
      return;
    }
    rawData.push_back(static_cast<uint16_t>(duration));
  }

  Serial.printf("Executing remote command %lu (RAW length %u)\n",
                static_cast<unsigned long>(commandId),
                static_cast<unsigned int>(rawData.size()));
  transmitRemoteRawSignal(rawData, carrierKhz);
  queueCommandResult(commandId, true);
  delay(kMessageDisplayMs);
  drawScreen(ScreenState::kReady);
}

void transmitCapturedSignalOnce() {
  // Replay the captured mark/space timings directly. This preserves the
  // captured 40-bit Panasonic variant instead of rebuilding a standard
  // 48-bit Panasonic frame from its decoded address and command fields.
  irSender.sendRaw(capturedSignal.rawData.data(),
                   capturedSignal.rawData.size(),
                   kIrCarrierFrequencyKhz);
}

void transmitWhileButtonAHeld() {
  if (!capturedSignal.valid) {
    return;
  }

  drawScreen(ScreenState::kSending);

  // Do not let the receiver capture the signal emitted by this device itself.
  // Re-enabling it after transmission also clears any partial receive state.
  irReceiver.disableIRIn();

  // Send immediately, then keep repeating at a normal remote-control cadence
  // for as long as button A remains physically held down.
  while (M5.BtnA.isPressed()) {
    const uint32_t transmissionStartedMs = millis();
    transmitCapturedSignalOnce();

    // Keep updating the M5 button state during the interval so releasing A is
    // noticed promptly instead of waiting for a fixed blocking delay.
    while (M5.BtnA.isPressed() &&
           millis() - transmissionStartedMs < kTransmitRepeatPeriodMs) {
      M5.update();
      delay(1);
    }

    M5.update();
  }

  irReceiver.enableIRIn();

  drawScreen(ScreenState::kDone);
  delay(kMessageDisplayMs);
  drawScreen(ScreenState::kReady);
}

void receiveWhileButtonBHeld() {
  bool capturedDuringThisHold = false;
  const uint32_t initialEdgeCount = irInputEdgeCount;
  uint32_t lastDebugUpdateMs = 0;
  drawScreen(ScreenState::kReceiving);

  Serial.println(F("[IR MONITOR] Started. Point a remote and press a button."));
  updateReceiveDebugMonitor(0);

  // Stay in receive mode only while button B remains held down.
  while (M5.BtnB.isPressed()) {
    if (!capturedDuringThisHold && irReceiver.decode(&receiveResults)) {
      saveReceivedSignal(receiveResults);
      capturedDuringThisHold = capturedSignal.valid;
      drawScreen(ScreenState::kCaptured);

      // Resume the receiver so that its internal state is ready for the next
      // time button B is held. This capture is intentionally not overwritten
      // again during the same button hold.
      irReceiver.resume();

      const bool uploaded = uploadCapturedSignal();
      drawScreen(uploaded ? ScreenState::kUploadDone
                          : ScreenState::kUploadFailed);
    }

    const uint32_t nowMs = millis();
    if (!capturedDuringThisHold &&
        nowMs - lastDebugUpdateMs >= kDebugRefreshMs) {
      const uint32_t edgesSinceButtonPress =
          irInputEdgeCount - initialEdgeCount;
      updateReceiveDebugMonitor(edgesSinceButtonPress);
      lastDebugUpdateMs = nowMs;
    }

    M5.update();
    delay(1);
  }

  drawScreen(ScreenState::kReady);
}

}  // namespace

void setup() {
  M5.begin();
  Serial.begin(115200);

  M5.Lcd.setRotation(kLcdRotation);
  M5.Lcd.setTextWrap(true);

  pinMode(kIrReceivePin, INPUT);
  attachInterrupt(digitalPinToInterrupt(kIrReceivePin), onIrInputChange, CHANGE);
  irSender.begin();
  irReceiver.enableIRIn();

  // Force the initial draw because currentScreen starts at kReady.
  currentScreen = ScreenState::kDone;
  drawScreen(ScreenState::kReady);
  Serial.println(F("IR capture/replay ready."));
  Serial.println(F("Debug: Hold button B and watch GPIO36/Edges."));
  Serial.println(connectToWifi() ? F("Wi-Fi connected.")
                                 : F("Wi-Fi connection failed."));
}

void loop() {
  M5.update();

  if (M5.BtnB.isPressed()) {
    receiveWhileButtonBHeld();
  } else if (M5.BtnA.isPressed()) {
    // Button A has no effect until at least one signal has been captured.
    transmitWhileButtonAHeld();
  } else {
    const uint32_t nowMs = millis();
    if (nowMs - lastCommandPollMs >=
        uploadConfig::kCommandPollIntervalMs) {
      lastCommandPollMs = nowMs;
      pollRemoteCommand();
    }
  }

  delay(1);
}
