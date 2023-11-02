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

static std::unique_ptr<rpc_server> server;
static std::unique_ptr<server_discovery::sender> sender;
static std::shared_ptr<rpc_core::rpc> rpc;
static const short PORT = 8080;
static asio::io_context context;

uint32_t get_tid() {
  return (uint32_t)xTaskGetCurrentTaskHandle();
}

static std::string getIp() {
  if (WiFi.isConnected()) {
    return WiFi.localIP().toString().c_str();
  } else {
    return WiFi.softAPIP().toString().c_str();
  }
}

static void initWiFi() {
  WiFi.setAutoReconnect(true);
  WiFi.begin("MI6", "88888888");
  WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
    LOGI("wifi event: %d", event);
    if (event != ARDUINO_EVENT_WIFI_STA_GOT_IP) return;

    LOGI("wifi ip: %s", getIp().c_str());
    server = std::make_unique<rpc_server>(context, PORT, rpc_config{.rpc = rpc});
    server->on_session = [](const std::weak_ptr<rpc_session>& ws) {
      LOGD("on_session");
    };
    LOGD("server running...");
    server->start();
    sender = std::make_unique<server_discovery::sender>(context, "ip", getIp());
  });
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
}

void setup() {
  Serial.begin(115200);

  // config wifi
  initWiFi();
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
