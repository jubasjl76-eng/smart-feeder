/**
 * Smart Pet Feeder Firmware
 * ESP32 — Wi-Fi, MQTT (QoS 2 commands / QoS 1 retained status), NTP schedules
 *
 * Hardware (diagram.json — do not change pinout):
 *   Servo PWM GPIO 4, ultrasonic TRIG 5 / ECHO 18, LED 2, button GPIO 0
 *
 * Credentials live in NVS Preferences (optionally seeded once from
 * gitignored provision.local.h). No compile-time API_KEY / HTTP loop.
 */

#include <WiFi.h>
#include <espMqttClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESP32Servo.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

#if defined(__has_include)
#  if __has_include("provision.local.h")
#    include "provision.local.h"
#    define HAS_PROVISION_LOCAL 1
#  endif
#endif

#ifndef WIFI_TIMEOUT
#define WIFI_TIMEOUT 30000
#endif

const int SERVO_PIN = 4;
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int LED_PIN = 2;
const int BUTTON_PIN = 0;

const int SERVO_OPEN_ANGLE = 90;
const int SERVO_CLOSE_ANGLE = 0;
const int DISPENSE_DURATION_MS = 2000;
const int MAX_FOOD_LEVEL_CM = 15;
const int MIN_FOOD_LEVEL_CM = 2;
const unsigned long STATUS_INTERVAL_MS = 30000;
const unsigned long BUTTON_DEBOUNCE_MS = 50;
const unsigned long ULTRASONIC_TIMEOUT_US = 30000;
const time_t NTP_SYNC_EPOCH = 1600000000;
const char* DEFAULT_TZ = "WET0WEST,M3.5.0/1,M10.5.0/2";
const uint8_t MQTT_STATUS_QOS = 1;
const uint8_t MQTT_COMMAND_QOS = 2;
const uint16_t MQTT_DEFAULT_PORT = 1883;

#define MAX_SCHEDULES 16
#define MQTT_MSG_MAX 2048
#define ID_LEN 48
#define HOST_LEN 96
#define SSID_LEN 64
#define PASS_LEN 64
#define TZ_LEN 48
#define TOPIC_LEN 160
#define USER_LEN 64
#define LWT_LEN 192

Preferences preferences;
Servo feederServo;
espMqttClient mqttClient;

bool wifiConnected = false;
bool mqttConnected = false;
bool ntpRequested = false;
unsigned long lastStatusMs = 0;
unsigned long lastMqttReconnectMs = 0;
unsigned long mqttReconnectDelayMs = 1000;
unsigned long lastWifiAttemptMs = 0;
unsigned long wifiRetryDelayMs = 1000;

char deviceId[ID_LEN] = {0};
char kennelId[ID_LEN] = {0};
char wifiSsid[SSID_LEN] = {0};
char wifiPassword[PASS_LEN] = {0};
char mqttHost[HOST_LEN] = {0};
char mqttPassword[PASS_LEN] = {0};
char timezonePosix[TZ_LEN] = {0};
char mqttUsername[USER_LEN] = {0};
char mqttClientId[ID_LEN] = {0};
char statusTopic[TOPIC_LEN] = {0};
char commandTopic[TOPIC_LEN] = {0};
char lwtPayload[LWT_LEN] = {0};
uint16_t mqttPort = MQTT_DEFAULT_PORT;

uint64_t lastFeedMs = 0;

struct FeedingSchedule {
  char id[ID_LEN];
  int hour;
  int minute;
  int amount;
  bool enabled;
};

FeedingSchedule schedules[MAX_SCHEDULES];
int scheduleCount = 0;
int lastScheduleMinuteOfDay = -1;

volatile bool feedPending = false;
volatile int feedPendingAmount = 100;
volatile bool schedulePending = false;
char schedulePendingJson[MQTT_MSG_MAX];
char mqttMsgBuf[MQTT_MSG_MAX];

int lastButtonStable = LOW;
int lastButtonRead = LOW;
unsigned long lastButtonChangeMs = 0;

#define MAX_LOG_ENTRIES 50
String eventLog[MAX_LOG_ENTRIES];
int logIndex = 0;
int logCount = 0;

