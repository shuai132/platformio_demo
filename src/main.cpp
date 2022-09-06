#include <HardwareSerial.h>
#include <IPv6Address.h>
#include <WiFiAP.h>
#include <WiFiSTA.h>

#include "RpcCore.hpp"
#include "tcp_server.hpp"

using namespace RpcCore;
using namespace asio_net;

#define LOG_TAG "main"

#define WIFI_USE_AP
//#define WIFI_USE_STA

asio::io_context context;
std::shared_ptr<RpcCore::Rpc> rpc;
std::unique_ptr<tcp_server> server;

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
#endif

static void initRpcTask() {
  rpc->subscribe("hello", [] {
    return "world";
  });
}

void setup() {
  Serial.begin(115200);
  pinMode(BLINK_LED, OUTPUT);

  initWiFi();

  std::thread([] {
    context.poll();
  }).detach();

  server = std::make_unique<tcp_server>(context, 8080);

  server->on_session = [](const std::weak_ptr<tcp_session>& ws) {
    ESP_LOGE(LOG_TAG, "on_session:\n");

    if (rpc) return;

    rpc = RpcCore::Rpc::create();
    rpc->setTimer([](uint32_t ms, RpcCore::Rpc::TimeoutCb cb) {
      auto timer = std::make_shared<asio::steady_timer>(context);
      timer->expires_after(std::chrono::milliseconds(ms));
      timer->async_wait([timer = std::move(timer), cb = std::move(cb)](asio::error_code) {
        cb();
      });
    });
    rpc->getConn()->sendPackageImpl = [ws](std::string data) {
      ws.lock()->send(std::move(data));
    };

    auto session = ws.lock();
    session->on_close = [] {
      ESP_LOGE(LOG_TAG, "session on_close:\n");
      rpc = nullptr;
    };
    session->on_data = [ws](std::string data) {
      ESP_LOGE(LOG_TAG, "session on_data: %s\n", data.c_str());
      rpc->getConn()->onRecvPackage(std::move(data));
    };

    initRpcTask();
  };
}

void loop() {
  delay(3000);
  dumpWiFiInfo();
}
