CC       = arm-none-eabi-gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -mcpu=cortex-m4 -mthumb -ffunction-sections -fdata-sections
INCLUDES = -Iinc
LDFLAGS  = -T link.ld -Wl,--gc-sections -nostartfiles
OBJCOPY  = arm-none-eabi-objcopy
SIZE     = arm-none-eabi-size

SRC_DIR  = src
INC_DIR  = inc
EXAM_DIR = examples
DEMO_DIR = demos
BUILD_DIR = build

SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS     = $(OBJS:.o=.d)

EXAM_SRCS = $(wildcard $(EXAM_DIR)/*.c)
EXAM_ELFS = $(patsubst $(EXAM_DIR)/%.c,$(BUILD_DIR)/%.elf,$(EXAM_SRCS))

DEMO_SRCS = $(wildcard $(DEMO_DIR)/*.c)
DEMO_ELFS = $(patsubst $(DEMO_DIR)/%.c,$(BUILD_DIR)/%.elf,$(DEMO_SRCS))

.PHONY: all clean examples demos

all: $(BUILD_DIR) $(OBJS) $(EXAM_ELFS) $(DEMO_ELFS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -c $< -o $@

$(BUILD_DIR)/%.elf: $(EXAM_DIR)/%.c $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/demo_%.elf: $(DEMO_DIR)/demo_%.c $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

examples: $(EXAM_ELFS)

demos: $(DEMO_ELFS)

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)
