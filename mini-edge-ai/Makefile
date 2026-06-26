CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -Iinclude
LDFLAGS  = -lm
AR       = ar rcs
BUILD    = build
SRCDIR   = src

HEADERS  = edge_inference.h model_conversion.h edge_tpu_npu.h inference_opt.h edge_pipeline.h
SOURCES  = edge_inference.c model_conversion.c edge_tpu_npu.c inference_opt.c edge_pipeline.c
OBJECTS  = $(patsubst %.c, $(BUILD)/%.o, $(SOURCES))
LIB      = $(BUILD)/libmini_edge.a
EXAMPLES = example_classification example_detection example_keyword_spotting
DEMOS    = demo_person_detection demo_federated_learning
TARGETS  = $(EXAMPLES) $(DEMOS)

TESTS    = test_core
BENCHES  = bench_core

.PHONY: all lib examples demos test bench setup clean help

all: setup lib examples demos test bench
	@echo Done.

lib: $(LIB)
	@echo Library built: $(LIB)

examples: setup $(addprefix $(BUILD)/, $(EXAMPLES))
	@echo Examples built.

demos: setup $(addprefix $(BUILD)/, $(DEMOS))
	@echo Demos built.

test: setup lib $(addprefix $(BUILD)/, $(TESTS))
	@echo Running tests...
	@$(BUILD)/test_core
	@echo Tests complete.

bench: setup lib $(addprefix $(BUILD)/, $(BENCHES))
	@echo Running benchmarks...
	@$(BUILD)/bench_core 1000
	@echo Benchmarks complete.

setup:
	@mkdir -p "$(BUILD)"

$(LIB): $(OBJECTS)
	$(AR) $@ $^

$(BUILD)/%.o: src/%.c $(addprefix include/, $(HEADERS))
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/example_classification: examples/example_classification.c $(LIB)
	$(CC) $(CFLAGS) $< -Iinclude -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

$(BUILD)/example_detection: examples/example_detection.c $(LIB)
	$(CC) $(CFLAGS) $< -Iinclude -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

$(BUILD)/example_keyword_spotting: examples/example_keyword_spotting.c $(LIB)
	$(CC) $(CFLAGS) $< -Iinclude -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

$(BUILD)/demo_person_detection: examples/demo_person_detection.c $(LIB)
	$(CC) $(CFLAGS) $< -Iinclude -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

$(BUILD)/demo_federated_learning: examples/demo_federated_learning.c $(LIB)
	$(CC) $(CFLAGS) $< -Iinclude -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

$(BUILD)/test_core: tests/test_core.c $(LIB)
	$(CC) $(CFLAGS) $< -Iinclude -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

$(BUILD)/bench_core: benches/bench_core.c $(LIB)
	$(CC) $(CFLAGS) $< -Iinclude -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

clean:
	@rm -rf "$(BUILD)"
	@echo Cleaned.

help:
	@echo Targets: all, lib, examples, demos, test, bench, clean, help
