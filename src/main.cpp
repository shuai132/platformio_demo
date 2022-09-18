#include <DNSServer.h>
#include <ESP_WiFiManager.h>
#include <HardwareSerial.h>
#include <IPv6Address.h>
#include <WiFi.h>
#include <WiFiSTA.h>
#include <esp_pthread.h>

#include <nvs_handle.hpp>

#include "led.h"
#include "log.h"
#include "rpc_server.hpp"
#include "server_discovery.hpp"

using namespace RpcCore;
using namespace asio_net;

static ESP_WiFiManager wifiManager;
static std::shared_ptr<RpcCore::Rpc> rpc;
static const short PORT = 8080;

const static char* NS_NAME_MISC = "misc";
static bool ledBootOn;
static bool ledStateOn;

#define ENABLE_AP_ONLY

static void initBtn() {
  // check reset
  esp_pthread_cfg_t cfg{1024 * 40, 5, false, "btn", tskNO_AFFINITY};
  esp_pthread_set_cfg(&cfg);
  std::thread([] {
    const auto pin = GPIO_NUM_9;
    pinMode(pin, INPUT);
    for (;;) {
      if (digitalRead(pin) == 0) {
        LOGI("reset wifi...");
        wifiManager.resetSettings();
      }
      sleep(1);
    }
  }).detach();
}

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

static void initRpcTask(asio::io_context& context) {
  rpc->subscribe<Void, RpcCore::Struct<AllState>>("getState", [](const Void&) {
    AllState state{
        .ledState = ledGetState(),
        .ledBootOn = ledBootOn,
        .ledStateOn = ledStateOn,
    };
    const auto& s = state.ledState;
    LOGI("getState: %d, %d, %d, %d", s.led1, s.led2, s.led3, s.led4);
    return state;
  });
  rpc->subscribe<RpcCore::Struct<LEDState>>("setState", [](const RpcCore::Struct<LEDState>& data) {
    auto s = data.value;
    LOGI("setState: %d, %d, %d, %d", s.led1, s.led2, s.led3, s.led4);
    ledSetState(s);
    if (ledStateOn) {
      ledOff();
      ledOn();
    }

    {
      auto nvs = nvs::open_nvs_handle(NS_NAME_MISC, NVS_READWRITE);
      nvs->set_blob("led_state", &s, sizeof(s));
      nvs->commit();
    }
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
  rpc->subscribe<RpcCore::Raw<uint8_t>>("set_boot_on", [](const RpcCore::Raw<uint8_t>& data) {
    uint8_t on = data.value;
    LOGI("set boot on: %d", on);
    auto nvs = nvs::open_nvs_handle(NS_NAME_MISC, NVS_READWRITE);
    nvs->set_item("led_boot_on", on);
    nvs->commit();
    ledBootOn = on;
  });
  rpc->subscribe<RpcCore::Raw<uint8_t>>("set_state_on", [](const RpcCore::Raw<uint8_t>& data) {
    uint8_t on = data.value;
    LOGI("set state on: %d", on);
    auto nvs = nvs::open_nvs_handle(NS_NAME_MISC, NVS_READWRITE);
    nvs->set_item("led_state_on", on);
    nvs->commit();
    ledStateOn = on;
  });
  rpc->subscribe<RpcCore::Raw<uint16_t>>("set_on_time", [&context](const RpcCore::Raw<uint16_t>& data) {
    uint16_t sec = data.value;
    LOGI("set on time: %d", sec);
    auto timer = std::make_shared<asio::steady_timer>(context);
    timer->expires_after(std::chrono::seconds(sec));
    ledOff();
    ledOn();
    timer->async_wait([=](std::error_code e) mutable {
      LOGI("timeout led off");
      ledOff();
      timer = nullptr;
    });
  });
}

void setup() {
  Serial.begin(115200);

  // led init
  ledInit();

  // nvs
  {
    auto nvs = nvs::open_nvs_handle(NS_NAME_MISC, NVS_READWRITE);
    LEDState state{};
    nvs->get_blob("led_state", &state, sizeof(state));
    nvs->get_item("led_boot_on", ledBootOn);
    nvs->get_item("led_state_on", ledStateOn);
    ledSetState(state);
    if (ledBootOn) {
      ledOn();
    }
    auto& s = state;
    LOGI("init state: %d, %d, %d, %d", s.led1, s.led2, s.led3, s.led4);
    LOGI("boot on: %d", ledBootOn);
    LOGI("state on: %d", ledStateOn);
  }

  // config wifi
  initBtn();
  initWiFi();
  dumpWiFiInfo();

  // start rpc task
  esp_pthread_cfg_t cfg{1024 * 40, 5, false, "rpc", tskNO_AFFINITY};
  esp_pthread_set_cfg(&cfg);
  std::thread([] {
    asio::io_context context;
    std::unique_ptr<rpc_server> server;

    server_discovery::sender sender(context, "ip", getIp() + ":" + std::to_string(PORT));

    server = std::make_unique<rpc_server>(context, PORT);
    server->on_session = [&context](const std::weak_ptr<rpc_session>& ws) {
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
      initRpcTask(context);
    };
    LOGD("asio running...");
    server->start(true);
  }).detach();
}

void loop() {
  delay(1000);
}
