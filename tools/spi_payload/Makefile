CXX = g++-13
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra

.PHONY: all clean

all: spi spi_ad9833 spi_ad9833_pga

spi: spi.cc spi_rle_encoder.hh
	$(CXX) $(CXXFLAGS) $< -o $@

spi_ad9833: spi_ad9833.cc ad9833.hh spi_rle_encoder.hh
	$(CXX) $(CXXFLAGS) $< -o $@

spi_ad9833_pga: spi_ad9833_pga.cc ad9833.hh mcp41010.hh spi_rle_encoder.hh
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f spi spi_ad9833 spi_ad9833_pga
