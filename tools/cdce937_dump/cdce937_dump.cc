#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

static int open_i2c(const char* dev) {
    int fd = ::open(dev, O_RDWR);
    if (fd < 0) {
        throw std::runtime_error(std::string("open(") + dev + "): " + std::strerror(errno));
    }
    return fd;
}

// Repeated-start register read: [write 1B reg] then [read N bytes]
static void i2c_read_rs(int fd, uint8_t addr7, uint8_t reg, uint8_t* out, size_t n) {
    uint8_t regbuf[1] = { reg };

    i2c_msg msgs[2]{};
    msgs[0].addr  = addr7;
    msgs[0].flags = 0;          // write
    msgs[0].len   = 1;
    msgs[0].buf   = regbuf;

    msgs[1].addr  = addr7;
    msgs[1].flags = I2C_M_RD;   // read
    msgs[1].len   = static_cast<__u16>(n);
    msgs[1].buf   = out;

    i2c_rdwr_ioctl_data x{};
    x.msgs  = msgs;
    x.nmsgs = 2;

    if (ioctl(fd, I2C_RDWR, &x) < 0) {
        std::ostringstream os;
        os << "ioctl(I2C_RDWR) addr7=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)addr7
           << " reg=0x" << std::setw(2) << (int)reg << std::dec
           << ": " << std::strerror(errno);
        throw std::runtime_error(os.str());
    }
}

static uint8_t read_u8(int fd, uint8_t addr7, uint8_t reg) {
    uint8_t v = 0;
    i2c_read_rs(fd, addr7, reg, &v, 1);
    return v;
}

static void dump_range(int fd, uint8_t addr7, uint8_t start, size_t n) {
    uint8_t buf[256];
    if (n > sizeof(buf)) throw std::runtime_error("dump_range too large");

    i2c_read_rs(fd, addr7, start, buf, n);

    for (size_t i = 0; i < n; i += 16) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)(start + i) << ": ";
        size_t line = (n - i < 16) ? (n - i) : 16;
        for (size_t j = 0; j < line; ++j) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i + j] << " ";
        }
        std::cout << std::dec << "\n";
    }
}

int main() {
    // 8-bit addresses: 0xDA (write) / 0xDB (read) -> 7-bit: 0x6D
    constexpr const char* I2C_DEV = "/dev/i2c-0";
    constexpr uint8_t CDCE937_ADDR7 = 0x6D;

    try {
        int fd = open_i2c(I2C_DEV);

        // Optional: set slave (not required if only using I2C_RDWR with addr in msgs,
        // but keeps behavior conventional if you extend code later).
        if (ioctl(fd, I2C_SLAVE, CDCE937_ADDR7) < 0) {
            throw std::runtime_error(std::string("ioctl(I2C_SLAVE): ") + std::strerror(errno));
        }

        std::cout << "CDCE937 @ " << I2C_DEV
                  << " 7-bit addr 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)CDCE937_ADDR7
                  << " (8-bit write/read: 0xDA/0xDB)\n" << std::dec;

        // Read a couple of bytes individually (useful sanity check)
        uint8_t r00 = read_u8(fd, CDCE937_ADDR7, 0x00);
        std::cout << "Reg 0x00 = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)r00 << std::dec << "\n";

        // Commonly relevant blocks (matches TI map organization for CDCE937 family)
        std::cout << "\n[00h..0Fh] Global / control region\n";
        dump_range(fd, CDCE937_ADDR7, 0x00, 0x10);

        std::cout << "\n[10h..15h] PLL1 block\n";
        dump_range(fd, CDCE937_ADDR7, 0x10, 0x06);

        std::cout << "\n[20h..25h] PLL2 block\n";
        dump_range(fd, CDCE937_ADDR7, 0x20, 0x06);

        std::cout << "\n[30h..35h] PLL3 block\n";
        dump_range(fd, CDCE937_ADDR7, 0x30, 0x06);

        // If you want a broader sweep (still safe: read-only):
        // std::cout << "\n[00h..3Fh] Full low-page dump\n";
        // dump_range(fd, CDCE937_ADDR7, 0x00, 0x40);

        ::close(fd);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
