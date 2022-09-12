#include <DNSServer.h>
#include <ESP_WiFiManager.h>
#include <HardwareSerial.h>
#include <IPv6Address.h>
#include <WiFi.h>
#include <WiFiSTA.h>
#include <esp_pthread.h>

#include "led.h"
#include "log.h"
#include "rpc_server.hpp"
#include "server_discovery.hpp"

using namespace RpcCore;
using namespace asio_net;

static ESP_WiFiManager wifiManager;
static std::shared_ptr<RpcCore::Rpc> rpc;
static const short PORT = 8080;

static void initWiFi() {
  LOGI("start wifi manager...");
  // wifiManager.resetSettings();
  wifiManager.setDebugOutput(true);
  wifiManager.setTimeout(10);
  wifiManager.autoConnect("001");

  LOGI("check connect...");
  if (!WiFi.isConnected()) {
    LOGI("not connect, start ESP...");
    WiFi.softAP("002", "1029384756");
  }
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

static void initRpcTask() {
  rpc->subscribe<RpcCore::String, RpcCore::String>("cmd", [](const RpcCore::String& data) {
    LOGI("from rpc: %s", data.c_str());
    return data;
  });
  rpc->subscribe<Void, RpcCore::Struct<LEDState>>("getState", [](const Void&) {
    auto s = ledGetState();
    LOGI("getState: %d, %d, %d, %d", s.led1, s.led2, s.led3, s.led4);
    return s;
  });
  rpc->subscribe<RpcCore::Struct<LEDState>>("setState", [](const RpcCore::Struct<LEDState>& data) {
    auto s = data.value;
    LOGI("setState: %d, %d, %d, %d", s.led1, s.led2, s.led3, s.led4);
    ledSetState(s);
    ledOff();
    ledOn();
  });
  rpc->subscribe("on", [] {
    LOGI("set on");
    ledOff();
    ledOn();
  });
  rpc->subscribe("off", [] {
    LOGI("set off");
    ledOff();
  });
}

void setup() {
  Serial.begin(115200);

  // config wifi
  initWiFi();
  dumpWiFiInfo();

  // io init
  ledInit();

  // start rpc task
  esp_pthread_cfg_t cfg{1024 * 40, 5, false, "rpc", tskNO_AFFINITY};
  esp_pthread_set_cfg(&cfg);
  std::thread([] {
    asio::io_context context;
    std::unique_ptr<rpc_server> server;

    server_discovery::sender sender(context, "ip", getIp() + ":" + std::to_string(PORT));

    server = std::make_unique<rpc_server>(context, PORT);
    server->on_session = [](const std::weak_ptr<rpc_session>& ws) {
      LOGD("on_session");

      if (rpc) {
        ws.lock()->close();
        return;
      }

      auto session = ws.lock();
      session->on_close = [] {
        LOGD("session: on_close");
        rpc = nullptr;
      };

      rpc = session->rpc;
      initRpcTask();
    };
    LOGD("asio running...");
    server->start(true);
  }).detach();
}

void loop() {
  delay(1000);
}
