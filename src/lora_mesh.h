#pragma once

#include <Arduino.h>

#include <functional>
#include <string>

#include "lora_config.h"
#include "mesh_core.hpp"

struct LoraMeshImpl {
  static void broadcast(std::string data);
  static void set_recv_handle(mesh_core::recv_handle_t handle);
  static mesh_core::timestamp_t get_timestamp_ms();
  static void run_delay(std::function<void()> handle, int ms);
};
extern mesh_core::mesh<LoraMeshImpl> lora_mesh;

void lora_init();
void lora_loop();

void lora_send(std::string data, int retry_count = 3, int retry_delay_ms = 500);
bool lora_try_send(const uint8_t* data, size_t size);
void lora_on_recv(mesh_core::recv_handle_t handle);
