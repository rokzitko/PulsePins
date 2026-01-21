#pragma once

#include <array>
#include <cerrno>
#include <cstdint>
#include <ctime>

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

static constexpr uint8_t REG_AMBIENT_TEMP = 0x05;

struct Args {
  int bus = 1;
  int addr = 0x18;
  double delay = 1.0;
  int count = 0;              // 0 => forever
  bool fahrenheit = false;
  bool timestamp = false;
  bool csv = false;
  bool reopen = false;
  bool quiet_errors = false;
};

static std::string now_iso_utc_seconds() {
  using namespace std::chrono;
  auto now = system_clock::now();
  std::time_t t = system_clock::to_time_t(now);
  std::tm tm_utc{};
  gmtime_r(&t, &tm_utc);
  std::ostringstream oss;
  oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

// Low-level Linux i2c-dev wrapper.
class I2CDevice {
 public:
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

 private:
   int fd_ = -1;
    std::string path_;
};

// MCP9808 temperature reading and output formatting.
class MCP9808 {
public:
   MCP9808(int bus, int addr7, bool reopen_each_sample)
     : bus_(bus), addr_(addr7), reopen_(reopen_each_sample) {
       if (!reopen_) dev_.emplace(bus_);
     }

    double read_temp_c() {
      if (reopen_) {
        I2CDevice d(bus_);
        return read_from_device_(d);
      }
      return read_from_device_(*dev_);
    }

   // Formatting moved here, per request.
   static void print_csv_header(const Args& args, std::ostream& os) {
     if (!args.csv) return;
     if (args.fahrenheit) os << "timestamp,temp_c,temp_f\n";
     else                 os << "timestamp,temp_c\n";
     os.flush();
   }

   static std::string format_line(const Args& args, double t_c) {
     const double t_f = args.fahrenheit ? (t_c * 9.0 / 5.0 + 32.0) : 0.0;
     std::ostringstream oss;
     oss.setf(std::ios::fixed);
     oss << std::setprecision(4);
     if (args.csv) {
       oss << now_iso_utc_seconds() << "," << t_c;
       if (args.fahrenheit) oss << "," << t_f;
       return oss.str();
     }
     if (args.timestamp) oss << now_iso_utc_seconds() << "  ";
     oss << t_c << " °C";
     if (args.fahrenheit) oss << "  (" << t_f << " °F)";
     return oss.str();
   }

   static void emit_quiet_error_placeholder(const Args& args, std::ostream& out, std::ostream& err) {
     if (args.csv) {
       out << now_iso_utc_seconds() << ",NaN";
       if (args.fahrenheit) out << ",NaN";
       out << "\n";
       out.flush();
     } else {
       const std::string prefix = args.timestamp ? (now_iso_utc_seconds() + "  ") : "";
       err << prefix << "ERROR: I2C read failed\n";
       err.flush();
     }
   }

 private:
   static double decode_temp_c_(uint8_t msb, uint8_t lsb) {
     const uint16_t raw = (static_cast<uint16_t>(msb) << 8) | static_cast<uint16_t>(lsb);
     const bool sign = (raw & 0x1000u) != 0;     // bit 12
     const uint16_t temp_raw = (raw & 0x0FFFu);  // lower 13 bits effectively used in the Python version
     double temp_c = static_cast<double>(temp_raw) * 0.0625;
     if (sign) temp_c -= 256.0;
     return temp_c;
   }

   double read_from_device_(I2CDevice& d) {
     const auto bytes = d.read_reg2(static_cast<uint8_t>(addr_), REG_AMBIENT_TEMP);
     return decode_temp_c_(bytes[0], bytes[1]);
   }

   int bus_;
   int addr_;
   bool reopen_;
   std::optional<I2CDevice> dev_;
};
