#ifdef MAIN_LORA_MESH

#include "lora_mesh.h"

#include <Arduino.h>
#include <RadioLib.h>

#include "SimpleTimer.h"
#include "log.h"
#include "utils.h"

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
static mesh_core::recv_handle_t recv_handle;
static volatile bool received_flag = false;

static void lora_start_recv();

///  ********** lora mesh **********  ///
void LoraMeshImpl::broadcast(std::string data) {
  LOGD("broadcast:");
  LOGD_HEX_H(data.data(), data.size());
  LOGD_HEX_D(data.data(), data.size());
  lora_send(std::move(data));
}
void LoraMeshImpl::set_recv_handle(mesh_core::recv_handle_t handle) {
  lora_on_recv(std::move(handle));
}
mesh_core::timestamp_t LoraMeshImpl::get_timestamp_ms() {
  return HAL_GetTick();
}
void LoraMeshImpl::run_delay(std::function<void()> handle, int ms) {
  timer.setTimeout(ms, std::move(handle));
}
static LoraMeshImpl lora_mesh_impl;
mesh_core::mesh<LoraMeshImpl> lora_mesh(&lora_mesh_impl);
///  ********** lora mesh **********  ///

static void system_reset() {
  delay(1000);
  HAL_NVIC_SystemReset();
}

static void lora_start_recv() {
  // set the function that will be called when new packet is received
  radio.setPacketReceivedAction([] {
    received_flag = true;
  });

  // start listening for LoRa packets
  LOGV("starting to listen...");
  int state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    LOGV("start recv: success!");
  } else {
    LOGE("start recv: failed, code: %d", state);
    system_reset();
  }
}

void lora_init() {
  LOGD("lora init...");
  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);
  digitalWrite(PIN_LED_1, LOW);
  digitalWrite(PIN_LED_2, LOW);

  if (auto state = radio.reset(true); state != RADIOLIB_ERR_NONE) {
    LOGE("reset failed, code: %d", state);
    system_reset();
  }

  int state = radio.begin();
  radio.setOutputPower(PA_POWER_DBM);
  if (state == RADIOLIB_ERR_NONE) {
    LOGD("init success");
  } else {
    LOGE("init failed, code: %d", state);
    system_reset();
  }

  // enable CPS, RF
  pinMode(PIN_CPS, OUTPUT);
  pinMode(PIN_RF, OUTPUT);
  digitalWrite(PIN_CPS, PA_PIN_VALUE);
  digitalWrite(PIN_RF, HIGH);
}

void lora_on_recv(mesh_core::recv_handle_t handle) {
  recv_handle = std::move(handle);
}

void lora_send(std::string data, int retry_count, int retry_delay_ms) {
  bool ok = lora_try_send((uint8_t*)data.data(), data.size());
  if (!ok) {
    if (retry_count-- <= 0) return;
    timer.setTimeout(retry_delay_ms, [=, data = std::move(data)]() mutable {
      LOGD("send: retry_count: %d", retry_count);
      lora_send(std::move(data), retry_count, retry_delay_ms);
    });
  }
}

bool lora_try_send(const uint8_t* data, size_t size) {
  // must clear irq, avoid recv trigger!
  radio.clearPacketReceivedAction();

  /// check channel free
  auto scan = radio.scanChannel();
  if (scan == RADIOLIB_CHANNEL_FREE) {
    LOGD("send: channel free");
  } else {
    LOGD("send: channel not free: %d", scan);
    lora_start_recv();
    return false;
  }

  // debug
  LOGD("=> SEND");
  LOGD_HEX_H(data, size);
  LOGD_HEX_D(data, size);

  /// send
  digitalWrite(PIN_LED_1, HIGH);
  int state = radio.transmit(data, size);
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

  lora_start_recv();
  return true;
}

void lora_loop() {
  timer.run();

  // check if the flag is set
  if (received_flag) {
    // reset flag
    received_flag = false;

    // read data
    digitalWrite(PIN_LED_2, HIGH);
    uint8_t buffer[256]{};
    auto bytes = radio.getPacketLength();
    int state = radio.readData(buffer, bytes);
    digitalWrite(PIN_LED_2, LOW);

    if (state == RADIOLIB_ERR_NONE) {
      // packet was successfully received
      if (bytes == 0) return;
      LOGD("SIZE: %u", bytes);
      LOGD("RSSI: %d dBm", (int)radio.getRSSI());
      LOGD("SNR: %d dB", (int)radio.getSNR());
      LOGD("FE: %d Hz", (int)(radio.getFrequencyError()));
      LOGD("<= DATA");
      LOGD_HEX_H(buffer, bytes);
      LOGD_HEX_D(buffer, bytes);
      if (recv_handle) recv_handle(std::string((char*)buffer, bytes), (mesh_core::lqs_t)radio.getSNR());
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      LOGD("CRC error!");
    } else {
      LOGD("recv failed, code %d", state);
    }
  }
}

#endif
