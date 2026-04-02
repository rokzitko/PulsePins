NR = 35
ALT_DEVICE_FAMILY ?= soc_cv_av
PY_OPT_LEVEL ?= -O2
PY_DEBUG ?= 0

PY_DEBUG_FLAGS =
ifeq ($(PY_DEBUG),1)
PY_DEBUG_FLAGS += -g
endif

CXXFLAGS = $(PY_OPT_LEVEL) $(PY_DEBUG_FLAGS) -Wall -DTIDBITNR=${NR} \
  -D$(ALT_DEVICE_FAMILY) \
  -Wformat=0 -std=c++17

CXX = g++$(GCC_SUFFIX)
CC = gcc$(GCC_SUFFIX)

.PHONY: build test clean copy_sources copy_tests

build:
	mkdir -p build
	cmake -S . -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_CXX_FLAGS="$(CXXFLAGS)" -B build
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
