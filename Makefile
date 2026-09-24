# ---------------------------------------------------------------------------
# STM32F446RE firmware build
#   make          -> build build/firmware.elf, .bin, .hex and print size
#   make clean    -> remove build artefacts
#   make size     -> flash/RAM usage report
#   make test     -> build and run host unit tests
#   make cppcheck -> static analysis
# ---------------------------------------------------------------------------

TARGET  := firmware
BUILD   := build

# --- version information compiled into the binary (configuration mgmt) ---
FW_VERSION ?= 0.1.0
GIT_HASH   := $(shell git rev-parse --short HEAD 2>/dev/null || echo nogit)

# --- toolchain ---
PREFIX  := arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size

# --- sources ---
SRCS    := src/startup.c src/main.c src/belt.c
OBJS    := $(SRCS:%.c=$(BUILD)/%.o)
LDSCRIPT:= linker/STM32F446RE.ld

# --- flags ---
CPU     := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS  := $(CPU) -Iinc -Os -g3 -std=c11 \
           -Wall -Wextra -Werror \
           -ffunction-sections -fdata-sections \
           -DFW_VERSION=\"$(FW_VERSION)\" -DGIT_HASH=\"$(GIT_HASH)\"
LDFLAGS := $(CPU) -T$(LDSCRIPT) -nostdlib -Wl,--gc-sections \
           -Wl,-Map=$(BUILD)/$(TARGET).map

all: $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex size

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

size: $(BUILD)/$(TARGET).elf
	@echo "--- version $(FW_VERSION) ($(GIT_HASH)) ---"
	$(SIZE) $<

# --- host unit tests (compiled with the NATIVE gcc, not the ARM one) ---
UNITY_DIR := tests/unity
test: $(UNITY_DIR)/src/unity.c
	@mkdir -p $(BUILD)
	gcc -Iinc -I$(UNITY_DIR)/src -Wall -Wextra \
	    tests/test_belt.c src/belt.c $(UNITY_DIR)/src/unity.c -o $(BUILD)/test_belt
	./$(BUILD)/test_belt

$(UNITY_DIR)/src/unity.c:
	git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git $(UNITY_DIR)

cppcheck:
	cppcheck --enable=warning,style,performance --error-exitcode=1 \
	         --suppress=missingIncludeSystem -Iinc src/

clean:
	rm -rf $(BUILD) $(UNITY_DIR)

.PHONY: all size test cppcheck clean
