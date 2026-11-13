# devkitPro / devkitARM libnds project
DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
LIBNDS ?= $(DEVKITPRO)/libnds
PORTLIBS ?= $(DEVKITPRO)/portlibs

include $(DEVKITARM)/ds_rules

TARGET := jerryclicker
BUILD := build
SOURCES := source
DATA :=
INCLUDES := -Igenerated

PREFIX := arm-none-eabi-
CC := $(DEVKITARM)/bin/$(PREFIX)gcc
CXX := $(DEVKITARM)/bin/$(PREFIX)g++
AS := $(CC)
LD := $(CC)

ARCH := -mthumb -mthumb-interwork -march=armv5te -mtune=arm946e-s
CFLAGS := -g -Wall -O2 -ffunction-sections -fdata-sections $(ARCH)
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS := $(ARCH)
LDFLAGS := -specs=ds_arm9.specs -g $(ARCH) -Wl,--gc-sections

LIBDIRS := $(LIBNDS) $(PORTLIBS)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# Use the known archive directly so resolving -lnds9 doesn't depend on
# the linker's library search path.
LIBS := $(LIBNDS)/lib/libnds9.a

.PHONY: all clean
all: generated/sprites.h $(TARGET).nds

generated/sprites.h: tools/convert_sprites.py
	python3 tools/convert_sprites.py .. --out nds/generated

$(TARGET).elf: generated/sprites.h

clean:
	rm -rf $(BUILD)
	rm -f $(TARGET).elf $(TARGET).nds generated/sprites.h generated/sprite_manifest.json
