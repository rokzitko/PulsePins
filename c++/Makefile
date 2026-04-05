NR = 35
TARGETHOST ?= de10nano
TIDBIT_DIR ?= ../tidbit

# Embed the current short git hash into host-side binaries for version/status reporting.
GIT_HASH := $(shell git rev-parse --short HEAD)

# For low-level troubleshooting, uncomment the following line for tracing out all memory-mapped I/O to FPGA.
#HW_DEBUG = -DDEBUG_FPGA_IO

# Lua source directory
LUA_DIR := third_party/lua

ALT_DEVICE_FAMILY ?= soc_cv_av
SOCEDS_ROOT ?= $(SOCEDS_DEST_ROOT)
HWLIBS_ROOT ?= $(SOCEDS_ROOT)/ip/altera/hps/altera_hps/hwlib

# Note: gcc-7 does not support the C++ filesystem library
#TOOLCHAIN ?= $(HOME)/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf
#CROSS_COMPILE ?= $(TOOLCHAIN)/bin/arm-linux-gnueabihf-

CROSS_COMPILE ?= arm-linux-gnueabihf-
GCC_SUFFIX ?= -7

# `SYSROOT` is only meaningful for the ARM cross-build. Leaving it empty allows host-side
# compile checks without emitting a broken `--sysroot=` flag.
ifneq ($(strip $(SYSROOT)),)
CROSSCFLAGS = --sysroot=$(SYSROOT)
CROSSCPPFLAGS = --sysroot=$(SYSROOT)
else
CROSSCFLAGS =
CROSSCPPFLAGS =
endif

CFLAGS   = -O2 -Wall -Wextra -fPIC
CPPFLAGS = -O3 -Wall -DTIDBITNR=${NR} \
  -D$(ALT_DEVICE_FAMILY) \
  -DGIT_HASH=$(GIT_HASH) \
  -I$(TIDBIT_DIR) -I$(LUA_DIR) \
  -I$(HWLIBS_ROOT)/include/ -I$(HWLIBS_ROOT)/include/$(ALT_DEVICE_FAMILY) \
  -I./third_party/hwlib/include/ -I./third_party/hwlib/include/$(ALT_DEVICE_FAMILY) \
  -I./third_party/asio/include -I./third_party \
  -I.. \
  -Wformat=0 -std=c++17 -Wno-psabi -Wno-cpp $(HW_DEBUG)
LDFLAGS = -Wall -pthread

CXX = $(CROSS_COMPILE)g++$(GCC_SUFFIX)
CC = $(CROSS_COMPILE)gcc$(GCC_SUFFIX)
AR = $(CROSS_COMPILE)ar
RANLIB = $(CROSS_COMPILE)ranlib

ARCH = arm

ifneq ($(MAKECMDGOALS),lint-whitespace)
GCC_VERSION := $(shell $(CXX) -dumpversion | cut -f1 -d.)

# Supported from gcc-8 onwards
ifeq ($(GCC_VERSION),8)
    FSLIB = -lstdc++fs
    LDFLAGS += $(FSLIB)
endif
endif

# Main host-tools build. `pptool` and `ppscpi` are cross-compiled for the board, while
# `unit_tests` is intentionally built with the host compiler for fast syntax/API checking.
build: pptool ppscpi unit_tests

# Whitespace policy check for project-owned C++ sources.
lint-whitespace:
	python3 ../scripts/check_cpp_whitespace.py .

# Lua core sources
LUA_CORE_SRC := \
  $(LUA_DIR)/lapi.c $(LUA_DIR)/lcode.c $(LUA_DIR)/lctype.c $(LUA_DIR)/ldebug.c \
  $(LUA_DIR)/ldo.c $(LUA_DIR)/ldump.c $(LUA_DIR)/lfunc.c $(LUA_DIR)/lgc.c \
  $(LUA_DIR)/llex.c $(LUA_DIR)/lmem.c $(LUA_DIR)/lobject.c $(LUA_DIR)/lopcodes.c \
  $(LUA_DIR)/lparser.c $(LUA_DIR)/lstate.c $(LUA_DIR)/lstring.c $(LUA_DIR)/ltable.c \
  $(LUA_DIR)/ltm.c $(LUA_DIR)/lundump.c $(LUA_DIR)/lvm.c $(LUA_DIR)/lzio.c \
  $(LUA_DIR)/lauxlib.c $(LUA_DIR)/lbaselib.c $(LUA_DIR)/lcorolib.c $(LUA_DIR)/ldblib.c \
  $(LUA_DIR)/liolib.c $(LUA_DIR)/lmathlib.c $(LUA_DIR)/loslib.c $(LUA_DIR)/lstrlib.c \
  $(LUA_DIR)/ltablib.c $(LUA_DIR)/lutf8lib.c $(LUA_DIR)/loadlib.c $(LUA_DIR)/linit.c

LUA_CORE_OBJ := $(LUA_CORE_SRC:.c=.o)
LUA_LIB      := $(LUA_DIR)/liblua.a

