#pragma once

#ifndef DEVICE
#define DEVICE DE10NANO
#define DE10NANO
#endif

// https://github.com/robseb/rstools/blob/main/FPGA-writeBridge/main.cpp

#if defined(DE10NANO) || defined(DE10STD)
#define HPSFPGA_OFST 0xC0000000 // HPS2FPGA Bridge
#define HPSFPGA_END  0xFBFFFFFF
#define H2F_RANGE   (HPSFPGA_END - HPSFPGA_OFST)

#define LWHPSFPGA_OFST 0xFF200000 // LWHPS2FPGA Bridge
#define LWHPSFPGA_END  0xFF3FFFFF
#define LWH2F_RANGE   (LWHPSFPGA_END - LWHPSFPGA_OFST)
#endif

// For A10: rstoolsA10 FPGA-writeBridge/main.cpp

#if defined(HAN)
#define ALT_FPGA_BRIDGE_H2F128_OFST        0xc0000000
#define HPSFPGA_OFST ALT_FPGA_BRIDGE_H2F128_OFST
#define H2F_RANGE 0x3c000000

#define ALT_FPGA_BRIDGE_LWH2F_OFST        0xff200000
#define LWHPSFPGA_OFST ALT_FPGA_BRIDGE_LWH2F_OFST
#define LWH2F_RANGE  0x200000
#endif
