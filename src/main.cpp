#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP_WiFiManager.h>
#include <HardwareSerial.h>

#include "SimpleTimer.h"
#include "log.h"
#include "rpc_client.hpp"
#include "rpc_server.hpp"

using namespace RpcCore;
using namespace esp_rpc;

static SimpleTimer timer;

namespace esp_rpc {
void setTimeout(uint32_t ms, std::function<void()> cb) {
  timer.setTimeout(ms, std::move(cb));
}
}  // namespace esp_rpc

static ESP_WiFiManager wifiManager;
static std::unique_ptr<rpc_server> server;
static std::shared_ptr<RpcCore::Rpc> rpc;
static const short PORT = 8080;

#define ENABLE_AP_ONLY

static void initWiFi() {
#ifdef ENABLE_AP_ONLY
  WiFi.softAP("002", "1029384756");
#else
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

static void initRpcTask() {
  rpc->subscribe("cmd", [](const RpcCore::String& data) -> RpcCore::String {
    LOGI("get cmd: cmd: %s", data.c_str());
    return "world";
  });
}

static void client_test() {
  // client
  rpc_client client;
  client.on_open = [&](const std::shared_ptr<RpcCore::Rpc>& rpc) {
    rpc->cmd("cmd")->msg(RpcCore::String("hello"))->rsp([&](const RpcCore::String& data) {})->call();
  };
  client.on_close = [&] {};
  client.open("localhost", std::to_string(PORT));
}

void setup() {
  Serial.begin(115200);

  // config wifi
  initWiFi();
  dumpWiFiInfo();

  // start rpc task
  server = std::make_unique<rpc_server>(PORT);
  server->on_session = [](const std::weak_ptr<rpc_session>& ws) {
    LOGD("on_session");

    if (rpc) {
      LOGD("rpc exist, will close new session");
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
  LOGD("rpc start...");
  server->start();
}

void loop() {
  timer.run();
}
