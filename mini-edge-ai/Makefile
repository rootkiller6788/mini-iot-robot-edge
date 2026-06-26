CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -Isrc
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

.PHONY: all lib examples demos setup clean help

all: setup lib examples demos
	@echo Done.

lib: $(LIB)
	@echo Library built: $(LIB)

examples: setup $(addprefix $(BUILD)/, $(EXAMPLES))
	@echo Examples built.

demos: setup $(addprefix $(BUILD)/, $(DEMOS))
	@echo Demos built.

setup:
	if not exist "$(BUILD)" mkdir "$(BUILD)"

$(LIB): $(OBJECTS)
	$(AR) $@ $^

$(BUILD)/%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/example_classification: example_classification.c $(LIB)
	$(CC) $(CFLAGS) $< -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

$(BUILD)/example_detection: example_detection.c $(LIB)
	$(CC) $(CFLAGS) $< -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

$(BUILD)/example_keyword_spotting: example_keyword_spotting.c $(LIB)
	$(CC) $(CFLAGS) $< -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

$(BUILD)/demo_person_detection: demo_person_detection.c $(LIB)
	$(CC) $(CFLAGS) $< -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

$(BUILD)/demo_federated_learning: demo_federated_learning.c $(LIB)
	$(CC) $(CFLAGS) $< -L$(BUILD) -lmini_edge $(LDFLAGS) -o $@

clean:
	if exist "$(BUILD)" rmdir /S /Q "$(BUILD)"
	@echo Cleaned.

help:
	@echo Targets: all, lib, examples, demos, clean, help
