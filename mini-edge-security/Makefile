CC = gcc
CFLAGS = -Wall -Wextra -O2 -I include
LDFLAGS = -lm

MINGW_BIN = D:/msys64/mingw64/bin
MINGW_USR = D:/msys64/usr/bin
EXPORT_PATH = export PATH=$(MINGW_BIN):$(MINGW_USR):$$PATH

SRCDIR = src
EXDIR = examples
OUTDIR = bin

SRCS = $(SRCDIR)/secure_boot_mcu.c $(SRCDIR)/trustzone_arm.c \
       $(SRCDIR)/psa_certified.c $(SRCDIR)/device_attest.c \
       $(SRCDIR)/secure_storage.c

OBJS = $(SRCS:$(SRCDIR)/%.c=$(OUTDIR)/%.o)

DEMOS = secure_boot_demo attestation_demo secure_storage_demo

all: $(OBJS)
	@echo "All library objects compiled."

$(OUTDIR):
	@mkdir -p "$(OUTDIR)"

$(OUTDIR)/%.o: $(SRCDIR)/%.c | $(OUTDIR)
	$(EXPORT_PATH) && $(CC) $(CFLAGS) -c $< -o $@

secure_boot_demo: $(EXDIR)/secure_boot_demo.c $(OUTDIR)/secure_boot_mcu.o | $(OUTDIR)
	$(EXPORT_PATH) && $(CC) $(CFLAGS) $< $(OUTDIR)/secure_boot_mcu.o -o $(OUTDIR)/secure_boot_demo $(LDFLAGS)
	@echo "  Running secure_boot_demo..."
	$(OUTDIR)/secure_boot_demo

attestation_demo: $(EXDIR)/attestation_demo.c $(OUTDIR)/device_attest.o $(OUTDIR)/psa_certified.o | $(OUTDIR)
	$(EXPORT_PATH) && $(CC) $(CFLAGS) $< $(OUTDIR)/device_attest.o $(OUTDIR)/psa_certified.o -o $(OUTDIR)/attestation_demo $(LDFLAGS)
	@echo "  Running attestation_demo..."
	$(OUTDIR)/attestation_demo

secure_storage_demo: $(EXDIR)/secure_storage_demo.c $(OUTDIR)/secure_storage.o | $(OUTDIR)
	$(EXPORT_PATH) && $(CC) $(CFLAGS) $< $(OUTDIR)/secure_storage.o -o $(OUTDIR)/secure_storage_demo $(LDFLAGS)
	@echo "  Running secure_storage_demo..."
	$(OUTDIR)/secure_storage_demo

test: secure_boot_demo attestation_demo secure_storage_demo
	@echo ""
	@echo "========================================"
	@echo "  ALL TESTS COMPLETE"
	@echo "========================================"

clean:
	@rm -rf "$(OUTDIR)"
	@echo "Cleaned build artifacts."

.PHONY: all test clean secure_boot_demo attestation_demo secure_storage_demo
