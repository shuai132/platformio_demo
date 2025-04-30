#include "utils.h"

#include <Arduino.h>

#include <cctype>

#include "log.h"

void debug_print_hex(const char* tag, const void* data, size_t size) {
  const auto* bytes = (const unsigned char*)data;

  L_O_G_PRINTF("%s HEX:", tag);
  for (size_t i = 0; i < size; ++i) {
    L_O_G_PRINTF(" %02X", bytes[i]);
  }
  L_O_G_PRINTF("\r\n");

  L_O_G_PRINTF("%s ASC:", tag);
  for (size_t i = 0; i < size; ++i) {
    unsigned char c = bytes[i];
    L_O_G_PRINTF("  %c", isprint(c) ? c : '.');
  }
  L_O_G_PRINTF("\r\n");
}
