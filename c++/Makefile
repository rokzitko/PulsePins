NR = 35
TARGETHOST ?= de10nano
TIDBIT_DIR ?= ../tidbit

GIT_HASH := $(shell git rev-parse --short HEAD)

# For low-level troubleshooting, uncomment the following line for tracing out all memory-mapped I/O to FPGA.
#HW_DEBUG = -DDEBUG_FPGA_IO

ALT_DEVICE_FAMILY ?= soc_cv_av
SOCEDS_ROOT ?= $(SOCEDS_DEST_ROOT)
HWLIBS_ROOT ?= $(SOCEDS_ROOT)/ip/altera/hps/altera_hps/hwlib
CROSS_COMPILE ?= arm-linux-gnueabihf-
CPPFLAGS = -O3 -g -Wall -DTIDBITNR=${NR} \
  -D$(ALT_DEVICE_FAMILY) \
  -DGIT_HASH=$(GIT_HASH) \
  -I$(TIDBIT_DIR) \
  -I$(HWLIBS_ROOT)/include/$(ALT_DEVICE_FAMILY) -I$(HWLIBS_ROOT)/include/ -I.. -I/usr/include \
  -Wformat=0 -std=c++17 -Wno-psabi $(HW_DEBUG)
LDFLAGS =  -g -Wall -pthread
GCC_SUFFIX ?= -10
CXX = $(CROSS_COMPILE)g++$(GCC_SUFFIX)
CC = $(CROSS_COMPILE)gcc$(GCC_SUFFIX)
ARCH = arm

GCC_VERSION := $(shell $(CXX) -dumpversion | cut -f1 -d.)

ifeq ($(GCC_VERSION),8)
    FSLIB = -lstdc++fs
    LDFLAGS += $(FSLIB)
endif

# Targets that run on DE10 board
TARGETS = pptool ppserver

# Symlinks to pptool
SYMLINKS = pptest ppmstest ppdmatest ppfg ppreset pptrig ppdelay ppqout ppaux ppcounter ppts \
           ppgpsdo pphelloworld

build: pptool ppserver unit_tests

pptool: pptool.o
	$(CXX) $(LDFLAGS)   $^ -o $@ $(FSLIB)

ppserver: ppserver.o
	$(CXX) $(LDFLAGS)   $^ -o $@ $(FSLIB)

HEADERS = $(wildcard *.hh) $(wildcard *.h) $(wildcard $(TIDBIT_DIR)/*.hh) $(wildcard $(TIDBIT_DIR)/*.h)

unit_tests: unit_tests.cc $(HEADERS)
	g++ $(CPPFLAGS) $< -o $@

pptool.o : pptool.cc ../hps_0.h $(HEADERS)
	$(CXX) $(CPPFLAGS) -c $< -o $@

ppserver.o : ppserver.cc ../hps_0.h $(HEADERS)
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
