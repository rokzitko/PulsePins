NR = 35
TARGETHOST ?= de10nano
TIDBIT_DIR ?= ../tidbit

GIT_HASH := $(shell git rev-parse --short HEAD)

# For low-level troubleshooting, uncomment the following line for tracing out all memory-mapped I/O to FPGA.
#HW_DEBUG = -DDEBUG_FPGA_IO

# Lua source directory
LUA_DIR := third_party/lua

ALT_DEVICE_FAMILY ?= soc_cv_av
SOCEDS_ROOT ?= $(SOCEDS_DEST_ROOT)
HWLIBS_ROOT ?= $(SOCEDS_ROOT)/ip/altera/hps/altera_hps/hwlib
CROSS_COMPILE ?= arm-linux-gnueabihf-
CFLAGS   = -O2 -Wall -Wextra -fPIC
CPPFLAGS = -O3 -g -Wall -DTIDBITNR=${NR} \
  $(SYSROOT) \
  -D$(ALT_DEVICE_FAMILY) \
  -DGIT_HASH=$(GIT_HASH) \
  -I$(TIDBIT_DIR) -I$(LUA_DIR) \
  -I$(HWLIBS_ROOT)/include/ -I$(HWLIBS_ROOT)/include/$(ALT_DEVICE_FAMILY) \
  -I./third_party/hwlib/include/ -I./third_party/hwlib/include/$(ALT_DEVICE_FAMILY) \
  -I./third_party/asio/include -I./third_party \
  -I.. \
  -Wformat=0 -std=c++17 -Wno-psabi $(HW_DEBUG)
LDFLAGS =  -g -Wall -pthread
GCC_SUFFIX ?= -10
CXX = $(CROSS_COMPILE)g++$(GCC_SUFFIX)
CC = $(CROSS_COMPILE)gcc$(GCC_SUFFIX)
ARCH = arm
AR := ar
RANLIB := ranlib

GCC_VERSION := $(shell $(CXX) -dumpversion | cut -f1 -d.)

ifeq ($(GCC_VERSION),8)
    FSLIB = -lstdc++fs
    LDFLAGS += $(FSLIB)
endif

build: pptool ppscpi unit_tests

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
	$(CC) $(CFLAGS) -c $< -o $@

# Targets that run on DE10 board
TARGETS = pptool ppscpi

# Symlinks to pptool
SYMLINKS = pptest ppmstest ppdmatest ppfg ppreset pptrig ppdelay ppqout ppaux ppcounter ppts \
           ppgpsdo pptemp ppfreq ppread ppvcd pphelloworld

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
	$(CXX) $(CPPFLAGS) -c $< -o $@

pptool_streaming.o : pptool_streaming.cc ../hps_0.h $(HEADERS)
	$(CXX) $(CPPFLAGS) -c $< -o $@

pptool_measurement.o : pptool_measurement.cc ../hps_0.h $(HEADERS)
	$(CXX) $(CPPFLAGS) -c $< -o $@

ppscpi.o : ppscpi.cc ../hps_0.h $(HEADERS)
	$(CXX) $(CPPFLAGS) -c $< -o $@

.PHONY: clean copy
clean:
	rm -f $(TARGETS) unit_tests *.a *.o *~

copy: $(TARGETS)
	scp $(TARGETS) @$(TARGETHOST):
	ssh $(TARGETHOST) 'for L in $(SYMLINKS); do ln -sf pptool $$L; done'

IMGROOT=../image/ext/home/root

copy_img: $(TARGETS)
	cp -v $(TARGETS) $(IMGROOT)
	cd $(IMGROOT) ; for L in $(SYMLINKS); do ln -sf pptool $$L; done

# Copy source files to the DE10 nano board for local compiling
CPPDIR=c++/
SOURCES=$(HEADERS) $(wildcard *.cc)
MISCFILES=Makefile
copy_sources:
	ssh $(TARGETHOST) mkdir -p $(CPPDIR)
	scp $(SOURCES) $(MISCFILES) @$(TARGETHOST):$(CPPDIR)
	scp ../hps_0.h @$(TARGETHOST):

copy_sources_img:
	mkdir -p $(IMGROOT)/$(CPPDIR)
	cp -v $(SOURCES) $(MISCFILES) $(IMGROOT)/$(CPPDIR)
	cp -v ../hps_0.h $(IMGROOT)
