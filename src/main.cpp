#include <HardwareSerial.h>
#include <IPv6Address.h>
#include <WiFiAP.h>
#include <WiFiSTA.h>
#include <esp_pthread.h>

#include "log.h"
#include "rpc_server.hpp"
#include "server_discovery.hpp"

using namespace RpcCore;
using namespace asio_net;

#define WIFI_USE_AP
//#define WIFI_USE_STA

static const short PORT = 8080;

static std::shared_ptr<RpcCore::Rpc> rpc;

#ifdef WIFI_USE_AP
static WiFiAPClass wifiAP;
static void initWiFi() {
  wifiAP.softAP("ESP", "88888888");
}
static void dumpWiFiInfo() {
  Serial.print("AP IP address: ");
  Serial.println(wifiAP.softAPIP());

  Serial.print("softAP Broadcast IP: ");
  Serial.println(wifiAP.softAPBroadcastIP());

  Serial.print("softAP NetworkID: ");
  Serial.println(wifiAP.softAPNetworkID());

  Serial.print("softAP SubnetCIDR: ");
  Serial.println(wifiAP.softAPSubnetCIDR());

  Serial.print("softAP Hostname: ");
  Serial.println(wifiAP.softAPgetHostname());

  Serial.print("softAP macAddress: ");
  Serial.println(wifiAP.softAPmacAddress());

  Serial.print("softAP StationNum: ");
  Serial.println(wifiAP.softAPgetStationNum());
}

std::string getIp() {
  return wifiAP.softAPIP().toString().c_str();
}
#endif

#ifdef WIFI_USE_STA
static WiFiSTAClass wifiSTA;
static void initWiFi() {
  wifiSTA.begin("MI9", "88888888");
}
static void dumpWiFiInfo() {
  Serial.println("myip:");
  Serial.println(wifiSTA.localIP());
}
std::string getIp() {
  return wifiSTA.localIP().toString().c_str();
}
#endif

static void initRpcTask() {
  rpc->subscribe<RpcCore::String, RpcCore::String>("cmd", [](const RpcCore::String& data) {
    LOGI("from rpc: %s", data.c_str());
    return data;
  });
}

void setup() {
  Serial.begin(115200);

  initWiFi();
  dumpWiFiInfo();

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
