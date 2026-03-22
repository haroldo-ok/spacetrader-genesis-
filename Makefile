##############################################################################
# Space Trader 1.2.0 – Sega Genesis Port
# Makefile for SGDK 1.70
#
# Usage:
#   make            – build ROM (out/rom.bin)
#   make clean      – remove build artefacts
#
# Prerequisites:
#   SGDK installed at $GDK  (default: /opt/sgdk)
#   m68k-elf cross-toolchain in PATH (ships with SGDK)
#
# SGDK 1.70 uses makefile.gen which provides:
#   $(CC)  = m68k-elf-gcc
#   $(AS)  = m68k-elf-as
#   Linker script, ROM header, SRAM support
##############################################################################

# ── Path to SGDK root ───────────────────────────────────────────────────────
GDK ?= /opt/sgdk

# ── Output directory ────────────────────────────────────────────────────────
OUT  := out
OBJS := $(OUT)/obj

# ── Source files ────────────────────────────────────────────────────────────
#
# We compile every .c in src/ EXCEPT SetField.c (it duplicates Field.c)
# and the original Merchant entry point (now replaced by main.c + GenApp*).
#
SRC_EXCLUDE := SetField.c
# Exclude SGDK boot stubs (compiled separately by SGDK linker scripts)
SRC_C_ALL := $(filter-out src/boot/%, $(wildcard src/*.c))

SRC_C     := $(filter-out $(addprefix src/,$(SRC_EXCLUDE)),$(SRC_C_ALL))

# ── Extra include paths ──────────────────────────────────────────────────────
#
# We need:
#   src/          – our ported game source and headers
#   $(GDK)/inc    – SGDK public headers (genesis.h, vdp.h, etc.)
#   $(GDK)/res    – SGDK resource compiler output
#
INCS := -Isrc -I$(GDK)/inc

# ── Compiler flags ──────────────────────────────────────────────────────────
#
# -m68000           : target the 68000 (not 68020 etc.)
# -O2               : optimise for size/speed
# -fomit-frame-pointer : saves registers on the tight 64KB stack
# -fno-builtin      : avoid GCC built-ins that assume libgcc functions exist
# -Wall -Wextra     : enable most warnings (informational; not errors)
# -Wno-unused-*     : suppress noise from the Palm-era code
# -Wno-implicit-function-declaration : FrmInitForm etc. are macro-defined
# -DSGDK_BUILD      : lets our code #ifdef for Genesis-specific paths
# -DBPP8            : 8-bit colour depth (genesis uses 512-colour palette via SGDK)
#
CCFLAGS := -m68000 -O2 -fomit-frame-pointer -fno-builtin \
           -Wall -Wextra \
           -Wno-unused-parameter \
           -Wno-unused-variable \
           -Wno-unused-function \
           -Wno-implicit-function-declaration \
           -Wno-sign-compare \
           -Wno-missing-field-initializers \
           $(INCS) \
           -DSGDK_BUILD \
           -DBPP8

# ── Object file list ─────────────────────────────────────────────────────────
OBJS_C := $(patsubst src/%.c,$(OBJS)/%.o,$(SRC_C))

# ── Default target ───────────────────────────────────────────────────────────
.PHONY: all clean

all: $(OUT)/rom.bin

# ── Link via SGDK's makefile.gen ─────────────────────────────────────────────
# SGDK 1.70's makefile.gen expects:
#   $(SRC_C)   – list of C source files (we override here)
#   $(INCS)    – include directories   (we extend here)
# Then it handles everything else (linker script, sections, padding to 1 MB,
# generating the ROM header, etc.)
#
# If you have a standard SGDK install, uncomment the include line below and
# delete the manual rule below it:
#
# include $(GDK)/makefile.gen
#
# ── Manual build rules (fallback if makefile.gen path differs) ───────────────
CC  := m68k-elf-gcc
AS  := m68k-elf-as
LD  := m68k-elf-ld
NM  := m68k-elf-nm
OBJCOPY := m68k-elf-objcopy

GDK_LIB := $(GDK)/lib/libmd.a
GDK_LD  := $(GDK)/md.ld

LDFLAGS := -T $(GDK_LD) \
           -Map $(OUT)/rom.map \
           --gc-sections

$(OBJS)/%.o: src/%.c | $(OBJS)
	$(CC) $(CCFLAGS) -c $< -o $@

$(OUT)/rom.elf: $(OBJS_C) $(GDK_LIB) | $(OUT)
	$(LD) $(LDFLAGS) $(OBJS_C) $(GDK_LIB) -o $@

$(OUT)/rom.bin: $(OUT)/rom.elf
	$(OBJCOPY) -O binary $< $@
	@echo ""
	@echo "============================================"
	@echo " Build complete: $(OUT)/rom.bin"
	@ls -lh $(OUT)/rom.bin
	@echo "============================================"

$(OUT) $(OBJS):
	mkdir -p $@

clean:
	rm -rf $(OUT)

# ── Phony convenience: rebuild with SGDK's own makefile.gen ─────────────────
sgdk:
	$(MAKE) -f $(GDK)/makefile.gen \
	    GDK=$(GDK) \
	    SRC_C="$(SRC_C)" \
	    INCS="$(INCS)" \
	    CCFLAGS="$(CCFLAGS)"
