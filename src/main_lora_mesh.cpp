#ifdef MAIN_LORA_MESH

#include <Arduino.h>
#include <RadioLib.h>

#include <string>

#include "SimpleTimer.h"
#include "log.h"
#include "mesh_core.hpp"

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

    // you can read received data as an Arduino String
    String str;
    int state = radio.readData(str);

    // you can also read received data as byte array
    /*
      byte byteArr[8];
      int numBytes = radio.getPacketLength();
      int state = radio.readData(byteArr, numBytes);
    */

    if (state == RADIOLIB_ERR_NONE) {
      // packet was successfully received
      LOGD("RECV: %s", str.c_str());
      LOGD("RSSI: %f dBm", radio.getRSSI());
      LOGD("SNR: %f dB", radio.getSNR());
      LOGD("Frequency error: %f Hz", radio.getFrequencyError());
      if (recv_handle) recv_handle(std::string(str.begin(), str.end()));
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
  radio.setOutputPower(0);
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
  digitalWrite(PIN_CPS, HIGH);
  digitalWrite(PIN_RF, HIGH);

  // set the function that will be called when new packet is received
  radio.setPacketReceivedAction([] {
    received_flag = true;
  });
}

static void lora_start_recv() {
  // start listening for LoRa packets
  LOGD("starting to listen...");
  int state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    LOGD("start recv: success!");
  } else {
    LOGE("start recv: failed, code: %d", state);
    while (true) {
      delay(10);
    }
  }
}

// counter to keep track of transmitted packets
static void lora_send() {
  LOGD("send...");

  // you can transmit C-string or Arduino string up to
  // 256 characters long
  String str = "Hello World! #" + String(send_count++);
  digitalWrite(PIN_LED_1, HIGH);
  int state = radio.transmit(str);
  digitalWrite(PIN_LED_1, LOW);

  // you can also transmit byte array up to 256 bytes long
  /*
    byte byteArr[] = {0x01, 0x23, 0x45, 0x56, 0x78, 0xAB, 0xCD, 0xEF};
    int state = radio.transmit(byteArr, 8);
  */
  if (state == RADIOLIB_ERR_NONE) {
    // the packet was successfully transmitted
    LOGD("send: success!");
    LOGD("data rate: %f bps", radio.getDataRate());
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
  DEBUG_SERIAL.begin(115200);
  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);

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
}

#endif
