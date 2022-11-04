#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESP_WiFiManager.h>
#include <HardwareSerial.h>

#include "SimpleTimer.h"
#include "log.h"
#include "rpc_client.hpp"
#include "rpc_server.hpp"

using namespace RpcCore;
using namespace esp_rpc;

#define TEST_RPC_CLIENT 0

static SimpleTimer timer;
static const short PORT = 8080;

namespace esp_rpc {
void setTimeout(uint32_t ms, std::function<void()> cb) {
  timer.setTimeout(ms, std::move(cb));
}
}  // namespace esp_rpc

// 启动函数
static void test_rpc_client() {
  static char ssid[] = "MI9";
  static char pass[] = "88888888";

  Serial.println("Start STA_Mode");
  WiFi.mode(WIFI_STA);
  if (WiFi.begin(ssid, pass) == WL_CONNECT_FAILED) {
    Serial.println("STA_Mode config failed");
  } else {
    Serial.println("STA_Mode is config successful");
  }
  Serial.println(::String(ssid) + " connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
  }
  MDNS.begin("esp");
  Serial.println("\r\nWiFi connected");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());
  Serial.println(WiFi.subnetMask());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.psk());

  // rpc client
  static rpc_client client;
  static std::shared_ptr<RpcCore::Rpc> rpc;
  client.on_open = [](std::shared_ptr<RpcCore::Rpc> rpc_) {
    LOGE("client: on_open");
    rpc = std::move(rpc_);
    rpc->cmd("cmd")->msg(RpcCore::String("hello"))->rsp([&](const RpcCore::String& data) {})->call();
  };
  client.on_close = [] {
    LOGE("client: on_close");
    rpc = nullptr;
  };
  client.on_open_failed = [](std::error_code ec) {
    LOGE("client: on_open_failed: %d", ec.value());
  };

  timer.setInterval(1000, [] {
    if (rpc) return;
    LOGE("client: try open...");
    client.open("192.168.178.115", PORT);
  });
}

static void test_rpc_server() {
#define ENABLE_AP_ONLY
#ifdef ENABLE_AP_ONLY
  WiFi.softAP("002", "1029384756");
#else
  LOGI("start wifi manager...");
  static ESP_WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  wifiManager.setTimeout(10);
  wifiManager.autoConnect("Mi9", "88888888");

  LOGI("check connect...");
  if (!WiFi.isConnected()) {
    LOGI("not connect, start ESP...");
    WiFi.softAP("002", "1029384756");
  }
#endif

  static auto getIp = []() -> std::string {
    if (WiFi.isConnected()) {
      return WiFi.localIP().toString().c_str();
    } else {
      return WiFi.softAPIP().toString().c_str();
    }
  };
  LOGI("IP address: %s", getIp().c_str());
  Serial.println();

  LOGI("start rpc server...");
  static std::unique_ptr<rpc_server> server;
  static std::shared_ptr<RpcCore::Rpc> rpc;

  // start rpc task
  server = std::make_unique<rpc_server>(PORT);
  server->on_session = [](const std::weak_ptr<rpc_session>& ws) {
    LOGI("on_session");

    if (rpc) {
      LOGI("rpc exist, will close new session");
      ws.lock()->close();
      return;
    }

    auto session = ws.lock();
    session->on_close = [] {
      LOGI("session: on_close");
      rpc = nullptr;
    };

    rpc = session->rpc;
    rpc->subscribe("cmd", [](const RpcCore::String& data) -> RpcCore::String {
      LOGI("get cmd: cmd: %s", data.c_str());
      return "world";
    });
  };
  LOGI("rpc start...");
  server->start();
}

void setup() {
  Serial.begin(115200);

  if (TEST_RPC_CLIENT) {  // NOLINT
    test_rpc_client();
  } else {
    test_rpc_server();
  }
}

void loop() {
  timer.run();
}
