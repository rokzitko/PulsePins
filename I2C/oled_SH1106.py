#!/usr/bin/env python3
from smbus2 import SMBus
import time

BUS = 1
ADDR = 0x3C
WIDTH = 128
HEIGHT = 64
PAGES = HEIGHT // 8

class SH1106:
    def __init__(self, bus_num=1, addr=0x3C, width=128, height=64, col_offset=2):
        self.bus = SMBus(bus_num)
        self.addr = addr
        self.width = width
        self.height = height
        self.pages = height // 8
        self.col_offset = col_offset
        self.buffer = bytearray(self.width * self.pages)

    def close(self):
        self.bus.close()

    def cmd(self, c):
        self.bus.write_byte_data(self.addr, 0x00, c)

    def cmd_seq(self, seq):
        for c in seq:
            self.cmd(c)

    def data(self, data_bytes):
        # SMBus block writes: keep chunks modest
        for i in range(0, len(data_bytes), 16):
            self.bus.write_i2c_block_data(self.addr, 0x40, list(data_bytes[i:i+16]))

    def init(self):
        seq = [
            0xAE,        # display off
            0xD5, 0x80,  # clock divide
            0xA8, 0x3F,  # multiplex ratio 1/64
            0xD3, 0x00,  # display offset
            0x40,        # start line = 0
            0xAD, 0x8B,  # DC-DC on (common SH1106 setting)
            0xA1,        # segment remap
            0xC8,        # COM scan direction remap
            0xDA, 0x12,  # COM pins config
            0x81, 0x7F,  # contrast
            0xD9, 0x22,  # pre-charge
            0xDB, 0x35,  # VCOM deselect
            0xA4,        # display follows RAM
            0xA6,        # normal display
            0xAF         # display on
        ]
        self.cmd_seq(seq)

    def clear(self):
        for i in range(len(self.buffer)):
            self.buffer[i] = 0x00

    def fill(self, value=0xFF):
        for i in range(len(self.buffer)):
            self.buffer[i] = value

    def set_pixel(self, x, y, color=1):
        if not (0 <= x < self.width and 0 <= y < self.height):
            return
        page = y // 8
        bit = y % 8
        idx = page * self.width + x
        if color:
            self.buffer[idx] |= (1 << bit)
        else:
            self.buffer[idx] &= ~(1 << bit)

    def rect(self, x, y, w, h, color=1):
        for xx in range(x, x + w):
            self.set_pixel(xx, y, color)
            self.set_pixel(xx, y + h - 1, color)
        for yy in range(y, y + h):
            self.set_pixel(x, yy, color)
            self.set_pixel(x + w - 1, yy, color)

    def show(self):
        # SH1106: write one page at a time
        for page in range(self.pages):
            self.cmd(0xB0 + page)  # page address

            col = self.col_offset
            self.cmd(0x00 | (col & 0x0F))          # lower column nibble
            self.cmd(0x10 | ((col >> 4) & 0x0F))   # upper column nibble

            start = page * self.width
            end = start + self.width
            self.data(self.buffer[start:end])

def main():
    oled = SH1106(bus_num=BUS, addr=ADDR, col_offset=2)
    oled.init()

    # 1) Force all pixels on, independent of RAM
    oled.cmd(0xA5)
    time.sleep(2)

    # Back to RAM-driven mode
    oled.cmd(0xA4)
    time.sleep(0.2)

    # 2) Write a full white framebuffer
    oled.fill(0xFF)
    oled.show()
    time.sleep(2)

    # 3) Checkerboard-ish test
    for page in range(PAGES):
        for x in range(WIDTH):
            oled.buffer[page * WIDTH + x] = 0xAA if (x % 2 == 0) else 0x55
    oled.show()
    time.sleep(2)

    # 4) Simple border
    oled.clear()
    oled.rect(0, 0, 128, 64, 1)
    oled.show()
    time.sleep(5)

    oled.clear()
    oled.show()
    oled.close()

if __name__ == "__main__":
    main()

