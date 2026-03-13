/**
 * Smart Pet Feeder Firmware
 * ESP32-based with Wi-Fi, scheduling, and API control
 * 
 * Features:
 * - Motor control for food dispensing
 * - Ultrasonic sensor for food level monitoring
 * - Wi-Fi connectivity for API control
 * - Scheduled feeding times
 * - Safety fail-safes
 * - Event logging
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ============== CONFIGURATION ==============
// Wi-Fi credentials - replace with your network
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// API Configuration
const char* API_BASE_URL = "http://localhost:3002/api";
const char* API_KEY = "your-api-key-here";

// Hardware Pins
const int SERVO_PIN = 4;
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int LED_PIN = 2;

// ============== CONSTANTS ==============
const int SERVO_OPEN_ANGLE = 90;
const int SERVO_CLOSE_ANGLE = 0;
const int DISPENSE_DURATION_MS = 2000;
const int MAX_FOOD_LEVEL_CM = 15;
const int MIN_FOOD_LEVEL_CM = 2;

// ============== GLOBALS ==============
Preferences preferences;
bool isConnected = false;
unsigned long lastApiCall = 0;
const unsigned long API_CALL_INTERVAL = 30000; // 30 seconds

// Feeding schedule
struct FeedingSchedule {
  int hour;
  int minute;
  bool enabled;
};

#define MAX_SCHEDULES 5
FeedingSchedule schedules[MAX_SCHEDULES];
int scheduleCount = 0;

// Event log
#define MAX_LOG_ENTRIES 50
String eventLog[MAX_LOG_ENTRIES];
int logIndex = 0;
int logCount = 0;

// ============== MOTOR CONTROL ==============
#include <Servo.h>
Servo feederServo;

/**
 * Initialize the servo motor
 */
void initMotor() {
  feederServo.attach(SERVO_PIN);
  feederServo.write(SERVO_CLOSE_ANGLE);
  logEvent("Motor initialized");
}

/**
 * Dispense food for a specified duration
 * @param durationMs Duration in milliseconds
 * @return true if successful
 */
bool dispenseFood(int durationMs = DISPENSE_DURATION_MS) {
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

// ============== ULTRASONIC SENSOR ==============
/**
 * Measure distance using ultrasonic sensor
 * @return Distance in centimeters, or -1 on error
 */
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = (duration * 0.0343) / 2;
  
  if (distance > 0 && distance < 400) {
    return distance;
  }
  return -1;
}

/**
 * Get food level percentage (0-100)
 */
int getFoodLevel() {
  float distance = measureDistance();
  if (distance < 0) return -1;
  
  int level = map(distance, MIN_FOOD_LEVEL_CM, MAX_FOOD_LEVEL_CM, 100, 0);
  return constrain(level, 0, 100);
}

/**
 * Check if food level is low
 */
bool isFoodLow() {
  return getFoodLevel() < 20;
}

/**
 * Check if motor operation is safe
 */
bool isMotorSafe() {
  float distance = measureDistance();
  return distance > MIN_FOOD_LEVEL_CM;
}

// ============== WIFI CONNECTION ==============
/**
 * Connect to Wi-Fi network
 */
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    isConnected = true;
    logEvent("WiFi connected");
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    isConnected = false;
    logEvent("ERROR: WiFi connection failed");
    Serial.println("\nWiFi connection failed!");
  }
}

// ============== API COMMUNICATION ==============
/**
 * Send status update to API
 */
void sendStatusToApi() {
  if (!isConnected) return;
  
  HTTPClient http;
  String url = String(API_BASE_URL) + "/status";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", API_KEY);
  
  StaticJsonDocument<256> doc;
  doc["feeder_id"] = preferences.getString("feeder_id", "feeder_001");
  doc["food_level"] = getFoodLevel();
  doc["is_low_food"] = isFoodLow();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["uptime_ms"] = millis();
  
  String json;
  serializeJson(doc, json);
  
  int httpCode = http.POST(json);
  
  if (httpCode == HTTP_CODE_OK) {
    logEvent("Status sent to API");
  } else {
    logEvent("ERROR: Failed to send status");
  }
  
  http.end();
}

