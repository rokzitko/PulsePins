#!/usr/bin/env python3
from smbus2 import SMBus
import time

BUS = 1
ADDR = 0x3C

def cmd(bus, c):
    bus.write_byte_data(ADDR, 0x00, c)

def init_ssd1306(bus):
    seq = [
        0xAE,       # display off
        0xD5, 0x80,
        0xA8, 0x3F, # 64 mux
        0xD3, 0x00,
        0x40,
        0x8D, 0x14, # charge pump on
        0x20, 0x00, # horizontal addressing
        0xA1,
        0xC8,
        0xDA, 0x12,
        0x81, 0xCF,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,       # resume RAM display
        0xA6,       # normal
        0x2E,       # stop scroll
        0xAF        # display on
    ]
    for c in seq:
        cmd(bus, c)

with SMBus(BUS) as bus:
    init_ssd1306(bus)

    # Force every pixel on, independent of framebuffer RAM
    cmd(bus, 0xA5)
    time.sleep(3)

    # Back to normal display mode
    cmd(bus, 0xA4)