# Build static Lua library
$(LUA_LIB): $(LUA_CORE_OBJ)
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(LUA_DIR)/%.o: $(LUA_DIR)/%.c
	$(CC) $(CROSSCFLAGS) $(CFLAGS) -c $< -o $@

# Targets that run on DE10 board
TARGETS = pptool ppscpi

# Multi-call command entry points created as symlinks to `pptool` on the target.
SYMLINKS = pptest ppmstest ppdmatest ppfg ppreset pptrig ppdelay ppqout ppaux ppcounter ppts \
           ppgpsdo pptemp ppfreq ppread ppplay ppvcd pphelloworld

PPTOOL_OBJS = pptool.o pptool_streaming.o pptool_measurement.o

pptool: $(PPTOOL_OBJS) $(LUA_LIB)
	$(CXX) $(LDFLAGS)   $^ -o $@ $(FSLIB) $(LUA_LIB)

ASIO_DIR := third_party/asio

$(ASIO_DIR):
	git submodule update --init --recursive

ppscpi: ppscpi.o $(ASIO_DIR)
	$(CXX) $(LDFLAGS)   $< -o $@ $(FSLIB)

HEADERS = $(wildcard *.hh) $(wildcard *.h) $(wildcard $(TIDBIT_DIR)/*.hh) $(wildcard $(TIDBIT_DIR)/*.h)

unit_tests: unit_tests.cc $(HEADERS)
	g++ $(CPPFLAGS) $< -o $@

pptool.o : pptool.cc ../hps_0.h $(HEADERS)
	$(CXX) $(CROSSCPPFLAGS) $(CPPFLAGS) -c $< -o $@

pptool_streaming.o : pptool_streaming.cc ../hps_0.h $(HEADERS)
	$(CXX) $(CROSSCPPFLAGS)  $(CPPFLAGS) -c $< -o $@

pptool_measurement.o : pptool_measurement.cc ../hps_0.h $(HEADERS)
	$(CXX) $(CROSSCPPFLAGS)  $(CPPFLAGS) -c $< -o $@

ppscpi.o : ppscpi.cc ../hps_0.h $(HEADERS)
	$(CXX) $(CROSSCPPFLAGS)  $(CPPFLAGS) -c $< -o $@

.PHONY: clean copy lint-whitespace
clean:
	rm -f $(TARGETS) unit_tests *.a *.o *~

# Copy the built executables to a live board and recreate the usual `pptool` symlink family.
copy: $(TARGETS)
	scp $(TARGETS) @$(TARGETHOST):
	ssh $(TARGETHOST) 'for L in $(SYMLINKS); do ln -sf pptool $$L; done'

IMGROOT=../image/ext/home/root

# Stage the same executables into the image tree.
copy_img: $(TARGETS)
	cp -v $(TARGETS) $(IMGROOT)
	cd $(IMGROOT) ; for L in $(SYMLINKS); do ln -sf pptool $$L; done

# Copy source files to the DE10-Nano for local compilation. This is useful because the board can
# build Python bindings natively and can also rebuild the C++ tools from source when needed.
CPPDIR=c++/
SOURCES=$(HEADERS) $(wildcard *.cc)
MISCFILES=Makefile
copy_sources:
	ssh $(TARGETHOST) mkdir -p $(CPPDIR)
	scp $(SOURCES) $(MISCFILES) @$(TARGETHOST):$(CPPDIR)
	scp ../hps_0.h @$(TARGETHOST):
	cd third_party/lua ; make clean
	ssh $(TARGETHOST) "mkdir -p $(CPPDIR)/third_party"
	scp -r third_party/lua-5.5.0 @$(TARGETHOST):$(CPPDIR)/third_party
	ssh $(TARGETHOST) "cd c++/third_party ; ln -fs lua-5.5.0 lua"
	scp -r third_party/asio @$(TARGETHOST):$(CPPDIR)/third_party
	scp -r include/ @$(TARGETHOST):$(CPPDIR)
	scp -r unit_tests_input/ @$(TARGETHOST):$(CPPDIR)

# Stage the same source bundle into the image tree for image-based rebuild workflows.
copy_sources_img:
	mkdir -p $(IMGROOT)/$(CPPDIR)
	cp -v $(SOURCES) $(MISCFILES) $(IMGROOT)/$(CPPDIR)
	cp -v ../hps_0.h $(IMGROOT)
	cd third_party/lua ; make clean
	mkdir -p $(CPPDIR)/third_party
	cp -r third_party/lua-5.5.0 $(IMGROOT)/$(CPPDIR)/third_party
	ln -fs $(CPPDIR)/third_party/lua-5.5.0 $(CPPDIR)/third_party/lua
	cp -r third_party/asio $(IMGROOT)/$(CPPDIR)/third_party
	cp -r include/ $(IMGROOT)/$(CPPDIR)
	cp -r unit_tests_input/ $(IMGROOT)/$(CPPDIR)
