NR = 35
ALT_DEVICE_FAMILY ?= soc_cv_av
CPPFLAGS = -O3 -g -Wall -DTIDBITNR=${NR} \
  -D$(ALT_DEVICE_FAMILY) \
  -I.. -I/usr/include \
  -Wformat=0 -std=c++17

.PHONY: build test clean copy_sources copy_tests

build:
	mkdir -p build
	cmake -S . -B build
	cmake --build build

# Unit tests
test:
	pytest test.py

clean:
	rm -rf build

# Copy source files to the DE10 nano board
PYDIR=python/
SOURCES=$(wildcard *.cc) pptool.py
MISCFILES=CMakeLists.txt Makefile test.py
copy_sources:
	ssh $(TARGETHOST) mkdir -p $(PYDIR)
	scp $(SOURCES) $(MISCFILES) @$(TARGETHOST):$(PYDIR)

copy_misc:
	ssh $(TARGETHOST) mkdir -p $(PYDIR)
	scp $(MISCFILES) @$(TARGETHOST):$(PYDIR)

IMGROOT=../image/ext/home/root

copy_sources_img:
	mkdir -p $(IMGROOT)/$(PYDIR)
	cp -v $(SOURCES) $(MISCFILES) $(IMGROOT)/$(PYDIR)
