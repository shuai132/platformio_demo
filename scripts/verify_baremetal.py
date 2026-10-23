"""Check the RAM-only C3 image and exclude accidental RTOS linkage."""
import hashlib
from pathlib import Path
import re
import struct
import sys

IRAM = (0x40380400, 0x40390000)
DRAM = (0x3FC90000, 0x3FCD0000)
FORBIDDEN = re.compile(r'FreeRTOS|freertos|^pthread_|^xTask|^vTask|^esp_timer_|^loopTask$|^app_main$|__gxx_personality|__cxa_throw|__cxa_allocate_exception')

def in_ram(address, size):
    return any(start <= address and address + size <= end for start, end in (IRAM, DRAM))

def verify_elf(path):
    data = Path(path).read_bytes()
    assert data[:7] == b'\x7fELF\x01\x01\x01', 'Expected little-endian ELF32'
    header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    assert header[2] == 243 and header[4] == IRAM[0], 'Wrong architecture or entry point'
    ranges = []
    for i in range(header[10]):
        kind, offset, address, physical, file_size, memory_size, flags, align = struct.unpack_from(
            '<IIIIIIII', data, header[5] + i * header[9])
        if kind != 1:
            continue
        assert in_ram(address, memory_size), 'Load segment outside application RAM'
        if IRAM[0] <= address < IRAM[1]:
            address -= 0x40380000 - 0x3fc80000
        ranges.append((address, address + memory_size))
    ranges.sort()
    assert all(a[1] <= b[0] for a, b in zip(ranges, ranges[1:])), 'Physical RAM overlap'
    sections = [struct.unpack_from('<IIIIIIIIII', data, header[6] + i * header[11])
                for i in range(header[12])]
    names = set()
    for section in sections:
        assert not section[2] & 0x400, 'Unexpected TLS section'
        if section[1] != 2:
            continue
        strings = sections[section[6]]
        table = data[strings[4]:strings[4] + strings[5]]
        for offset in range(section[4], section[4] + section[5], section[9]):
            name_offset, value, size, info, other, index = struct.unpack_from('<IIIBBH', data, offset)
            name = table[name_offset:].split(b'\0', 1)[0].decode()
            if not name:
                continue
            assert not (index == 0 and info >> 4 in (1, 2)), 'Unresolved symbol: ' + name
            assert not FORBIDDEN.search(name), 'Unexpected runtime symbol: ' + name
            names.add(name)
    assert {'_start', 'baremetal_start', 'main'} <= names, 'Missing startup symbols'
    print('Bare-metal ELF: RAM layout and single-thread runtime verified')

def verify_image(path):
    data = Path(path).read_bytes()
    assert data[0] == 0xE9 and struct.unpack_from('<H', data, 12)[0] == 5, 'Not an ESP32-C3 ROM image'
    assert struct.unpack_from('<I', data, 4)[0] == IRAM[0], 'Wrong entry point'
    offset, checksum = 24, 0xEF
    for _ in range(data[1]):
        address, size = struct.unpack_from('<II', data, offset)
        offset += 8
        assert in_ram(address, size), 'Image segment outside application RAM'
        for byte in data[offset:offset + size]:
            checksum ^= byte
        offset += size
    footer = (offset // 16 + 1) * 16 - 1
    assert data[footer] == checksum, 'Invalid image checksum'
    assert data[23] == 1, 'Missing image digest'
    assert data[footer + 1:] == hashlib.sha256(data[:footer + 1]).digest(), 'Invalid image digest'
    print('ROM image: entry, RAM segments, checksum and SHA-256 verified')

if __name__ == '__main__':
    for path in sys.argv[1:]:
        (verify_elf if path.endswith('.elf') else verify_image)(path)
