#include <DNSServer.h>
#include <ESP_WiFiManager.h>
#include <HardwareSerial.h>
#include <IPv6Address.h>
#include <WiFi.h>
#include <WiFiSTA.h>
#include <esp_pthread.h>

#define BITTLE   // Petoi 9 DOF robot dog: 1 on head + 8 on leg
#define BiBoard  // ESP32 Board with 12 channels of built-in PWM for joints
#include "OpenCat.h"
#include "asio_net/rpc_server.hpp"
#include "asio_net/server_discovery.hpp"
#include "log.h"

using namespace asio_net;

static ESP_WiFiManager wifiManager;
static std::shared_ptr<rpc_core::rpc> rpc;
static const short PORT = 8080;
static asio::io_context context;

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

static void startRpcTask() {
  LOGI("startRpcTask");
  // start rpc task
  rpc = rpc_core::rpc::create();
  rpc->subscribe("on", [] {
    LOGI("on");
  });
  rpc->subscribe("off", [] {
    LOGI("off");
  });
  rpc->subscribe("cmd", [](const std::string& cmd) {
    LOGI("cmd: %s", cmd.c_str());
    tQueue->addTask('k', cmd.c_str());
  });

  static server_discovery::sender sender(context, "ip", getIp() + ":" + std::to_string(PORT));
  static rpc_server server(context, PORT, rpc_config{.rpc = rpc});
  server.on_session = [](const std::weak_ptr<rpc_session>& ws) {
    LOGD("on_session");
  };
  LOGD("asio running...");
  server.start();
}

void setup() {
  Serial.begin(115200);

  // config wifi
  initWiFi();
  dumpWiFiInfo();
  startRpcTask();

  initRobot();
}

void loop() {
  if (!tQueue->cleared()) {
    tQueue->popTask();
  } else {
    readSignal();
  }
  reaction();

  context.poll();
  context.restart();
}
