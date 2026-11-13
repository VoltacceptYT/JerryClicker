# Nintendo DS project configuration

DEVKITPRO ?= /opt/devkitpro
include $(DEVKITPRO)/base_tools

TARGET := jerryclicker_nds
BUILD_DIR := build
SRC_DIR := source
TOOLS_DIR := tools

CXX := arm-none-eabi-g++
CFLAGS := -MMD -MP -Wall -O2 -marm -march=armv5te -mtune=arm946e-s -fomit-frame-pointer -ffunction-sections -fdata-sections
INCLUDES := -I$(DEVKITPRO)/libnds/include -I$(DEVKITPRO)/libfat/include -I$(DEVKITPRO)/libgba/include
LDFLAGS := -specs=nds.specs -L$(DEVKITPRO)/libnds/lib -L$(DEVKITPRO)/libfat/lib -L$(DEVKITPRO)/libgba/lib
LDLIBS := -lnds9 -lfat -lmm7 -lmm9

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

all: $(BUILD_DIR)/$(TARGET).nds

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/$(TARGET).nds: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $(BUILD_DIR)/$(TARGET).elf $(OBJECTS) $(LDLIBS)
	arm-none-eabi-objcopy -O binary $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin
	ndstool -c $(BUILD_DIR)/$(TARGET).nds -9 $(BUILD_DIR)/$(TARGET).bin -b $(BUILD_DIR)/$(TARGET).bmp

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
