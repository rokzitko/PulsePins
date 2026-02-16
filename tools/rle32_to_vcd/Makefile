CXXFLAGS=-std=c++17

all: rle32_to_vcd

rle32_to_vcd: rle32_to_vcd.cc
	${CXX} ${CXXFLAGS} $< -o $@

clean:
	rm -rf rle32_to_vcd test_rle.txt test_out.vcd

