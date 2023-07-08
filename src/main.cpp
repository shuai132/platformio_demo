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

#define ENABLE_AP_ONLY

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
    rpc->subscribe("on", [] {
      LOGI("set on");
      led_on();
    });
    rpc->subscribe("off", [] {
      LOGI("set off");
      led_off();
    });
  };
  LOGD("asio running...");
  server->start(true);
}

void loop() {
  LOGE("never get here");
}
