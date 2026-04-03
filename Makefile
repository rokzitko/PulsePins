# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

PREFIX=pulsepins
SOF=${PREFIX}.sof
RBF=${PREFIX}.rbf

TARGETHOST ?= de10nano

# Quartus command location. Override in `Makefile.local` if the toolchain lives elsewhere.
QDIR ?= ${HOME}/intelFPGA_lite/21.1/quartus/bin/

RM=rm -vf
QSYS=time qsys-generate
QSH=time ${QDIR}/quartus_sh

QSYSIN=base_hps.qsys
SOPC=base_hps.sopcinfo
QPF=$(abspath ${PREFIX}.qpf)
HPS=hps_0.h
IMGROOT=image/ext/home/root

SOURCE=$(wildcard *.sv) $(wildcard ../IP/*.v) $(wildcard ../IP/*.sv)
IPSOURCE=$(wildcard ip/*/*.v) $(wildcard ip/*/*.vh) $(wildcard ip/*/*.sv)

# Optional local overrides
-include Makefile.local

.PHONY: c++
# Full hardware + host-software build. This is the main project build entry point.
all: ${HPS} ${SOF} ${RBF} c++

c++:
	make -C c++

${SOPC}: ${QSYSIN} ${IPSOURCE} $(wildcard *_hw.tcl)
	${QSYS} --synthesis=VERILOG ${QSYSIN} 2>&1 | tee build-log-qsys

${SOF}: ${SOPC} ${SOURCE} ${PREFIX}.qsf
	${QSH} --flow compile ${QPF} 2>&1 | tee build-log-compile
	sha256sum ${SOF} >sha256.${SOF}

${HPS}: ${SOPC}
	sopc-create-header-files ${SOPC} --single ${HPS} --module hps_0

${RBF}: ${SOF}
	quartus_cpf -c ${PREFIX}.cof
	sha256sum ${RBF} >sha256.${RBF}

# Copy only the FPGA runtime image to a live target board.
copy: ${RBF}
	scp ${RBF} @${TARGETHOST}:${PREFIX}.rbf

# Copy the FPGA runtime image to the boot partition path on the live target board.
copy_boot: ${RBF}
	scp ${RBF} @${TARGETHOST}:fat/socfpga.rbf

# Stage only the FPGA runtime image into the image tree.
copy_img: ${RBF}
	scp ${RBF} ${IMGROOT}/${PREFIX}.rbf

# Do not check for dependences, force copying the current file
forcecopy:
	scp ${RBF} @${TARGETHOST}:${PREFIX}.rbf

# Deploy the usual live-board runtime bundle: FPGA image, C++, Python, tests, I2C helpers,
# and shell completion support.
copy_all: copy
	cd c++ ; make copy ; make copy_sources
	cd python ; make copy_sources ; make copy_misc
	cd tests ; make copy
	cd I2C ; make copy
	cd contrib/completions ; make copy


# Stage the same runtime bundle into the image tree instead of pushing it to a board.
copy_all_img: copy_img
	cd c++ ; make copy_img ; make copy_sources_img
	cd python ; make copy_sources_img
	cd tests ; make copy_img
	cd I2C ; make copy_img
	cd contrib/completions ; make copy_img

copy_all_image: copy_all_img

# Lint only the top-level Verilog/SystemVerilog files in the repository root.
lint-verilator:
	verilator --lint-only -Wall *.sv

lint-verible:
	verible-verilog-lint --rules=-line-length,-parameter-name-style *.sv

lint: lint-verible

clean:
	${RM} ${SOF} ${RBF} ${HPS}
	${RM} ${PREFIX}*.rpt
	${RM} ${PREFIX}*.done
	${RM} ${PREFIX}*.smsg
	${RM} ${PREFIX}*.summary
	${RM} ${PREFIX}*.jdi
	${RM} ${PREFIX}*.pin
	${RM} ${PREFIX}*.qws
	${RM} ${PREFIX}*.sld
	${RM} hps_sdram_p0_summary.csv
	${RM} base_hps.sopcinfo
	${RM} c5_pin_model_dump.txt
	${RM} ${PREFIX}*_assignment_defaults.qdf
	${RM} qar_info.json
	${RM} -r incremental_db/
	${RM} -r db/
	${RM} -r hps_isw_handoff/
	${RM} -r .qsys_edit/
	${RM} -r base_hps/
	${RM} -r build-log-*
	${RM} -r sha256.pulsepins.rbf sha256.pulsepins.sof
	${RM} -r report_divclk_worst_100.rpt
	${MAKE} -C c++ clean
	${MAKE} -C python clean
	${MAKE} -C docs clean
	${MAKE} -C ip clean
	${MAKE} -C image clean
