CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -O2
INC      = -Iinclude
AR       = ar rcs

SRCDIR   = src
INCDIR   = include
OBJDIR   = build
LIBDIR   = lib
EXAMPLES = examples

SRCS     = $(wildcard $(SRCDIR)/*.c)
OBJS     = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
LIB      = $(LIBDIR)/libindustrial.a
EX_SRCS  = $(wildcard $(EXAMPLES)/*.c)
EX_BINS  = $(patsubst $(EXAMPLES)/%.c,$(OBJDIR)/%.exe,$(EX_SRCS))

.PHONY: all clean examples

all: $(LIB) examples

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@if not exist "$(OBJDIR)" mkdir "$(OBJDIR)"
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIBDIR):
	@if not exist "$(LIBDIR)" mkdir "$(LIBDIR)"

$(LIB): $(LIBDIR) $(OBJS)
	$(AR) $@ $(OBJS)

$(OBJDIR)/%.exe: $(EXAMPLES)/%.c $(LIB)
	$(CC) $(CFLAGS) $(INC) $< -L$(LIBDIR) -lindustrial -o $@

examples: $(EX_BINS)

clean:
	@if exist "$(OBJDIR)" rmdir /s /q "$(OBJDIR)"
	@if exist "$(LIBDIR)" rmdir /s /q "$(LIBDIR)"
