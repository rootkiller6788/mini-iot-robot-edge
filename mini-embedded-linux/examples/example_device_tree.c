#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device_tree_overlay.h"

int main(void)
{
    printf("=== Device Tree & Overlay Example ===\n\n");

    device_tree_t dt;
    dt_init(&dt);

    int soc = dt_node_add(&dt, dt.root_index, "soc");
    dt_prop_add_string(&dt, soc, "compatible", "simple-bus");
    dt_prop_add_u32(&dt, soc, "#address-cells", 1);
    dt_prop_add_u32(&dt, soc, "#size-cells", 1);

    int gpio = dt_node_add(&dt, soc, "gpio@50003000");
    dt_prop_add_string(&dt, gpio, "compatible", "arm,pl061");
    dt_prop_add_u32(&dt, gpio, "reg", 0x50003000);
    dt_prop_add_u32(&dt, gpio, "gpio-controller", 1);
    dt_prop_add_string(&dt, gpio, "gpio-ranges", "&pinctrl 0 0 32");
    dt_phandle_assign(&dt, gpio);

    int i2c = dt_node_add(&dt, soc, "i2c@40002000");
    dt_prop_add_string(&dt, i2c, "compatible", "arm,pl031");
    dt_prop_add_u32(&dt, i2c, "reg", 0x40002000);
    dt_prop_add_u32(&dt, i2c, "clock-frequency", 100000);
    dt_phandle_assign(&dt, i2c);

    int sensor = dt_node_add(&dt, i2c, "sensor@48");
    dt_prop_add_string(&dt, sensor, "compatible", "ti,tmp102");
    dt_prop_add_u32(&dt, sensor, "reg", 0x48);
    dt_prop_add_u32(&dt, sensor, "sampling-interval", 100);

    int chosen = dt_node_add(&dt, dt.root_index, "chosen");
    dt_prop_add_string(&dt, chosen, "bootargs", "console=ttyS0,115200 root=/dev/mmcblk0p2 rw");

    printf("Device tree created with %d nodes\n\n", dt.node_count);
    dt_dump(&dt);

    printf("\n--- Device Tree Overlay ---\n\n");

    dt_overlay_t ov;
    dt_overlay_init(&ov);

    dt_overlay_add_fragment(&ov, "__overlay__");
    int led_node = dt_overlay_add_node(&ov, 0, "leds");

    dt_overlay_t ov2;
    dt_overlay_init(&ov2);
    dt_overlay_add_fragment(&ov2, "__overlay__");
    int btn_node = dt_overlay_add_node(&ov2, 0, "gpio-keys");

    dt_overlay_dump(&ov);
    printf("\nApplying overlays to base device tree...\n");
    int merged = dt_apply_overlays(&dt, (const dt_overlay_t[]){ov, ov2}, 2);
    printf("Merged %d overlay nodes\n\n", merged);
    dt_dump(&dt);

    printf("\n--- GPIO Pinmux via Device Tree ---\n\n");

    dt_pinmux_table_t pm;
    dt_pinmux_setup(&pm, "st,stm32mp157");

    dt_pinmux_add(&pm, 0, 0, 2, 0, 2);
    dt_pinmux_add(&pm, 0, 1, 2, 0, 2);
    dt_pinmux_add(&pm, 0, 10, 1, 1, 1);
    dt_pinmux_add(&pm, 0, 11, 1, 1, 1);
    dt_pinmux_add(&pm, 1, 0, 3, 0, 3);

    dt_pinmux_dump(&pm);
    dt_pinmux_apply_to_dt(&dt, &pm);
    dt_pinmux_generate_overlay(&pm, "output/pinmux-overlay.dtbo");

    printf("\n--- DTB Compilation ---\n");
    dt_compile_dts("board.dts", "board.dtb");
    dt_decompile_dtb("board.dtb", "board.dts");
    dt_write_dtb(&dt, "output/final.dtb");

    printf("\n--- Phandle Resolution ---\n");
    int node_idx = dt_node_find(&dt, "/soc/gpio@50003000");
    if (node_idx >= 0) {
        uint32_t phandle = 0;
        dt_phandle_resolve(&dt, node_idx, &phandle);
        printf("GPIO node phandle: %u\n", phandle);
    }

    printf("\nExample complete.\n");
    return 0;
}
