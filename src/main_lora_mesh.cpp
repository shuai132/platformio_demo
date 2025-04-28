#ifdef MAIN_LORA_MESH

#include <Arduino.h>
#include <RadioLib.h>

#include "log.h"

// NSS pin:   PA4
// DIO1 pin:  PB4
// NRST pin:  PB15
// BUSY pin:  PB14
static LLCC68 radio = new Module(PA4, PB4, PB15, PB14);

#include "mesh_core.hpp"

/**
 * supply your implementation:
 *
 * NOTE:
 * 1. broadcast and recv_handle should ensure packet is complete
 * 2. all methods can be static
 */
struct Impl {
  /**
   * @param data binary data to broadcast
   */
  void broadcast(std::string data) {
    (void)(data);
  }

  /**
   * @param handle store it, call it on receive broadcast.
   */
  void set_recv_handle(std::function<void(std::string)> handle) {
    (void)(handle);
  }

  /**
   * @return millisecond timestamp
   */
  static mesh_core::timestamp_t get_timestamp_ms() {
    return 0;
  }

  /**
   * @param handle call it after `ms`
   * @param ms milliseconds
   */
  static void run_delay(std::function<void()> handle, int ms) {
    (void)(handle);
    (void)(ms);
  }
};

int mesh_core_test() {
  Impl impl;
  mesh_core::mesh<Impl> mesh(&impl);
  mesh.set_addr(0x00);
  mesh.on_recv([](mesh_core::addr_t addr, const mesh_core::data_t& data) {
    MESH_CORE_LOG("addr: 0x%02X, data: %s", addr, data.c_str());
  });
  mesh.send(0x01, "hello");
  return 0;
}

void lora_init() {
  LOGD("init...");
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    LOGD("init success");
  } else {
    LOGD("init failed, code: %d", state);
    while (true) {
      delay(10);
    }
  }
}

// counter to keep track of transmitted packets
int count = 0;

void lora_test() {
  LOGD("send...");

  // you can transmit C-string or Arduino string up to
  // 256 characters long
  String str = "Hello World! #" + String(count++);
  int state = radio.transmit(str);

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
    // timeout occured while transmitting packet
    LOGD("send: timeout!");
  } else {
    // some other error occurred
    LOGD("send: failed, code: %d", state);
  }

  // wait for a second before transmitting again
  delay(1000);
}

void setup() {
  Serial1.begin(115200);
  pinMode(BLINK_LED, OUTPUT);
  lora_init();
}

void loop() {
  LOG("mesh...");
  digitalWrite(BLINK_LED, HIGH);
  delay(100);
  digitalWrite(BLINK_LED, LOW);
  delay(100);
  lora_test();
}

#endif
