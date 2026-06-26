CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -O2 -g
INC      = -I.
LDFLAGS  =

SRCS     = yocto_buildroot.c device_tree_overlay.c kernel_module.c \
           initramfs_rootfs.c busybox_app.c
OBJS     = $(SRCS:.c=.o)
HDRS     = yocto_buildroot.h device_tree_overlay.h kernel_module.h \
           initramfs_rootfs.h busybox_app.h

EXAMPLES = example_yocto_buildroot example_device_tree example_kernel_module
DEMOS    = demo_embedded_linux demo_rootfs_boot

.PHONY: all clean run

all: $(EXAMPLES) $(DEMOS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

example_yocto_buildroot: example_yocto_buildroot.c yocto_buildroot.o
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LDFLAGS)

example_device_tree: example_device_tree.c device_tree_overlay.o
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LDFLAGS)

example_kernel_module: example_kernel_module.c kernel_module.o
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LDFLAGS)

demo_embedded_linux: demo_embedded_linux.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LDFLAGS)

demo_rootfs_boot: demo_rootfs_boot.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LDFLAGS)

run: demo_embedded_linux demo_rootfs_boot
	@echo "=== Running demo_embedded_linux ==="
	./demo_embedded_linux
	@echo ""
	@echo "=== Running demo_rootfs_boot ==="
	./demo_rootfs_boot

clean:
	del /f /q $(OBJS) $(EXAMPLES:=.exe) $(DEMOS:=.exe) demo_embedded_linux demo_rootfs_boot 2>nul || rm -f $(OBJS) $(EXAMPLES) $(DEMOS)