void logEvent(const String& message);
int getFoodLevel();
bool isFoodLow();
bool isMotorSafe();
int amountToDurationMs(int amount);
bool dispenseFood(int durationMs = DISPENSE_DURATION_MS);
void requestFeed(int amount);
void updateLastFeed();
uint64_t unixTimeMs();
bool isNtpSynced();
void publishStatus();
void configureMqttIdentity();

void logEvent(const String& message) {
  String entry = "[" + String(millis()) + "] " + message;
  eventLog[logIndex] = entry;
  logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
  if (logCount < MAX_LOG_ENTRIES) logCount++;
  Serial.println(entry);
}

void initMotor() {
  feederServo.attach(SERVO_PIN);
  feederServo.write(SERVO_CLOSE_ANGLE);
  logEvent("Motor initialized");
}

int amountToDurationMs(int amount) {
  if (amount < 1) amount = 100;
  long ms = (long)amount * (long)DISPENSE_DURATION_MS / 100L;
  if (ms < 200) ms = 200;
  if (ms > 10000) ms = 10000;
  return (int)ms;
}

bool dispenseFood(int durationMs) {
  if (!isMotorSafe()) {
    logEvent("ERROR: Motor not safe to operate");
    return false;
  }
  logEvent("Dispensing food...");
  feederServo.write(SERVO_OPEN_ANGLE);
  delay(durationMs);
  feederServo.write(SERVO_CLOSE_ANGLE);
  logEvent("Food dispensed successfully");
  return true;
}

void requestFeed(int amount) {
  feedPendingAmount = amount;
  feedPending = true;
}

void updateLastFeed() {
  lastFeedMs = unixTimeMs();
  preferences.putULong64("last_feed", lastFeedMs);
}

float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, ULTRASONIC_TIMEOUT_US);
  if (duration <= 0) return -1;
  float distance = (duration * 0.0343f) / 2.0f;
  if (distance > 0 && distance < 400) return distance;
  return -1;
}

int getFoodLevel() {
  float distance = measureDistance();
  if (distance < 0) return -1;
  int level = map((long)distance, MIN_FOOD_LEVEL_CM, MAX_FOOD_LEVEL_CM, 100, 0);
  return constrain(level, 0, 100);
}

bool isFoodLow() {
  int level = getFoodLevel();
  return level >= 0 && level < 20;
}

bool isMotorSafe() {
  float distance = measureDistance();
  return distance > MIN_FOOD_LEVEL_CM;
}

uint64_t unixTimeMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < 0) return 0;
  return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

bool isNtpSynced() {
  return time(nullptr) >= NTP_SYNC_EPOCH;
}

void startNtp() {
  const char* tz = timezonePosix[0] ? timezonePosix : DEFAULT_TZ;
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");
  setenv("TZ", tz, 1);
  tzset();
  ntpRequested = true;
  logEvent(String("NTP started, TZ=") + tz);
}

void clearSchedules() {
  scheduleCount = 0;
  memset(schedules, 0, sizeof(schedules));
}

bool parseSchedulesFromJson(const char* json) {
  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    logEvent(String("ERROR: schedule JSON: ") + err.c_str());
    return false;
  }
  JsonArray arr;
  if (doc.is<JsonArray>()) arr = doc.as<JsonArray>();
  else if (doc["schedules"].is<JsonArray>()) arr = doc["schedules"].as<JsonArray>();
  else if (doc["params"]["schedules"].is<JsonArray>()) arr = doc["params"]["schedules"].as<JsonArray>();
  else {
    logEvent("ERROR: schedule_set missing schedules array");
    return false;
  }
  FeedingSchedule parsed[MAX_SCHEDULES];
  int n = 0;
  for (JsonObject sched : arr) {
    if (n >= MAX_SCHEDULES) break;
    const char* id = sched["id"] | "";
    const char* timeStr = sched["time"] | "";
    int amount = sched["amount"] | 100;
    bool enabled = sched["enabled"] | true;
    int hour = -1;
    int minute = -1;
    if (sscanf(timeStr, "%d:%d", &hour, &minute) != 2 || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
      logEvent(String("ERROR: invalid schedule time: ") + timeStr);
      continue;
    }
    memset(&parsed[n], 0, sizeof(parsed[n]));
    strncpy(parsed[n].id, id, ID_LEN - 1);
    parsed[n].hour = hour;
    parsed[n].minute = minute;
    parsed[n].amount = amount < 1 ? 100 : amount;
    parsed[n].enabled = enabled;
    n++;
  }
  memcpy(schedules, parsed, sizeof(parsed));
  scheduleCount = n;
  lastScheduleMinuteOfDay = -1;
  logEvent(String("Schedule snapshot replaced, count=") + String(scheduleCount));
  return true;
}

