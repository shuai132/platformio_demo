#if defined(ESP8266)

#include <interrupts.h>

#include <cstdint>

// The ESP8266 toolchain lacks these libatomic entry points used by coro's
// callback awaiter. Interrupt exclusion serializes them on this single-core MCU;
// compiler barriers provide ordering for all requested memory orders.
// GCC's libatomic compare-exchange ABI has no "weak" parameter:
// https://github.com/gcc-mirror/gcc/blob/releases/gcc-10.3.0/libatomic/cas_n.c
extern "C" uint32_t arduino_atomic_exchange_4(volatile void* ptr, uint32_t value, int order) __asm__("__atomic_exchange_4");
extern "C" bool arduino_atomic_compare_exchange_4(volatile void* ptr, void* expected, uint32_t desired, int success_order,
                                                  int failure_order) __asm__("__atomic_compare_exchange_4");

uint32_t arduino_atomic_exchange_4(volatile void* ptr, uint32_t value, int) {
  esp8266::InterruptLock lock;
  __asm__ __volatile__("" ::: "memory");
  auto* target = static_cast<volatile uint32_t*>(ptr);
  const auto previous = *target;
  *target = value;
  __asm__ __volatile__("" ::: "memory");
  return previous;
}

bool arduino_atomic_compare_exchange_4(volatile void* ptr, void* expected, uint32_t desired, int, int) {
  esp8266::InterruptLock lock;
  __asm__ __volatile__("" ::: "memory");
  auto* target = static_cast<volatile uint32_t*>(ptr);
  auto* expected_value = static_cast<uint32_t*>(expected);
  const auto previous = *target;
  const bool matched = previous == *expected_value;
  if (matched) {
    *target = desired;
  } else {
    *expected_value = previous;
  }
  __asm__ __volatile__("" ::: "memory");
  return matched;
}

#endif
