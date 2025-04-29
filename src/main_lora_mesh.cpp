#ifdef MAIN_LORA_MESH

#include <Arduino.h>
#include <RadioLib.h>

#include <string>

#include "SimpleTimer.h"
#include "log.h"
#include "mesh_core.hpp"

#define DEVICE_ID 0x01

// PA config
#define PA_PIN_VALUE LOW
#define PA_POWER_DBM 0

#define PIN_CPS PB13
#define PIN_RF PA1

#define PIN_LED_1 PB6
#define PIN_LED_2 PB5

#define PIN_NSS PA4
#define PIN_DIO1 PB0
#define PIN_NRST PB15
#define PIN_BUSY PA2
static LLCC68 radio = new Module(PIN_NSS, PIN_DIO1, PIN_NRST, PIN_BUSY);

static SimpleTimer timer;
static std::function<void(std::string)> recv_handle;
static volatile bool received_flag = false;
static int send_count = 0;

struct Impl {
  static void broadcast(std::string data) {
    radio.transmit(data.data(), data.size());
  }

  static void set_recv_handle(std::function<void(std::string)> handle) {
    recv_handle = std::move(handle);
  }

  static mesh_core::timestamp_t get_timestamp_ms() {
    return HAL_GetTick();
  }

  static void run_delay(std::function<void()> handle, int ms) {
    timer.setTimeout(ms, std::move(handle));
  }
};

static int mesh_core_test() {
  Impl impl;
  mesh_core::mesh<Impl> mesh(&impl);
  mesh.set_addr(0x00);
  mesh.on_recv([](mesh_core::addr_t addr, const mesh_core::data_t& data) {
    MESH_CORE_LOG("addr: 0x%02X, data: %s", addr, data.c_str());
  });
  mesh.send(0x01, "hello");
  return 0;
}

static void loop_check_recv() {
  // check if the flag is set
  if (received_flag) {
    // reset flag
    received_flag = false;

    uint8_t buffer[256]{};
    auto bytes = radio.getPacketLength();
    int state = radio.readData(buffer, bytes);
    if (state == RADIOLIB_ERR_NONE) {
      // packet was successfully received
      if (bytes == 0) return;
      LOGD("SIZE: %u", bytes);
      LOGD("DATA: %s", buffer);
      LOGD("RSSI: %d dBm", (int)radio.getRSSI());
      LOGD("SNR: %d dB", (int)radio.getSNR());
      LOGD("FE: %d Hz", (int)(radio.getFrequencyError()));
      if (recv_handle) recv_handle(std::string((char*)buffer, bytes));
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      LOGD("CRC error!");
    } else {
      LOGD("recv failed, code %d", state);
    }
  }
}

static void lora_init() {
  LOGD("lora init...");
  int state = radio.begin();
  radio.setOutputPower(PA_POWER_DBM);
  if (state == RADIOLIB_ERR_NONE) {
    LOGD("init success");
  } else {
    LOGE("init failed, code: %d", state);
    while (true) {
      delay(10);
    }
  }

  // enable CPS, RF
  pinMode(PIN_CPS, OUTPUT);
  pinMode(PIN_RF, OUTPUT);
  digitalWrite(PIN_CPS, PA_PIN_VALUE);
  digitalWrite(PIN_RF, HIGH);

  // set the function that will be called when new packet is received
  radio.setPacketReceivedAction([] {
    received_flag = true;
  });
}

static void lora_start_recv() {
  // start listening for LoRa packets
  LOGV("starting to listen...");
  int state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    LOGV("start recv: success!");
  } else {
    LOGE("start recv: failed, code: %d", state);
    while (true) {
      delay(10);
    }
  }
}

static void lora_send() {
  LOGD("send...");

  uint8_t buffer[128]{};
  size_t size = snprintf((char*)buffer, sizeof(buffer), "Hello from: 0x%02X, %d", DEVICE_ID, send_count++);

  /// check channel free
  auto scan = radio.scanChannel();
  if (scan == RADIOLIB_CHANNEL_FREE) {
    LOGD("scan channel: free");
  } else if (scan == RADIOLIB_LORA_DETECTED) {
    LOGW("scan channel: busy, cancel send!");
    return;
  }

  /// send
  digitalWrite(PIN_LED_1, HIGH);
  int state = radio.transmit(buffer, size);
  digitalWrite(PIN_LED_1, LOW);
  // send result
  if (state == RADIOLIB_ERR_NONE) {
    // the packet was successfully transmitted
    LOGD("send: ok! rate: %d bps", (int)radio.getDataRate());
  } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    // the supplied packet was longer than 256 bytes
    LOGD("send: too long!");
  } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
    // timeout occurred while transmitting packet
    LOGD("send: timeout!");
  } else {
    // some other error occurred
    LOGE("send: failed, code: %d", state);
  }
}

void setup() {
  // init io
  DEBUG_SERIAL.begin(DEBUG_BAUDRATE);
  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);
  LOGD("DEVICE_ID: 0x%02X", DEVICE_ID);
  delay(random(200, 500));

  // init lora
  lora_init();
  lora_start_recv();

  // send test
  timer.setInterval(1000, [] {
    lora_send();
    lora_start_recv();
  });
}

void loop() {
  timer.run();
  loop_check_recv();
}

#endif
