CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -I include
LDFLAGS  = -lm
OUTDIR   = bin
SRCDIR   = src
EXAMPLEDIR = examples

SRCS     = $(wildcard $(SRCDIR)/*.c)
OBJS     = $(patsubst $(SRCDIR)/%.c, $(OUTDIR)/%.o, $(SRCS))
EXAMPLES_SRCS = $(wildcard $(EXAMPLEDIR)/*.c)
EXAMPLES_BINS = $(patsubst $(EXAMPLEDIR)/%.c, $(OUTDIR)/%, $(EXAMPLES_SRCS))

.PHONY: all clean examples

all: $(OUTDIR) $(OBJS)
	@echo "  [OK] mini-tinyml library objects built."

$(OUTDIR):
	if not exist "$(OUTDIR)" mkdir "$(OUTDIR)"

$(OUTDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

examples: $(OUTDIR) $(EXAMPLES_BINS)

$(OUTDIR)/%: $(EXAMPLEDIR)/%.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

clean:
	@if exist "$(OUTDIR)" rmdir /s /q "$(OUTDIR)"
	@echo "  [OK] Cleaned build artifacts."