void persistSchedules() {
  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < scheduleCount; i++) {
    JsonObject o = arr.createNestedObject();
    o["id"] = schedules[i].id;
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", schedules[i].hour, schedules[i].minute);
    o["time"] = timeBuf;
    o["amount"] = schedules[i].amount;
    o["enabled"] = schedules[i].enabled;
  }
  char json[2048];
  serializeJson(arr, json, sizeof(json));
  preferences.putString("schedules", json);
}

void loadSchedulesFromNvs() {
  String json = preferences.getString("schedules", "");
  if (json.length() == 0) { clearSchedules(); return; }
  if (!parseSchedulesFromJson(json.c_str())) clearSchedules();
}

void applyScheduleSet(const char* json) {
  if (!parseSchedulesFromJson(json)) return;
  persistSchedules();
}

void checkSchedule() {
  if (!isNtpSynced() || scheduleCount == 0) return;
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  int minuteOfDay = local.tm_hour * 60 + local.tm_min;
  if (minuteOfDay == lastScheduleMinuteOfDay) return;
  lastScheduleMinuteOfDay = minuteOfDay;
  for (int i = 0; i < scheduleCount; i++) {
    if (schedules[i].enabled && schedules[i].hour == local.tm_hour && schedules[i].minute == local.tm_min) {
      logEvent(String("Scheduled feeding triggered: ") + schedules[i].id);
      requestFeed(schedules[i].amount);
    }
  }
}

void copyToBuf(char* dest, size_t destLen, const String& src) {
  strncpy(dest, src.c_str(), destLen - 1);
  dest[destLen - 1] = '\0';
}

void seedFromProvisionLocal() {
#if defined(HAS_PROVISION_LOCAL)
  if (preferences.getBool("provisioned", false)) return;
  preferences.putString("wifi_ssid", PROVISION_WIFI_SSID);
  preferences.putString("wifi_pass", PROVISION_WIFI_PASSWORD);
  preferences.putString("mqtt_host", PROVISION_MQTT_HOST);
#ifdef PROVISION_MQTT_PORT
  preferences.putUShort("mqtt_port", (uint16_t)PROVISION_MQTT_PORT);
#else
  preferences.putUShort("mqtt_port", MQTT_DEFAULT_PORT);
#endif
  preferences.putString("mqtt_pass", PROVISION_MQTT_PASSWORD);
  preferences.putString("device_id", PROVISION_DEVICE_ID);
  preferences.putString("kennel_id", PROVISION_KENNEL_ID);
#ifdef PROVISION_TZ
  preferences.putString("tz", PROVISION_TZ);
#else
  preferences.putString("tz", DEFAULT_TZ);
#endif
  preferences.putBool("provisioned", true);
  logEvent("NVS seeded from provision.local.h");
#endif
}

void loadProvision() {
  seedFromProvisionLocal();
  copyToBuf(wifiSsid, sizeof(wifiSsid), preferences.getString("wifi_ssid", ""));
  copyToBuf(wifiPassword, sizeof(wifiPassword), preferences.getString("wifi_pass", ""));
  copyToBuf(mqttHost, sizeof(mqttHost), preferences.getString("mqtt_host", ""));
  mqttPort = preferences.getUShort("mqtt_port", MQTT_DEFAULT_PORT);
  copyToBuf(mqttPassword, sizeof(mqttPassword), preferences.getString("mqtt_pass", ""));
  copyToBuf(deviceId, sizeof(deviceId), preferences.getString("device_id", ""));
  copyToBuf(kennelId, sizeof(kennelId), preferences.getString("kennel_id", ""));
  copyToBuf(timezonePosix, sizeof(timezonePosix), preferences.getString("tz", DEFAULT_TZ));
  lastFeedMs = preferences.getULong64("last_feed", 0);
  if (timezonePosix[0] == '\0') strncpy(timezonePosix, DEFAULT_TZ, TZ_LEN - 1);
  loadSchedulesFromNvs();
  configureMqttIdentity();
}

