#!/usr/bin/env python3

from smbus2 import SMBus
import time

I2C_BUS = 1
OLED_ADDR = 0x3C

WIDTH = 128
HEIGHT = 64
PAGES = HEIGHT // 8


class SSD1306:
    def __init__(self, bus_num=1, addr=0x3C, width=128, height=64):
        self.bus = SMBus(bus_num)
        self.addr = addr
        self.width = width
        self.height = height
        self.pages = height // 8
        self.buffer = bytearray(self.width * self.pages)

    def close(self):
        self.bus.close()

    def cmd(self, c):
        self.bus.write_byte_data(self.addr, 0x00, c)

    def cmd_seq(self, seq):
        for c in seq:
            self.cmd(c)

    def data(self, data_bytes):
        # Split into chunks small enough for SMBus block write
        for i in range(0, len(data_bytes), 31):
            chunk = list(data_bytes[i:i+31])
            self.bus.write_i2c_block_data(self.addr, 0x40, chunk)

    def init(self):
        seq = [
            0xAE,       # display off
            0xD5, 0x80, # clock divide
            0xA8, 0x3F, # multiplex 1/64
            0xD3, 0x00, # display offset
            0x40,       # start line
            0x8D, 0x14, # charge pump on
            0x20, 0x00, # horizontal addressing mode
            0xA1,       # segment remap
            0xC8,       # COM scan direction remapped
            0xDA, 0x12, # COM pins config
            0x81, 0xCF, # contrast
            0xD9, 0xF1, # pre-charge
            0xDB, 0x40, # VCOMH deselect
            0xA4,       # resume RAM display
            0xA6,       # normal display
            0x2E,       # stop scroll
            0xAF        # display on
        ]
        self.cmd_seq(seq)
        self.clear()
        self.show()

    def set_full_window(self):
        self.cmd(0x21)                 # column address
        self.cmd(0)                    # start
        self.cmd(self.width - 1)       # end
        self.cmd(0x22)                 # page address
        self.cmd(0)                    # start
        self.cmd(self.pages - 1)       # end

    def show(self):
        self.set_full_window()
        self.data(self.buffer)

    def clear(self, color=0):
        fill = 0xFF if color else 0x00
        for i in range(len(self.buffer)):
            self.buffer[i] = fill

    def set_pixel(self, x, y, color=1):
        if not (0 <= x < self.width and 0 <= y < self.height):
            return
        page = y // 8
        bit = y % 8
        index = page * self.width + x
        if color:
            self.buffer[index] |= (1 << bit)
        else:
            self.buffer[index] &= ~(1 << bit)

    def hline(self, x0, x1, y, color=1):
        if x0 > x1:
            x0, x1 = x1, x0
        for x in range(x0, x1 + 1):
            self.set_pixel(x, y, color)

    def vline(self, x, y0, y1, color=1):
        if y0 > y1:
            y0, y1 = y1, y0
        for y in range(y0, y1 + 1):
            self.set_pixel(x, y, color)

    def rect(self, x, y, w, h, color=1):
        self.hline(x, x + w - 1, y, color)
        self.hline(x, x + w - 1, y + h - 1, color)
        self.vline(x, y, y + h - 1, color)
        self.vline(x + w - 1, y, y + h - 1, color)

    def fill_rect(self, x, y, w, h, color=1):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.set_pixel(xx, yy, color)

    def draw_char(self, x, y, ch, color=1):
        glyph = FONT5X7.get(ch, FONT5X7.get('?'))
        # glyph: 5 columns, LSB at top
        for col, bits in enumerate(glyph):
            for row in range(7):
                if bits & (1 << row):
                    self.set_pixel(x + col, y + row, color)
        # one blank column spacing is implicit

    def draw_text(self, x, y, text, color=1, spacing=1):
        cx = x
        for ch in text:
            if ch == '\n':
                cx = x
                y += 8
                continue
            self.draw_char(cx, y, ch, color)
            cx += 5 + spacing


