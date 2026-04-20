NR = 35
ALT_DEVICE_FAMILY ?= soc_cv_av

# Default to a lighter-weight board build. Host-side builds are mainly for import/syntax/API
# testing, while production Python modules are still expected to be built on the DE10-Nano.
PY_OPT_LEVEL ?= -O2
PY_DEBUG ?= 0

PY_DEBUG_FLAGS =
ifeq ($(PY_DEBUG),1)
PY_DEBUG_FLAGS += -g
endif

CXXFLAGS = $(PY_OPT_LEVEL) $(PY_DEBUG_FLAGS) -Wall -DTIDBITNR=${NR} \
  -D$(ALT_DEVICE_FAMILY) \
  -Wformat=0 -std=c++17

USE_PREGENERATED ?= 0

ifeq ($(USE_PREGENERATED),1)
  HPSFILE := artifacts/hps_0.h
	# Path relative to python/build
  CXXFLAGS += -I../../c++/artifacts/
else
  HPSFILE := ../hps_0.h
  CXXFLAGS += -I../..
endif

CXX = g++$(GCC_SUFFIX)
CC = gcc$(GCC_SUFFIX)

.PHONY: build test test-host clean copy_sources copy_misc copy_sources_img

# Configure and build the nanobind extension modules.
build:
	mkdir -p build
	cmake -S . -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_CXX_FLAGS="$(CXXFLAGS)" -B build
	cmake --build build

# Python-side test entry point.
test:
	PYTHONPATH=./build pytest test.py

# Host-side CI entry point: run only the binding/API tests that do not require board-backed MMIO.
test-host:
	PYTHONPATH=./build pytest -m "not hardware" test.py

clean:
	rm -rf build

# Copy source files to the DE10-Nano board for native module builds.
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

# Stage the same source bundle into the image tree.
copy_sources_img:
	mkdir -p $(IMGROOT)/$(PYDIR)
	cp -v $(SOURCES) $(MISCFILES) $(IMGROOT)/$(PYDIR)