bool provisionReady() {
  return deviceId[0] && kennelId[0] && wifiSsid[0] && mqttHost[0];
}

void configureMqttIdentity() {
  snprintf(mqttUsername, sizeof(mqttUsername), "device:%s", deviceId);
  strncpy(mqttClientId, deviceId, sizeof(mqttClientId) - 1);
  snprintf(statusTopic, sizeof(statusTopic), "kennel/%s/feeder/%s/status", kennelId, deviceId);
  snprintf(commandTopic, sizeof(commandTopic), "kennel/%s/feeder/%s/command", kennelId, deviceId);
  snprintf(lwtPayload, sizeof(lwtPayload),
           "{\"deviceId\":\"%s\",\"kennelId\":\"%s\",\"timestamp\":0,\"status\":\"offline\"}",
           deviceId, kennelId);
}

void publishStatus() {
  if (!mqttClient.connected()) return;
  StaticJsonDocument<384> doc;
  doc["deviceId"] = deviceId;
  doc["kennelId"] = kennelId;
  doc["timestamp"] = unixTimeMs();
  doc["status"] = "online";
  int level = getFoodLevel();
  if (level >= 0) doc["foodLevel"] = level;
  doc["lastFeed"] = lastFeedMs;
  char payload[384];
  serializeJson(doc, payload, sizeof(payload));
  uint16_t packetId = mqttClient.publish(statusTopic, MQTT_STATUS_QOS, true, payload);
  if (packetId == 0) logEvent("ERROR: status publish failed");
}

bool idsMatchCommand(JsonDocument& root) {
  const char* cmdDevice = root["deviceId"] | "";
  const char* cmdKennel = root["kennelId"] | "";
  if (cmdDevice[0] && strcmp(cmdDevice, deviceId) != 0) return false;
  if (cmdKennel[0] && strcmp(cmdKennel, kennelId) != 0) return false;
  return true;
}

void handleQueuedCommand(const char* json) {
  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) { logEvent(String("ERROR: command JSON: ") + err.c_str()); return; }
  if (!idsMatchCommand(doc)) { logEvent("Ignoring command for another device"); return; }
  const char* command = doc["command"] | "";
  if (strcmp(command, "feed") == 0) {
    int amount = 100;
    if (doc["params"]["amount"].is<int>() || doc["params"]["amount"].is<float>())
      amount = doc["params"]["amount"].as<int>();
    logEvent(String("MQTT feed amount=") + String(amount));
    requestFeed(amount);
  } else if (strcmp(command, "schedule_set") == 0) {
    char snapshot[MQTT_MSG_MAX];
    if (doc["params"]["schedules"].is<JsonArray>()) {
      serializeJson(doc["params"]["schedules"], snapshot, sizeof(snapshot));
      applyScheduleSet(snapshot);
    } else {
      logEvent("ERROR: schedule_set missing params.schedules");
    }
  } else {
    logEvent(String("Unknown command: ") + command);
  }
}

void onMqttConnect(bool sessionPresent) {
  mqttConnected = true;
  mqttReconnectDelayMs = 1000;
  logEvent(String("MQTT connected, sessionPresent=") + String(sessionPresent ? "1" : "0"));
  uint16_t packetId = mqttClient.subscribe(commandTopic, MQTT_COMMAND_QOS);
  if (packetId == 0) logEvent("ERROR: command subscribe failed");
  else logEvent("Subscribed to command topic QoS 2");
  publishStatus();
  lastStatusMs = millis();
}

void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
  mqttConnected = false;
  logEvent(String("MQTT disconnected, reason=") + String(static_cast<uint8_t>(reason)));
  lastMqttReconnectMs = millis();
  if (mqttReconnectDelayMs < 30000) mqttReconnectDelayMs *= 2;
}

