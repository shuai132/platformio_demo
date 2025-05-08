#ifdef MAIN_LORA_MESH

#include <Arduino.h>

#include <string>

#include "SimpleTimer.h"
#include "log.h"
#include "lora_mesh.h"

static SimpleTimer timer;

static void start_send_test() {
  static int send_count = 0;
  timer.setInterval(1000, [] {
    uint8_t data[128]{};
    size_t size = snprintf((char*)data, sizeof(data), "Hello from: 0x%02X, %d", DEVICE_ID, send_count++);
    lora_mesh.send(SEND_TEST_TO_ADDR, std::string((char*)data, size));
  });
}

void setup() {
  // init serial
  DEBUG_SERIAL.begin(DEBUG_BAUDRATE);

  // init device
  LOGD("DEVICE_ID: 0x%02X", DEVICE_ID);
  delay(DEVICE_ID * 100 % 1000);

  // init lora
  lora_init();

  // init mesh
  lora_mesh.init(DEVICE_ID);
  lora_mesh.on_recv([](mesh_core::addr_t addr, const mesh_core::data_t& data) {
    LOGD("mesh: addr: 0x%02X, data: %s", addr, data.c_str());
  });

  start_send_test();
}

void loop() {
  timer.run();
  lora_loop();
}

#endif
