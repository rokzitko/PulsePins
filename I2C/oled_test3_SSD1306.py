#!/usr/bin/env python3
from smbus2 import SMBus, i2c_msg
import time

BUS = 1
ADDR = 0x3C
WIDTH = 128
HEIGHT = 64
PAGES = HEIGHT // 8

class OLED:
    def __init__(self, busno=1, addr=0x3C):
        self.bus = SMBus(busno)
        self.addr = addr
        self.buf = bytearray(WIDTH * PAGES)

    def close(self):
        self.bus.close()

    def write_cmd(self, *cmds):
        msg = i2c_msg.write(self.addr, bytes([0x00, *cmds]))
        self.bus.i2c_rdwr(msg)

    def write_data(self, data):
        # raw I2C write: control byte 0x40 followed by data bytes
        chunk = 16
        for i in range(0, len(data), chunk):
            msg = i2c_msg.write(self.addr, bytes([0x40]) + bytes(data[i:i+chunk]))
            self.bus.i2c_rdwr(msg)

    def init(self):
        self.write_cmd(
            0xAE,       # display off
            0xD5, 0x80,
            0xA8, 0x3F,
            0xD3, 0x00,
            0x40,
            0x8D, 0x14, # charge pump on
            0x20, 0x00, # horizontal addressing mode
            0xA1,       # segment remap
            0xC8,       # COM scan direction remap
            0xDA, 0x12,
            0x81, 0xCF,
            0xD9, 0xF1,
            0xDB, 0x40,
            0xA4,       # display follows RAM
            0xA6,       # normal display
            0x2E,       # stop scroll
            0xAF        # display on
        )

    def show(self):
        self.write_cmd(
            0x21, 0x00, WIDTH - 1,   # column range
            0x22, 0x00, PAGES - 1    # page range
        )
        self.write_data(self.buf)

    def clear(self):
        for i in range(len(self.buf)):
            self.buf[i] = 0x00

    def fill(self, value):
        for i in range(len(self.buf)):
            self.buf[i] = value

    def set_pixel(self, x, y, on=True):
        if not (0 <= x < WIDTH and 0 <= y < HEIGHT):
            return
        page = y // 8
        bit = y % 8
        idx = page * WIDTH + x
        if on:
            self.buf[idx] |= (1 << bit)
        else:
            self.buf[idx] &= ~(1 << bit)

    def rect(self, x, y, w, h, on=True):
        for xx in range(x, x + w):
            self.set_pixel(xx, y, on)
            self.set_pixel(xx, y + h - 1, on)
        for yy in range(y, y + h):
            self.set_pixel(x, yy, on)
            self.set_pixel(x + w - 1, yy, on)


if __name__ == "__main__":
    oled = OLED(BUS, ADDR)
    oled.init()

    # full white
    oled.fill(0xFF)
    oled.show()
    time.sleep(2)

    # full black
    oled.clear()
    oled.show()
    time.sleep(1)

    # border
    oled.clear()
    oled.rect(0, 0, 128, 64, True)
    oled.show()
    time.sleep(5)

    oled.close()

