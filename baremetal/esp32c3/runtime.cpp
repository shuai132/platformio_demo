#include <sys/lock.h>
#include <sys/reent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <new>

#include "baremetal_c3.hpp"
extern "C" {
int ets_printf(const char*, ...);
int uart_tx_one_char(char);
void ets_update_cpu_frequency(uint32_t);
void uart_div_modify(uint8_t, uint32_t);
extern char __heap_start, __heap_end;
extern void (*__init_array_start[])();
extern void (*__init_array_end[])();
[[noreturn]] void _exit(int);
}
int main();
namespace {
char* heap_break = &__heap_start;
void hardware_init() {
  using pwm_platform::reg;
  // Disable RTC, super and timer-group watchdogs inherited from ROM boot.
  reg(0x600080a8) = 0x50d83aa1;
  reg(0x60008090) = 0;
  reg(0x600080a8) = 0;
  reg(0x600080b0) = 0x8f1d312a;
  reg(0x600080ac) = reg(0x600080ac) | (1U << 31);
  reg(0x600080b0) = 0;
  for (uintptr_t group : {uintptr_t(0x6001f000), uintptr_t(0x60020000)}) {
    reg(group + 0x64) = 0x50d83aa1;
    reg(group + 0x48) = 0;
    reg(group + 0x64) = 0;
  }
  reg(0x60023000) = reg(0x60023000) | (1U << 31) | (1U << 30) | 1;
  // Run directly from the 40 MHz crystal without the ROM boot divider.
  reg(0x600c0058) = reg(0x600c0058) & ~0xfffU;
  ets_update_cpu_frequency(40);
  const uint32_t apb = 40000000U >> 12;
  reg(0x600080bc) = apb | (apb << 16);
  uart_div_modify(0, (40000000U << 4) / 115200);
  asm volatile("csrw 0x7e0, %0\ncsrw 0x7e1, %0" ::"r"(1U));
  const auto us_start = pwm_platform::micros32();
  const auto ticks_start = pwm_platform::pulse_clock32();
  while (uint32_t(pwm_platform::micros32() - us_start) < 10000) {
  }
  const auto elapsed_us = uint32_t(pwm_platform::micros32() - us_start);
  const auto elapsed_ticks = uint32_t(pwm_platform::pulse_clock32() - ticks_start);
  pwm_platform::pulse_clock_mhz = (elapsed_ticks + elapsed_us / 2) / elapsed_us;
  if (pwm_platform::pulse_clock_mhz != 40) {
    ets_printf("baremetal: unexpected pulse clock %u MHz\n", pwm_platform::pulse_clock_mhz);
    _exit(3);
  }
}
}  // namespace
namespace pwm_platform {
void log(const char* message) {
  ets_printf("%s\n", message);
}
}  // namespace pwm_platform
extern "C" [[noreturn]] void baremetal_start() {
  hardware_init();
  for (auto* init = __init_array_start; init != __init_array_end; ++init) (*init)();
  ets_printf("baremetal C3: no RTOS, GPIO 3/4/5 PWM 1000 Hz, CPU %u MHz\n", pwm_platform::pulse_clock_mhz);
  _exit(main());
}
extern "C" [[noreturn]] void baremetal_trap(uint32_t cause, uint32_t pc, uint32_t value) {
  ets_printf("baremetal trap: cause=%x pc=%x value=%x\n", cause, pc, value);
  for (;;) asm volatile("nop");
}
extern "C" void* _sbrk(ptrdiff_t increment) {
  if ((increment > 0 && increment > &__heap_end - heap_break) || (increment < 0 && increment < &__heap_start - heap_break)) {
    errno = ENOMEM;
    return reinterpret_cast<void*>(-1);
  }
  auto* previous = heap_break;
  heap_break += increment;
  return previous;
}
extern "C" int _write(int, const char* data, int size) {
  for (int i = 0; i < size; ++i) uart_tx_one_char(data[i]);
  return size;
}
extern "C" int _read(int, char*, int) {
  return 0;
}
extern "C" int _close(int) {
  return -1;
}
extern "C" off_t _lseek(int, off_t, int) {
  return 0;
}
extern "C" int _fstat(int, struct stat* info) {
  info->st_mode = S_IFCHR;
  return 0;
}
extern "C" int _isatty(int) {
  return 1;
}
extern "C" int _getpid() {
  return 1;
}
extern "C" int _kill(int, int) {
  return -1;
}
extern "C" [[noreturn]] void _exit(int status) {
  ets_printf("baremetal exit: %d\n", status);
  for (;;) asm volatile("nop");
}
// No interrupts or threads: libc locks have no competing owner.
extern "C" void __retarget_lock_init(_LOCK_T* lock) {
  *lock = nullptr;
}
extern "C" void __retarget_lock_init_recursive(_LOCK_T* lock) {
  *lock = nullptr;
}
extern "C" void __retarget_lock_close(_LOCK_T) {}
extern "C" void __retarget_lock_close_recursive(_LOCK_T) {}
extern "C" void __retarget_lock_acquire(_LOCK_T) {}
extern "C" void __retarget_lock_acquire_recursive(_LOCK_T) {}
extern "C" void __retarget_lock_release(_LOCK_T) {}
extern "C" void __retarget_lock_release_recursive(_LOCK_T) {}
extern "C" int __retarget_lock_try_acquire(_LOCK_T) {
  return 1;
}
extern "C" int __retarget_lock_try_acquire_recursive(_LOCK_T) {
  return 1;
}
extern "C" struct _reent* __getreent() {
  return _impure_ptr;
}
struct __lock {
  uint32_t unused;
};
extern "C" {
struct __lock __lock___arc4random_mutex;
struct __lock __lock___at_quick_exit_mutex;
struct __lock __lock___atexit_recursive_mutex;
struct __lock __lock___dd_hash_mutex;
struct __lock __lock___env_recursive_mutex;
struct __lock __lock___malloc_recursive_mutex;
struct __lock __lock___sfp_recursive_mutex;
struct __lock __lock___tz_mutex;
}
void* operator new(std::size_t size) {
  if (void* result = std::malloc(size ? size : 1)) return result;
  pwm_platform::log("baremetal: out of heap");
  _exit(1);
}
void* operator new[](std::size_t size) {
  return ::operator new(size);
}
void operator delete(void* ptr) noexcept {
  std::free(ptr);
}
void operator delete[](void* ptr) noexcept {
  std::free(ptr);
}
void operator delete(void* ptr, std::size_t) noexcept {
  std::free(ptr);
}
void operator delete[](void* ptr, std::size_t) noexcept {
  std::free(ptr);
}
namespace std {
[[noreturn]] void __throw_bad_function_call() {
  _exit(2);
}
[[noreturn]] void __throw_bad_alloc() {
  _exit(1);
}
[[noreturn]] void __throw_bad_array_new_length() {
  _exit(1);
}
}  // namespace std
#include <memory>
namespace std {
// Pinned GCC 14 uses inline identity with -fno-rtti; no cross-DSO fallback.
bool _Sp_make_shared_tag::_S_eq(const type_info&) noexcept {
  return false;
}
}  // namespace std
