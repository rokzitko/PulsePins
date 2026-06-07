# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

PREFIX=pulsepins
SOF=${PREFIX}.sof
RBF=${PREFIX}.rbf

TARGETHOST ?= de10nano
SCP_TARGET ?= $(TARGETHOST)

# Quartus bin directory. Override in `Makefile.local` if the toolchain lives elsewhere.
QDIR ?= ${HOME}/intelFPGA_lite/21.1/quartus/bin
QUARTUS_ROOT ?= $(abspath ${QDIR}/..)
QSYS_DIR ?= ${QUARTUS_ROOT}/sopc_builder/bin
CHECK_QUARTUS_TIMING ?= 1

RM=rm -vf
QSYS ?= time ${QSYS_DIR}/qsys-generate
QSH ?= time ${QDIR}/quartus_sh
SOPC_CREATE_HEADER_FILES ?= ${QSYS_DIR}/sopc-create-header-files
QCPF ?= ${QDIR}/quartus_cpf

QSYSIN=base_hps.qsys
SOPC=base_hps.sopcinfo
QPF=$(abspath ${PREFIX}.qpf)
HPS=hps_0.h
IMAGE_ROOT ?= image
IMGROOT ?= ${IMAGE_ROOT}/ext/home/root
IMAGE_ROOT_ABS = $(abspath ${IMAGE_ROOT})
IMGROOT_ABS = $(abspath ${IMGROOT})

SOURCE=$(wildcard *.sv) $(wildcard ../IP/*.v) $(wildcard ../IP/*.sv)
IPSOURCE=$(wildcard ip/*/*.v) $(wildcard ip/*/*.vh) $(wildcard ip/*/*.sv)

# Optional local overrides
-include Makefile.local

.PHONY: c++ board-smoke dev-check timing-check timing-sdc-check
# Full hardware + host-software build. This is the main project build entry point.
all: ${HPS} ${SOF} ${RBF} c++

c++:
	$(MAKE) -C c++

# Consolidated host-side contributor sanity pass. This intentionally stays in the
# host-safe lane and does not touch the FPGA toolchain or the live board.
dev-check:
	$(MAKE) CROSS_COMPILE= GCC_SUFFIX= USE_PREGENERATED=1 -C c++ host-main-compile unit_tests test
	$(MAKE) -C docs site
	$(MAKE) CROSS_COMPILE= GCC_SUFFIX= USE_PREGENERATED=1 -C python build
	$(MAKE) CROSS_COMPILE= GCC_SUFFIX= USE_PREGENERATED=1 -C python test-host
	python3 -m py_compile python/test.py python/test_cli.py python/test_scpi_client.py python/test_timeline.py python/pptool.py python/pulsepins/*.py python/examples/*.py tests/test2.py

timing-check:
	python3 scripts/check_quartus_timing.py --root .

timing-sdc-check:
	python3 scripts/check_quartus_timing.py --root . --sdc-only

${SOPC}: ${QSYSIN} ${IPSOURCE} $(wildcard *_hw.tcl)
	${QSYS} --synthesis=VERILOG ${QSYSIN} 2>&1 | tee build-log-qsys

${SOF}: ${SOPC} ${SOURCE} ${PREFIX}.qsf ${PREFIX}.sdc scripts/check_quartus_timing.py
	${QSH} --flow compile ${QPF} 2>&1 | tee build-log-compile
ifeq ($(CHECK_QUARTUS_TIMING),1)
	python3 scripts/check_quartus_timing.py --root .
endif
	sha256sum ${SOF} >sha256.${SOF}

${HPS}: ${SOPC}
	${SOPC_CREATE_HEADER_FILES} ${SOPC} --single ${HPS} --module hps_0

${RBF}: ${SOF}
	${QCPF} -c ${PREFIX}.cof
	sha256sum ${RBF} >sha256.${RBF}

# Copy only the FPGA runtime image to a live target board.
copy: ${RBF}
	scp ${RBF} ${SCP_TARGET}:${PREFIX}.rbf

# Copy the FPGA runtime image to the boot partition path on the live target board.
copy_boot: ${RBF}
	scp ${RBF} ${SCP_TARGET}:fat/socfpga.rbf

# Stage only the FPGA runtime image into the image tree.
copy_img: ${RBF}
	mkdir -p ${IMGROOT}
	cp -v ${RBF} ${IMGROOT}/${PREFIX}.rbf

# Do not check for dependences, force copying the current file
forcecopy:
	scp ${RBF} ${SCP_TARGET}:${PREFIX}.rbf

# Deploy the usual live-board runtime bundle: FPGA image, C++, Python, tests, I2C helpers,
# and shell completion support.
copy_all: copy
	$(MAKE) -C c++ TARGETHOST=$(TARGETHOST) SCP_TARGET=$(SCP_TARGET) copy
	$(MAKE) -C c++ TARGETHOST=$(TARGETHOST) SCP_TARGET=$(SCP_TARGET) copy_sources
	$(MAKE) -C python TARGETHOST=$(TARGETHOST) SCP_TARGET=$(SCP_TARGET) copy_sources
	$(MAKE) -C python TARGETHOST=$(TARGETHOST) SCP_TARGET=$(SCP_TARGET) copy_misc
	$(MAKE) -C tests TARGETHOST=$(TARGETHOST) SCP_TARGET=$(SCP_TARGET) copy
	$(MAKE) -C I2C TARGETHOST=$(TARGETHOST) SCP_TARGET=$(SCP_TARGET) copy
	$(MAKE) -C contrib/completions TARGETHOST=$(TARGETHOST) SCP_TARGET=$(SCP_TARGET) copy


# Stage the same runtime bundle into the image tree instead of pushing it to a board.
copy_all_img: copy_img
	$(MAKE) -C c++ IMGROOT=$(IMGROOT_ABS) copy_img
	$(MAKE) -C c++ IMGROOT=$(IMGROOT_ABS) copy_sources_img
	$(MAKE) -C python IMGROOT=$(IMGROOT_ABS) copy_sources_img
	$(MAKE) -C tests IMGROOT=$(IMGROOT_ABS) copy_img
	$(MAKE) -C I2C IMGROOT=$(IMGROOT_ABS) copy_img
	$(MAKE) -C contrib/completions IMGROOT=$(IMAGE_ROOT_ABS) copy_img

copy_all_image: copy_all_img

# Manual live-board smoke pass against the current build artifacts.
# Use `TARGETHOST=... make board-smoke` to point it at a different board.
board-smoke:
	./scripts/board_smoke.sh "${TARGETHOST}"

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
