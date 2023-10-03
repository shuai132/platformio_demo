#include <DNSServer.h>
#include <ESP_WiFiManager.h>
#include <HardwareSerial.h>
#include <IPv6Address.h>
#include <WiFi.h>
#include <WiFiSTA.h>
#include <esp_pthread.h>

#include "asio_net/rpc_server.hpp"
#include "asio_net/server_discovery.hpp"
#include "led.h"
#include "log.h"

using namespace asio_net;

static ESP_WiFiManager wifiManager;
static std::shared_ptr<rpc_core::rpc> rpc;
static const short PORT = 8080;

#define ENABLE_AP_ONLY

uint32_t get_tid() {
  return (uint32_t)xTaskGetCurrentTaskHandle();
}

static void initWiFi() {
#ifdef ENABLE_AP_ONLY
  WiFi.softAP("002", "1029384756");
#else
  // LOGI("reset wifi...");
  // wifiManager.resetSettings();

  LOGI("start wifi manager...");
  wifiManager.setDebugOutput(true);
  wifiManager.setTimeout(10);
  wifiManager.autoConnect("001");

  LOGI("check connect...");
  if (!WiFi.isConnected()) {
    LOGI("not connect, start ESP...");
    WiFi.softAP("002", "1029384756");
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

  // led init
  led_init();

  // config wifi
  initWiFi();
  dumpWiFiInfo();

  //  // start rpc task
  //  esp_pthread_cfg_t cfg{1024 * 40, 5, false, "rpc", tskNO_AFFINITY};
  //  esp_pthread_set_cfg(&cfg);
  //  std::thread([] {
  //
  //  }).detach();
  rpc = rpc_core::rpc::create();
  rpc->subscribe("on", [] {
    LOGI("set on");
    led_on();
  });
  rpc->subscribe("off", [] {
    LOGI("set off");
    led_off();
  });

  asio::io_context context;
  server_discovery::sender sender(context, "ip", getIp() + ":" + std::to_string(PORT));

  rpc_server server(context, PORT, rpc_config{.rpc = rpc});
  server.on_session = [](const std::weak_ptr<rpc_session>& ws) {
    LOGD("on_session");
  };
  LOGD("asio running...");
  server.start(true);
}

void loop() {
  LOGE("never get here");
}