void onMqttMessage(const espMqttClientTypes::MessageProperties& properties,
                   const char* topic, const uint8_t* payload, size_t len,
                   size_t index, size_t total) {
  (void)topic;
  if (total >= MQTT_MSG_MAX) { logEvent("ERROR: MQTT payload too large"); return; }
  if (index + len > MQTT_MSG_MAX - 1) return;
  memcpy(mqttMsgBuf + index, payload, len);
  if (index + len < total) return;
  mqttMsgBuf[total] = '\0';
  if (properties.retain) { logEvent("Ignoring retained command"); return; }
  strncpy(schedulePendingJson, mqttMsgBuf, MQTT_MSG_MAX - 1);
  schedulePendingJson[MQTT_MSG_MAX - 1] = '\0';
  schedulePending = true;
}

void setupMqttClient() {
  mqttClient.setKeepAlive(30);
  mqttClient.setCleanSession(false);
  mqttClient.setClientId(mqttClientId);
  mqttClient.setCredentials(mqttUsername, mqttPassword);
  mqttClient.setServer(mqttHost, mqttPort);
  mqttClient.setWill(statusTopic, MQTT_STATUS_QOS, true, lwtPayload);
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
}

void connectMqtt() {
  if (!provisionReady() || !wifiConnected || mqttClient.connected()) return;
  logEvent("Connecting MQTT...");
  if (!mqttClient.connect()) logEvent("MQTT connect() returned false");
}

void connectWiFi() {
  if (wifiSsid[0] == '\0') {
    logEvent("ERROR: WiFi SSID missing (provision NVS or provision.local.h)");
    wifiConnected = false;
    return;
  }
  logEvent(String("Connecting WiFi SSID=") + wifiSsid);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSsid, wifiPassword);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    logEvent("WiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    if (!ntpRequested) startNtp();
  } else {
    wifiConnected = false;
    logEvent("ERROR: WiFi connection failed");
  }
}

void handleButton() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonRead) {
    lastButtonChangeMs = millis();
    lastButtonRead = reading;
  }
  if (millis() - lastButtonChangeMs < BUTTON_DEBOUNCE_MS) return;
  if (reading == lastButtonStable) return;
  lastButtonStable = reading;
  if (lastButtonStable == HIGH) {
    logEvent("Manual button feed");
    requestFeed(100);
  }
}

void updateLED() {
  if (!mqttConnected) digitalWrite(LED_PIN, (millis() / 1000) % 2);
  else if (isFoodLow()) digitalWrite(LED_PIN, (millis() / 200) % 2);
  else digitalWrite(LED_PIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  preferences.begin("smart-feeder", false);
  loadProvision();
  initMotor();
  if (!provisionReady()) {
    logEvent("ERROR: missing deviceId/kennelId/wifiSsid/mqttHost in Preferences");
  } else {
    setupMqttClient();
    connectWiFi();
    if (wifiConnected) connectMqtt();
  }
  logEvent("Smart Feeder initialized");
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH); delay(200);
    digitalWrite(LED_PIN, LOW); delay(200);
  }
}

void loop() {
  mqttClient.loop();
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    mqttConnected = false;
    unsigned long now = millis();
    if (lastWifiAttemptMs == 0 || now - lastWifiAttemptMs >= wifiRetryDelayMs) {
      lastWifiAttemptMs = now;
      connectWiFi();
      if (wifiConnected) {
        wifiRetryDelayMs = 1000;
        lastMqttReconnectMs = 0;
        mqttReconnectDelayMs = 1000;
        connectMqtt();
      } else if (wifiRetryDelayMs < 30000) {
        wifiRetryDelayMs *= 2;
      }
    }
  } else {
    wifiConnected = true;
    wifiRetryDelayMs = 1000;
    if (!ntpRequested) startNtp();
    if (!mqttClient.connected() && provisionReady()) {
      unsigned long now = millis();
      if (now - lastMqttReconnectMs >= mqttReconnectDelayMs) {
        lastMqttReconnectMs = now;
        connectMqtt();
      }
    }
  }
  if (schedulePending) {
    schedulePending = false;
    handleQueuedCommand(schedulePendingJson);
  }
  if (feedPending) {
    int amount = feedPendingAmount;
    feedPending = false;
    if (dispenseFood(amountToDurationMs(amount))) {
      updateLastFeed();
      publishStatus();
    }
  }
  handleButton();
  checkSchedule();
  if (mqttClient.connected() && millis() - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = millis();
    publishStatus();
  }
  updateLED();
  delay(50);
}
