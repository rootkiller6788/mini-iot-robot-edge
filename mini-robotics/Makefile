CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2 -Iinclude
AR      = ar
ARFLAGS = rcs

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=build/%.o)
LIB  = build/libmini-robotics.a

EXAMPLES_SRC = $(wildcard examples/*.c)
EXAMPLES_BIN = $(EXAMPLES_SRC:examples/%.c=build/%)

LIBS = -lm

.PHONY: all lib examples clean

all: lib examples

lib: $(LIB)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	if not exist build mkdir build

examples: $(EXAMPLES_BIN)

build/ex1_forward_kinematics: examples/ex1_forward_kinematics.c $(LIB)
	$(CC) $(CFLAGS) $< -o $@ -Lbuild -lmini-robotics $(LIBS)

build/ex2_path_planning: examples/ex2_path_planning.c $(LIB)
	$(CC) $(CFLAGS) $< -o $@ -Lbuild -lmini-robotics $(LIBS)

build/ex3_slam_demo: examples/ex3_slam_demo.c $(LIB)
	$(CC) $(CFLAGS) $< -o $@ -Lbuild -lmini-robotics $(LIBS)

clean:
	if exist build rmdir /s /q build
