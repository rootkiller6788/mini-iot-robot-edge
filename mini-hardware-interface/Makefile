CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -O2
AR      = ar
ARFLAGS = rcs

INCDIR  = include
SRCDIR  = src
OBJDIR  = build
LIBDIR  = lib
BINDIR  = bin

INCLUDES = -I$(INCDIR)

SRCS = $(SRCDIR)/gpio_pwm_adc.c \
       $(SRCDIR)/sensor_polling.c \
       $(SRCDIR)/pid_controller.c \
       $(SRCDIR)/actuator_driver.c \
       $(SRCDIR)/signal_cond.c

OBJS = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

LIB = $(LIBDIR)/libmini-hardware-interface.a

EXAMPLES = $(BINDIR)/example_gpio_servo \
           $(BINDIR)/example_sensor \
           $(BINDIR)/example_pid_motor

DEMOS = $(BINDIR)/demo_robot_control \
        $(BINDIR)/demo_env_monitor

.PHONY: all clean examples demos dirs

all: dirs $(LIB)

dirs:
	@if not exist "$(OBJDIR)" mkdir "$(OBJDIR)"
	@if not exist "$(LIBDIR)" mkdir "$(LIBDIR)"
	@if not exist "$(BINDIR)" mkdir "$(BINDIR)"

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

examples: dirs $(EXAMPLES)

$(BINDIR)/example_gpio_servo: examples/example_gpio_servo.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIBDIR) -lmini-hardware-interface -lm -o $@

$(BINDIR)/example_sensor: examples/example_sensor.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIBDIR) -lmini-hardware-interface -lm -o $@

$(BINDIR)/example_pid_motor: examples/example_pid_motor.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIBDIR) -lmini-hardware-interface -lm -o $@

demos: dirs $(DEMOS)

$(BINDIR)/demo_robot_control: demos/demo_robot_control.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIBDIR) -lmini-hardware-interface -lm -o $@

$(BINDIR)/demo_env_monitor: demos/demo_env_monitor.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIBDIR) -lmini-hardware-interface -lm -o $@

clean:
	@if exist "$(OBJDIR)" rmdir /s /q "$(OBJDIR)"
	@if exist "$(LIBDIR)" rmdir /s /q "$(LIBDIR)"
	@if exist "$(BINDIR)" rmdir /s /q "$(BINDIR)"