/**
 * Fetch feeding schedule from API
 */
void fetchScheduleFromApi() {
  if (!isConnected) return;
  
  HTTPClient http;
  String url = String(API_BASE_URL) + "/schedule";
  
  http.begin(url);
  http.addHeader("X-API-Key", API_KEY);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      scheduleCount = 0;
      JsonArray scheduleArray = doc["schedules"];
      for (JsonObject sched : scheduleArray) {
        if (scheduleCount < MAX_SCHEDULES) {
          schedules[scheduleCount].hour = sched["hour"];
          schedules[scheduleCount].minute = sched["minute"];
          schedules[scheduleCount].enabled = sched["enabled"];
          scheduleCount++;
        }
      }
      logEvent("Schedule updated from API");
    }
  }
  
  http.end();
}

/**
 * Handle incoming API command
 */
void handleApiCommand(const String& command) {
  if (command == "feed") {
    dispenseFood();
  } else if (command == "status") {
    Serial.printf("Food Level: %d%%\n", getFoodLevel());
    Serial.printf("Is Low: %s\n", isFoodLow() ? "Yes" : "No");
  }
}

// ============== SCHEDULING ==============
/**
 * Check if it's time to feed based on schedule
 */
void checkSchedule() {
  if (scheduleCount == 0) return;
  
  static int lastMinute = -1;
  int currentMinute = minute();
  
  if (currentMinute != lastMinute) {
    lastMinute = currentMinute;
    
    for (int i = 0; i < scheduleCount; i++) {
      if (schedules[i].enabled && 
          schedules[i].hour == hour() && 
          schedules[i].minute == minute()) {
        logEvent("Scheduled feeding triggered");
        dispenseFood();
      }
    }
  }
}

// ============== EVENT LOGGING ==============
/**
 * Add event to log
 */
void logEvent(const String& message) {
  String entry = "[" + String(millis()) + "] " + message;
  eventLog[logIndex] = entry;
  logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
  if (logCount < MAX_LOG_ENTRIES) logCount++;
  
  Serial.println(entry);
}

/**
 * Get log entries as JSON string
 */
String getLogJson() {
  String json = "{\"logs\":[";
  int start = (logCount < MAX_LOG_ENTRIES) ? 0 : logIndex;
  
  for (int i = 0; i < logCount; i++) {
    int idx = (start + i) % MAX_LOG_ENTRIES;
    json += "\"" + eventLog[idx] + "\"";
    if (i < logCount - 1) json += ",";
  }
  
  json += "]}";
  return json;
}

// ============== LED INDICATORS ==============
/**
 * Update LED based on status
 */
void updateLED() {
  if (!isConnected) {
    // Slow blink when not connected
    digitalWrite(LED_PIN, (millis() / 1000) % 2);
  } else if (isFoodLow()) {
    // Fast blink when food is low
    digitalWrite(LED_PIN, (millis() / 200) % 2);
  } else {
    // Solid on when connected and OK
    digitalWrite(LED_PIN, HIGH);
  }
}

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  
  // Initialize preferences
  preferences.begin("smart-feeder", false);
  
  // Initialize hardware
  initMotor();
  
  // Connect to Wi-Fi
  connectWiFi();
  
  logEvent("Smart Feeder initialized");
  
  // Blink LED to indicate startup
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
}

// ============== MAIN LOOP ==============
void loop() {
  // Check Wi-Fi connection
  if (WiFi.status() != WL_CONNECTED) {
    isConnected = false;
    connectWiFi();
  }
  
  // Periodic API calls
  if (millis() - lastApiCall > API_CALL_INTERVAL) {
    lastApiCall = millis();
    sendStatusToApi();
    fetchScheduleFromApi();
  }
  
  // Check feeding schedule
  checkSchedule();
  
  // Update LED
  updateLED();
  
  delay(100);
}
