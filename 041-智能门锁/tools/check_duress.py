# -*- coding: utf-8 -*-
"""在 RAM dump 里定位 board_security_config_t，验证胁迫密码/业主密码是否仍为出厂值。

用法: python check_duress.py <ram_dump.txt>
ram_dump.txt 由 STM32CubeProgrammer 的 `-r32 0x20000000 20480` 生成，
每行形如  `0x20000000 : 7AC1248F E8569D03 ...`（每字 8 个十六进制字符，小端）。
"""
import hmac
import hashlib
import re
import sys

# 与 smart_lock_board_config.h 中 PROVISION_DEFAULT_DEVICE_SECRET 完全一致
SECRET = bytes([
    0x8F, 0x24, 0xC1, 0x7A, 0x03, 0x9D, 0x56, 0xE8,
    0x11, 0xB7, 0x4C, 0xD2, 0x6A, 0x35, 0xF0, 0x9C,
    0x5E, 0x82, 0xAB, 0x17, 0x40, 0xC9, 0x7B, 0x2D,
    0x63, 0x08, 0xE5, 0x94, 0x3A, 0x7F, 0xD6, 0x51])

word_re = re.compile(r'0x([0-9A-Fa-f]{8})\s*:\s*([0-9A-Fa-f]{8}(?:\s+[0-9A-Fa-f]{8})*)')


def load_dump(path):
    mem = bytearray()
    base = None
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            m = word_re.search(line)
            if not m:
                continue
            addr = int(m.group(1), 16)
            if base is None:
                base = addr
            for w in m.group(2).split():
                mem += int(w, 16).to_bytes(4, 'little')   # Cortex-M 小端
    return base, bytes(mem)


def find_all(mem, pattern):
    out = []
    i = mem.find(pattern)
    while i != -1:
        out.append(i)
        i = mem.find(pattern, i + 1)
    return out


def tag(pin):
    return hmac.new(SECRET, pin, hashlib.sha1).digest()


def main():
    base, mem = load_dump(sys.argv[1])
    print('dump base=0x%08X size=%d' % (base, len(mem)))

    hits = find_all(mem, SECRET)
    print('device_secret hits:', ['0x%08X' % (base + h) for h in hits])

    # TOTP 种子是 ASCII "12345678901234567890"，在 RAM 里最好认，
    # 找到它就等于找到了 security_config（totp_secret 在结构体偏移 32 处）。
    totp = b'12345678901234567890'
    tlocs = find_all(mem, totp)
    print('totp_secret hits:', ['0x%08X' % (base + h) for h in tlocs])
    for h in tlocs:
        s = base + h - 32
        print('    security_config? base=0x%08X' % s)
        print('    device_secret[0:32] =', mem[h - 32:h].hex())
        print('    owner_pin  tag     =', mem[h + 33:h + 53].hex(),
              'len=%d valid=%d' % (mem[h + 53], mem[h + 54]))
        print('    duress_pin tag     =', mem[h + 55:h + 75].hex(),
              'len=%d valid=%d' % (mem[h + 75], mem[h + 76]))

    for name, pin in (('owner 123456', b'123456'),
                      ('duress 654321', b'654321')):
        t = tag(pin)
        locs = find_all(mem, t)
        print('%-14s tag=%s hits=%s' % (name, t.hex(), ['0x%08X' % (base + h) for h in locs]))
        for h in locs:
            print('    tag@0x%08X  pin_length=%d valid=%d'
                  % (base + h, mem[h + 20], mem[h + 21]))


if __name__ == '__main__':
    main()
