// CRC-32/IEEE (Ethernet)
// standard parameters (reflected, init/xorout = 0xFFFFFFFF)

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <array>

static std::array<uint32_t, 256> make_crc32_table() {
  std::array<uint32_t, 256> table{};
  constexpr uint32_t poly = 0xEDB88320u; // reflected 0x04C11DB7

  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t c = i;
    for (int k = 0; k < 8; ++k) {
      c = (c & 1u) ? (poly ^ (c >> 1)) : (c >> 1);
    }
    table[i] = c;
  }
  return table;
}

int main() {
  // Read stdin as binary bytes.
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  const auto table = make_crc32_table();

  uint32_t crc = 0xFFFFFFFFu; // init
  char ch;
  while (std::cin.get(ch)) {
    uint8_t b = static_cast<uint8_t>(ch);
    crc = (crc >> 8) ^ table[(crc ^ b) & 0xFFu];
  }
  crc ^= 0xFFFFFFFFu; // xorout

  std::cout << std::hex << std::uppercase
    << std::setw(8) << std::setfill('0')
    << crc << "\n";
  return 0;
}
