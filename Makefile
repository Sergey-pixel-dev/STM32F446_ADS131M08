# STM32F446RE + ADS131M08 — bare-metal build

TARGET      = firmware

PREFIX      = arm-none-eabi-
CC          = $(PREFIX)gcc
OBJCOPY     = $(PREFIX)objcopy
OBJDUMP     = $(PREFIX)objdump
SIZE        = $(PREFIX)size

# Directories
BUILD_DIR   = build
SRC_DIR     = src
INC_DIR     = inc
CMSIS_DIR   = cmsis
CMSIS_INC   = $(CMSIS_DIR)/include
LD_DIR      = ld

# Sources
C_SOURCES   = $(wildcard $(SRC_DIR)/*.c) $(CMSIS_DIR)/system_stm32f4xx.c
ASM_SOURCES = $(CMSIS_DIR)/startup_stm32f446xx.s

OBJECTS     = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS    += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

# Flags
CPU         = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS        = -DSTM32F446xx
INCS        = -I$(INC_DIR) -I$(CMSIS_INC)

CFLAGS      = $(CPU) $(DEFS) $(INCS) -O2 -Wall -Wextra -g -std=c11
LDFLAGS     = $(CPU) -T$(LD_DIR)/STM32F446XX_FLASH.ld \
              -specs=nano.specs -lc -lm -lnosys \
              -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref

# Rules
.PHONY: all clean flash size

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex size

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(CC) -c $(CPU) $< -o $@

$(BUILD_DIR):
	mkdir -p $@

size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

clean:
	rm -rf $(BUILD_DIR)

# Flash via ST-Link (requires st-flash)
flash: $(BUILD_DIR)/$(TARGET).bin
	st-flash write $< 0x08000000