# Minimal 5x7 font. Each entry is 5 columns, each byte uses bits 0..6 for rows top..bottom.
FONT5X7 = {
    ' ': [0x00, 0x00, 0x00, 0x00, 0x00],
    '!': [0x00, 0x00, 0x5F, 0x00, 0x00],
    '.': [0x00, 0x60, 0x60, 0x00, 0x00],
    ':': [0x00, 0x36, 0x36, 0x00, 0x00],
    '-': [0x08, 0x08, 0x08, 0x08, 0x08],
    '/': [0x20, 0x10, 0x08, 0x04, 0x02],
    '?': [0x02, 0x01, 0x51, 0x09, 0x06],

    '0': [0x3E, 0x51, 0x49, 0x45, 0x3E],
    '1': [0x00, 0x42, 0x7F, 0x40, 0x00],
    '2': [0x42, 0x61, 0x51, 0x49, 0x46],
    '3': [0x21, 0x41, 0x45, 0x4B, 0x31],
    '4': [0x18, 0x14, 0x12, 0x7F, 0x10],
    '5': [0x27, 0x45, 0x45, 0x45, 0x39],
    '6': [0x3C, 0x4A, 0x49, 0x49, 0x30],
    '7': [0x01, 0x71, 0x09, 0x05, 0x03],
    '8': [0x36, 0x49, 0x49, 0x49, 0x36],
    '9': [0x06, 0x49, 0x49, 0x29, 0x1E],

    'A': [0x7E, 0x11, 0x11, 0x11, 0x7E],
    'B': [0x7F, 0x49, 0x49, 0x49, 0x36],
    'C': [0x3E, 0x41, 0x41, 0x41, 0x22],
    'D': [0x7F, 0x41, 0x41, 0x22, 0x1C],
    'E': [0x7F, 0x49, 0x49, 0x49, 0x41],
    'F': [0x7F, 0x09, 0x09, 0x09, 0x01],
    'G': [0x3E, 0x41, 0x49, 0x49, 0x7A],
    'H': [0x7F, 0x08, 0x08, 0x08, 0x7F],
    'I': [0x00, 0x41, 0x7F, 0x41, 0x00],
    'J': [0x20, 0x40, 0x41, 0x3F, 0x01],
    'K': [0x7F, 0x08, 0x14, 0x22, 0x41],
    'L': [0x7F, 0x40, 0x40, 0x40, 0x40],
    'M': [0x7F, 0x02, 0x0C, 0x02, 0x7F],
    'N': [0x7F, 0x04, 0x08, 0x10, 0x7F],
    'O': [0x3E, 0x41, 0x41, 0x41, 0x3E],
    'P': [0x7F, 0x09, 0x09, 0x09, 0x06],
    'Q': [0x3E, 0x41, 0x51, 0x21, 0x5E],
    'R': [0x7F, 0x09, 0x19, 0x29, 0x46],
    'S': [0x46, 0x49, 0x49, 0x49, 0x31],
    'T': [0x01, 0x01, 0x7F, 0x01, 0x01],
    'U': [0x3F, 0x40, 0x40, 0x40, 0x3F],
    'V': [0x1F, 0x20, 0x40, 0x20, 0x1F],
    'W': [0x7F, 0x20, 0x18, 0x20, 0x7F],
    'X': [0x63, 0x14, 0x08, 0x14, 0x63],
    'Y': [0x03, 0x04, 0x78, 0x04, 0x03],
    'Z': [0x61, 0x51, 0x49, 0x45, 0x43],
}


def main():
    oled = SSD1306(bus_num=I2C_BUS, addr=OLED_ADDR, width=WIDTH, height=HEIGHT)
    oled.init()

    oled.clear()
    oled.rect(0, 0, 128, 64, 1)
    oled.draw_text(8, 8,  "HELLO OLED")
    oled.draw_text(8, 24, "I2C BUS: 1")
    oled.draw_text(8, 40, "ADDR: 0X3C")
    oled.show()
    time.sleep(5)

    oled.clear()
    oled.draw_text(8, 8,  "TEXT ONLY")
    oled.draw_text(8, 24, "NO PILLOW")
    oled.draw_text(8, 40, "SSD1306")
    oled.show()
    time.sleep(5)

    oled.clear()
    oled.show()
    oled.close()


if __name__ == "__main__":
    main()

