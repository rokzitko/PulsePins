#pragma once

#include <array>
#include <cstdint>
#include <cerrno>
#include <system_error>
#include <string>
#include <sstream>
#include <exception>
#include <stdexcept>

#if defined(__linux__) && __has_include(<linux/i2c-dev.h>) && __has_include(<linux/i2c.h>)
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

// Low-level Linux i2c-dev wrapper.
class I2CDevice {
 public:
#if defined(__linux__) && __has_include(<linux/i2c-dev.h>) && __has_include(<linux/i2c.h>)
   explicit I2CDevice(int bus) {
      std::ostringstream path;
      path << "/dev/i2c-" << bus;
     path_ = path.str();
     fd_ = ::open(path_.c_str(), O_RDWR);
     if (fd_ < 0)
       throw std::system_error(errno, std::generic_category(), "open(" + path_ + ")");
   }

   ~I2CDevice() {
     if (fd_ >= 0) ::close(fd_);
   }

   I2CDevice(const I2CDevice&) = delete;
   I2CDevice& operator=(const I2CDevice&) = delete;

   // Combined write(reg) + read(n) transaction with repeated start.
   std::array<uint8_t, 2> read_reg2(uint8_t addr7, uint8_t reg) {
     uint8_t wbuf[1] = { reg };
     uint8_t rbuf[2] = { 0, 0 };
     i2c_msg msgs[2];
     msgs[0].addr  = addr7;
     msgs[0].flags = 0;        // write
     msgs[0].len   = 1;
     msgs[0].buf   = wbuf;
     msgs[1].addr  = addr7;
     msgs[1].flags = I2C_M_RD; // read
     msgs[1].len   = 2;
     msgs[1].buf   = rbuf;
     i2c_rdwr_ioctl_data data;
     data.msgs  = msgs;
     data.nmsgs = 2;
     if (ioctl(fd_, I2C_RDWR, &data) < 0)
       throw std::system_error(errno, std::generic_category(), "ioctl(I2C_RDWR)");
     return { rbuf[0], rbuf[1] };
   }

   // Write exactly 2 bytes (no register prefix).
   // Typical use: devices expecting a 2-byte payload (e.g., big-endian value) after the address phase.
   void write2(uint8_t addr7, std::array<uint8_t, 2> bytes) {
     uint8_t wbuf[2] = { bytes[0], bytes[1] };

     i2c_msg msg;
     msg.addr  = addr7;
     msg.flags = 0;           // write
     msg.len   = 2;
     msg.buf   = wbuf;

     i2c_rdwr_ioctl_data data;
     data.msgs  = &msg;
     data.nmsgs = 1;

     if (ioctl(fd_, I2C_RDWR, &data) < 0)
       throw std::system_error(errno, std::generic_category(), "ioctl(I2C_RDWR) write2");
   }

   // Write register + 2 bytes in one transaction (single write message).
   // Typical use: device expects first byte = register pointer, then 2 data bytes.
   void write_reg2(uint8_t addr7, uint8_t reg, std::array<uint8_t, 2> bytes) {
     uint8_t wbuf[3] = { reg, bytes[0], bytes[1] };

     i2c_msg msg;
     msg.addr  = addr7;
     msg.flags = 0;           // write
     msg.len   = 3;
     msg.buf   = wbuf;

     i2c_rdwr_ioctl_data data;
     data.msgs  = &msg;
     data.nmsgs = 1;

      if (ioctl(fd_, I2C_RDWR, &data) < 0)
        throw std::system_error(errno, std::generic_category(), "ioctl(I2C_RDWR) write_reg2");
    }
#else
   explicit I2CDevice(int) {}

    ~I2CDevice() = default;

    I2CDevice(const I2CDevice&) = delete;
    I2CDevice& operator=(const I2CDevice&) = delete;

    std::array<uint8_t, 2> read_reg2(uint8_t, uint8_t) {
      return { 0, 0 };
    }

    void write2(uint8_t, std::array<uint8_t, 2>) {}

    void write_reg2(uint8_t, uint8_t, std::array<uint8_t, 2>) {}
#endif

  private:
#if defined(__linux__) && __has_include(<linux/i2c-dev.h>) && __has_include(<linux/i2c.h>)
    int fd_ = -1;
    std::string path_;
#endif
};
