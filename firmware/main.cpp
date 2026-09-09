/**
 * Smart Pet Feeder — firmware on smart-pet-device-sdk.
 *
 * The SDK owns the plumbing every device used to re-implement (Wi-Fi + SoftAP
 * provisioning, NTP, MQTT on kennel/{kennelId}/feeder/{deviceId}/*, LWT, OTA,
 * command/ack, schedule caching, offline journal). This file is just the feeder:
 * dispense on command / schedule, report the food level, a manual button, a
 * status LED.
 *
 * First boot with no NVS creds: the device opens the "smartpet-<mac>" Wi-Fi AP;
 * connect and fill in the form. Nothing is hardcoded.
 *
 * The pre-SDK single-file firmware is kept as smart-feeder.legacy.cpp.
 *
 * Wiring (diagram.json, unchanged): servo GPIO4, ultrasonic TRIG 5 / ECHO 18,
 * status LED GPIO2, manual-feed button GPIO0.
 */
#include <SmartPetDevice.h>

constexpr int SERVO_PIN  = 4;
constexpr int TRIG_PIN   = 5;
constexpr int ECHO_PIN   = 18;
constexpr int LED_PIN    = 2;
constexpr int BUTTON_PIN = 0;

// grams/second of auger turn. ponytail: calibrate on a real board (run the feed
// command, weigh the output, divide) and set this. 20 is a placeholder.
constexpr float AUGER_GRAMS_PER_SEC = 20.0f;
constexpr float MANUAL_FEED_GRAMS   = 40.0f;
constexpr float LOW_FOOD_PCT        = 20.0f;

spd::SmartPetDevice dev("feeder");
spd::FeederModule   feeder(SERVO_PIN, TRIG_PIN, ECHO_PIN);

static int      lastButtonStable = LOW;
static int      lastButtonRead = LOW;
static uint32_t buttonChangedMs = 0;

static void feedNow(float grams) {
  digitalWrite(LED_PIN, HIGH);
  bool ok = feeder.dispense(grams);
  digitalWrite(LED_PIN, LOW);
  if (ok) dev.reportAction("fed", grams);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  feeder.begin();
  feeder.setCalibration(AUGER_GRAMS_PER_SEC);
  dev.begin();

  // Manual "Feed Now" from the backend.
  dev.onCommand("feed", [](JsonObjectConst p, const String&) {
    feedNow(p["amount"] | MANUAL_FEED_GRAMS);
    return true;
  });

  // Scheduled feeds — schedule_set is stored + cached by the SDK; this fires it.
  dev.onScheduledAction([](float grams) { feedNow(grams); });

  // Extra fields on every retained status message.
  dev.onStatusFill([](JsonObject& s) {
    float lvl = feeder.foodLevelPct();
    s["foodLevel"] = lvl;
    s["isFoodLow"] = lvl < LOW_FOOD_PCT;
    s["jammed"]    = feeder.jammed();
  });

  // "identify" from the console: blink the LED.
  dev.identifyFn_ = [](int secs) {
    for (int i = 0; i < secs * 2; ++i) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(250);
    }
    digitalWrite(LED_PIN, LOW);
  };
}

// Debounced manual-feed button (GPIO0), lifted from the legacy firmware.
static void pollButton() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonRead) {
    buttonChangedMs = millis();
    lastButtonRead = reading;
  }
  if (millis() - buttonChangedMs < 50) return;
  if (reading == lastButtonStable) return;
  lastButtonStable = reading;
  if (lastButtonStable == HIGH) {
    Serial.println("[feeder] manual button feed");
    feedNow(MANUAL_FEED_GRAMS);
  }
}

// Status LED: fast blink = provisioning/offline, slow blink = low food, on = ok.
static void updateLed() {
  if (dev.inProvisioning() || !dev.isOnline()) {
    digitalWrite(LED_PIN, (millis() / 300) % 2);
  } else if (feeder.foodLevelPct() < LOW_FOOD_PCT) {
    digitalWrite(LED_PIN, (millis() / 1000) % 2);
  } else {
    digitalWrite(LED_PIN, HIGH);
  }
}

void loop() {
  dev.loop();
  pollButton();
  updateLed();

  // Publish the food level as a metric once a minute.
  static uint32_t lastLevelPub = 0;
  if (dev.isOnline() && millis() - lastLevelPub > 60000) {
    lastLevelPub = millis();
    dev.publishMetric("level", feeder.foodLevelPct(), "percent");
  }

  delay(10);
}
