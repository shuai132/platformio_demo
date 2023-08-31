#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#elif defined(ESP32)
#include <ESPmDNS.h>
#include <WiFi.h>
#endif

#include <ESP_WiFiManager.h>
#include <HardwareSerial.h>

#include "SimpleTimer.h"
#include "TaskQueue.h"
#include "log.h"
#include "rpc_client.hpp"
#include "rpc_server.hpp"

using namespace rpc_core;
using namespace esp_rpc;

#define TEST_RPC_CLIENT 0

// #define USE_HOME_WIFI
#ifdef USE_HOME_WIFI
static char ssid[] = "ChinaNet-GiES";
static char pass[] = "6gj7qynr";
#define IP_ADDR "192.168.1.234"
#else
static char ssid[] = "MI6";
static char pass[] = "88888888";
#define IP_ADDR "192.168.178.115"
#endif

static const short PORT = 6666;

static SimpleTimer timer;
static TaskQueue taskQueue;

namespace esp_rpc {
void dispatch(std::function<void()> runnable) {
  taskQueue.post(std::move(runnable));
}

void set_timeout(uint32_t ms, std::function<void()> cb) {
  taskQueue.post([ms, cb = std::move(cb)]() mutable {
    timer.setTimeout(ms, std::move(cb));
  });
}
}  // namespace esp_rpc

static void printThread(const char* str) {
#ifdef ESP8266
  LOGI("printThread: msg: %s, with isr: %d", str, ETS_INTR_WITHINISR());
#elif defined(ESP32)
  LOGI("printThread: msg: %s, task: %p", str, xTaskGetCurrentTaskHandle());
#endif
}

static void test_rpc_client() {
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
  static std::shared_ptr<rpc_core::rpc> rpc;
  client.on_open = [](std::shared_ptr<rpc_core::rpc> rpc_) {
    LOGE("client: on_open");
    printThread("on_open");
    rpc = std::move(rpc_);
    rpc->cmd("cmd")->msg(std::string("hello"))->rsp([&](const std::string& data) {})->call();
  };
  client.on_close = [] {
    LOGE("client: on_close");
    printThread("on_close");
    rpc = nullptr;
  };
  client.on_open_failed = [](const std::error_code& ec) {
    printThread("on_open_failed");
    LOGE("client: on_open_failed: %d", ec.value());
  };

  taskQueue.post([] {
    timer.setInterval(1000, [] {
      if (rpc) return;
      LOGE("client: try open...");
      client.open(IP_ADDR, PORT);
    });
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
  static std::shared_ptr<rpc_core::rpc> rpc;

  // start rpc task
  server = std::make_unique<rpc_server>(PORT);
  server->on_session = [](const std::weak_ptr<rpc_session>& ws) {
    LOGI("on_session");
    printThread("on_session");
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
    rpc->subscribe("cmd", [](const std::string& data) -> std::string {
      LOGI("get cmd: cmd: %s", data.c_str());
      return "world";
    });
  };
  LOGI("rpc start...");
  server->start();
}

void setup() {
  Serial.begin(115200);
  printThread("setup");
  if (TEST_RPC_CLIENT) {  // NOLINT
    test_rpc_client();
  } else {
    test_rpc_server();
  }
}

void loop() {
  taskQueue.poll();
  timer.run();
  delay(10);
}
