CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -I include
OUTDIR   = bin

SRC      = src/cortex_m_core.c src/gpio_uart.c src/nvic_dma.c \
           src/memory_map.c src/mcu_peripheral.c

EXAMPLE1 = examples/example_gpio.c
EXAMPLE2 = examples/example_timer.c
EXAMPLE3 = examples/example_memory.c

.PHONY: all clean dirs

all: dirs $(OUTDIR)/example_gpio.exe $(OUTDIR)/example_timer.exe $(OUTDIR)/example_memory.exe

dirs:
	mkdir -p "$(OUTDIR)"

$(OUTDIR)/example_gpio.exe: $(SRC) $(EXAMPLE1)
	$(CC) $(CFLAGS) $(SRC) $(EXAMPLE1) -o $@

$(OUTDIR)/example_timer.exe: $(SRC) $(EXAMPLE2)
	$(CC) $(CFLAGS) $(SRC) $(EXAMPLE2) -o $@

$(OUTDIR)/example_memory.exe: $(SRC) $(EXAMPLE3)
	$(CC) $(CFLAGS) $(SRC) $(EXAMPLE3) -o $@

clean:
	rm -rf "$(OUTDIR)"
