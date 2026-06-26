CC       ?= gcc
CFLAGS   ?= -std=c99 -Wall -Wextra -Wpedantic -O2 -g
AR       ?= ar

INCLUDES = -I include
LDFLAGS  = -lm

SRC_DIR  = src
INC_DIR  = include
EXA_DIR  = examples
BLD_DIR  = build

SOURCES  = $(wildcard $(SRC_DIR)/*.c)
OBJECTS  = $(patsubst $(SRC_DIR)/%.c, $(BLD_DIR)/%.o, $(SOURCES))
LIBRARY  = $(BLD_DIR)/libmini_ota.a

EXAMPLES  = $(wildcard $(EXA_DIR)/*.c)
EXAMPLES_BIN = $(patsubst $(EXA_DIR)/%.c, $(BLD_DIR)/%, $(EXAMPLES))

.PHONY: all clean examples run $(EXAMPLES_BIN)

all: $(LIBRARY) examples

$(BLD_DIR):
	@if not exist "$(BLD_DIR)" mkdir "$(BLD_DIR)"

$(BLD_DIR)/%.o: $(SRC_DIR)/%.c | $(BLD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIBRARY): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

$(BLD_DIR)/basic_ota: $(EXA_DIR)/basic_ota.c $(LIBRARY) | $(BLD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BLD_DIR) -lmini_ota $(LDFLAGS) -o $@

$(BLD_DIR)/delta_ota: $(EXA_DIR)/delta_ota.c $(LIBRARY) | $(BLD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BLD_DIR) -lmini_ota $(LDFLAGS) -o $@

$(BLD_DIR)/server_ota: $(EXA_DIR)/server_ota.c $(LIBRARY) | $(BLD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BLD_DIR) -lmini_ota $(LDFLAGS) -o $@

examples: $(EXAMPLES_BIN)

basic_ota: $(BLD_DIR)/basic_ota
delta_ota: $(BLD_DIR)/delta_ota
server_ota: $(BLD_DIR)/server_ota

run: examples
	@echo Running basic_ota...
	@$(BLD_DIR)/basic_ota
	@echo.
	@echo Running delta_ota...
	@$(BLD_DIR)/delta_ota
	@echo.
	@echo Running server_ota...
	@$(BLD_DIR)/server_ota

clean:
	@if exist "$(BLD_DIR)" rmdir /s /q "$(BLD_DIR)"

help:
	@echo mini-ota-update Makefile
	@echo.
	@echo Targets:
	@echo   all          Build library and all examples
	@echo   examples     Build all example binaries
	@echo   basic_ota    Build basic OTA example
	@echo   delta_ota    Build delta OTA example
	@echo   server_ota   Build server OTA example
	@echo   run          Build and run all examples
	@echo   clean        Remove build artifacts
	@echo   help         Show this help
	@echo.
	@echo Variables:
	@echo   CC           C compiler (default: gcc)
	@echo   CFLAGS       Compiler flags (default: -std=c99 -Wall -Wextra...)
	@echo   AR           Archiver (default: ar)
