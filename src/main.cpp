#include <Adafruit_AHTX0.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>
#include <IPv6Address.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiSTA.h>
#include <Wire.h>
#include <esp_pthread.h>

#include "log.h"
#include "sntp.h"
#include "time_utils.h"

static DHT dht(GPIO_NUM_10, DHT11);
static Adafruit_AHTX0 aht;

// #define ENABLE_AP_MODE

uint32_t get_tid() {
  return (uint32_t)xTaskGetCurrentTaskHandle();
}

static void initWiFi() {
#ifdef ENABLE_AP_MODE
  WiFi.softAP("esp", "1029384756");
#else
  WiFi.begin("ikun", "1029384756");
  int count = 0;
  while (!WiFi.isConnected()) {
    if (count++ > 10) {
      esp_restart();
    }
    delay(1000);
    LOG(".");
  }
#endif
}

static std::string getIp() {
  if (WiFi.isConnected()) {
    return WiFi.localIP().toString().c_str();
  } else {
    return WiFi.softAPIP().toString().c_str();
  }
}

static void dumpWiFiInfo() {
  LOGI("IP address: %s", getIp().c_str());
  Serial.println();
}

void setup() {
  Serial.begin(115200);

  // init
  dht.begin();
  Wire.begin(GPIO_NUM_7, GPIO_NUM_8);
  aht.begin();

  // config wifi
  initWiFi();
  dumpWiFiInfo();

  // ntp
  sntp_env_init();
  sntp_obtain_time();
}

void push_data(float temperature, float humidity) {
  JsonDocument doc;
  doc["timestamps"] = utils::getTimestamps();
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  String json_str;
  serializeJson(doc, json_str);

  HTTPClient client;
  client.begin("http://thingsboard.cloud/api/v1/4oGxrTz1NVZeBrK2lqPp/telemetry");
  client.addHeader("Content-Type", "application/json");
  auto ret = client.POST(json_str);
  LOGI("http ret: %d %s", ret, client.errorToString(ret).c_str());
  client.end();
}

void process_dht() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  LOGI("Temperature: %f", t);
  LOGI("humidity: %f", h);
}

void process_aht() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);  // populate temp and humidity objects with fresh data
  LOGI("Temperature: %f", temp.temperature);
  LOGI("humidity: %f", humidity.relative_humidity);

  push_data(temp.temperature, humidity.relative_humidity);
}

void loop() {
  delay(2000);

  LOGI("process_dht");
  process_dht();
  LOGI("process_aht");
  process_aht();
}
