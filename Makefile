# Jerry Clicker NDS - works with devkitARM + libnds (1.x and the newer calico-based 2.x)
ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set - run inside the devkitpro/devkitarm container or install devkitPro)
endif
DEVKITARM ?= $(DEVKITPRO)/devkitARM

TARGET  := JerryClicker
BUILD   := build
SRCDIR  := source
LIBNDS  := $(DEVKITPRO)/libnds
CALICO  := $(DEVKITPRO)/calico

CC := $(DEVKITARM)/bin/arm-none-eabi-gcc

ARCH   := -march=armv5te -mtune=arm946e-s -mthumb
CFLAGS := -g -Wall -O2 -ffunction-sections -fdata-sections $(ARCH) -DARM9 -D__NDS__ \
          -I$(LIBNDS)/include -I$(CALICO)/include
LIBDIRS := -L$(LIBNDS)/lib -L$(CALICO)/lib
LIBS    := -lfat -lnds9
LIBM    := -lm

# Find the ARM9 linker specs (name/location differs between libnds versions)
SPECFILE := $(firstword $(wildcard \
    $(CALICO)/share/ds9.specs \
    $(DEVKITARM)/arm-none-eabi/lib/ds9.specs \
    $(DEVKITARM)/arm-none-eabi/lib/ds_arm9.specs \
    $(LIBNDS)/share/ds_arm9.specs))
ifeq ($(SPECFILE),)
SPECS := -specs=ds_arm9.specs
else
SPECS := -specs=$(SPECFILE)
endif

# calico (libnds 2.x) needs its own runtime library
ifneq ($(wildcard $(CALICO)/lib/libcalico_ds9.a),)
LIBS += -lcalico_ds9
endif

# Default ARM7 core
ARM7 := $(firstword $(wildcard \
    $(CALICO)/bin/ds7_maine.elf \
    $(CALICO)/bin/ds7_maxmod.elf \
    $(CALICO)/bin/ds7.elf \
    $(LIBNDS)/default.elf \
    $(CALICO)/bin/ds7_*.elf))

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SOURCES))

.PHONY: all clean info
all: $(TARGET).nds

info:
	@echo "SPECS=$(SPECS)"; echo "ARM7=$(ARM7)"; echo "LIBS=$(LIBS)"

$(BUILD)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILD)
	$(CC) -MMD -MP $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJECTS)
	$(CC) $(ARCH) $(SPECS) -Wl,--gc-sections -Wl,-Map,$(TARGET).map $(OBJECTS) $(LIBDIRS) -Wl,--start-group $(LIBS) -Wl,--end-group $(LIBM) -o $@

$(TARGET).nds: $(TARGET).elf icon.bmp
	@test -n "$(ARM7)" || (echo "ERROR: could not find default ARM7 core"; exit 1)
	ndstool -c $@ -9 $< -7 $(ARM7) -b icon.bmp "Jerry Clicker;Click for Jerry Candies;Jerry"

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).map

-include $(OBJECTS:.o=.d)
