#include "gpio_uart.h"
#include "mcu_peripheral.h"
#include "nvic_dma.h"
#include <stdio.h>
#include <string.h>

#define PCLK1 42000000UL

static uint16_t mock_spi_device_read(uint32_t spi_base, uint8_t reg_addr) {
    spi_transmit_8(spi_base, reg_addr);
    return spi_receive_8(spi_base);
}

static void demo_gpio(void) {
    printf("\n=== GPIO Demo ===\n");

    gpio_pin_t led_pin   = { GPIOA_BASE, GPIO_PIN_5 };
    gpio_pin_t button_pin = { GPIOC_BASE, GPIO_PIN_13 };

    gpio_init(led_pin, GPIO_MODE_OUTPUT, GPIO_OTYPE_PUSH_PULL, GPIO_PUPD_NONE, GPIO_SPEED_HIGH);
    gpio_init(button_pin, GPIO_MODE_INPUT, GPIO_OTYPE_PUSH_PULL, GPIO_PUPD_UP, GPIO_SPEED_LOW);

    gpio_write_pin(led_pin, true);
    printf("  LED (PA5) = ON\n");

    bool button_state = gpio_read_pin(button_pin);
    printf("  Button (PC13) = %s\n", button_state ? "PRESSED" : "RELEASED");

    gpio_toggle_pin(led_pin);
    printf("  LED toggled\n");

    gpio_pin_t uart_tx = { GPIOA_BASE, GPIO_PIN_9 };
    gpio_pin_t uart_rx = { GPIOA_BASE, GPIO_PIN_10 };
    gpio_init(uart_tx, GPIO_MODE_ALTFUNC, GPIO_OTYPE_PUSH_PULL, GPIO_PUPD_UP, GPIO_SPEED_HIGH);
    gpio_init(uart_rx, GPIO_MODE_ALTFUNC, GPIO_OTYPE_PUSH_PULL, GPIO_PUPD_UP, GPIO_SPEED_HIGH);
    gpio_set_alt_func(uart_tx, GPIO_AF7);
    gpio_set_alt_func(uart_rx, GPIO_AF7);
    printf("  UART1 TX/RX configured (PA9/AF7, PA10/AF7)\n");

    gpio_lock_pin(led_pin);
    printf("  PA5 pin locked\n");

    printf("  Port A input data: 0x%04X\n", gpio_read_port(GPIOA_BASE));
}

static void demo_uart(void) {
    printf("\n=== UART Demo ===\n");

    uart_config_t uart_cfg;
    uart_cfg.uart_base    = USART2_BASE;
    uart_cfg.baud_rate    = 115200;
    uart_cfg.word_length  = UART_WORD_LENGTH_8;
    uart_cfg.stop_bits    = UART_STOP_BITS_1;
    uart_cfg.parity       = UART_PARITY_NONE;
    uart_cfg.flow_ctrl    = UART_FLOW_CTRL_NONE;
    uart_cfg.oversampling = UART_OVERSAMPLING_16;

    uart_init(&uart_cfg);
    printf("  USART2 initialized @ 115200 baud 8N1\n");

    const char *msg = "Hello from mini-mcu UART!\r\n";
    uart_send_string(USART2_BASE, msg);
    printf("  TX: '%s'\n", msg);

    uint32_t status = uart_get_status(USART2_BASE);
    printf("  UART status register: 0x%08lX\n", (unsigned long)status);

    uart_enable_interrupt(USART2_BASE, UART_CR1_RXNEIE);
    printf("  RXNE interrupt enabled\n");

    uart_disable_interrupt(USART2_BASE, UART_CR1_RXNEIE);
    printf("  RXNE interrupt disabled\n");

    uart_deinit(USART2_BASE);
    printf("  USART2 deinitialized\n");
}

static void demo_spi(void) {
    printf("\n=== SPI Demo ===\n");

    gpio_pin_t spi_sck  = { GPIOB_BASE, GPIO_PIN_3 };
    gpio_pin_t spi_miso = { GPIOB_BASE, GPIO_PIN_4 };
    gpio_pin_t spi_mosi = { GPIOB_BASE, GPIO_PIN_5 };
    gpio_pin_t spi_nss  = { GPIOB_BASE, GPIO_PIN_6 };

    gpio_init(spi_sck,  GPIO_MODE_ALTFUNC, GPIO_OTYPE_PUSH_PULL, GPIO_PUPD_NONE, GPIO_SPEED_HIGH);
    gpio_init(spi_miso, GPIO_MODE_ALTFUNC, GPIO_OTYPE_PUSH_PULL, GPIO_PUPD_NONE, GPIO_SPEED_HIGH);
    gpio_init(spi_mosi, GPIO_MODE_ALTFUNC, GPIO_OTYPE_PUSH_PULL, GPIO_PUPD_NONE, GPIO_SPEED_HIGH);
    gpio_init(spi_nss,  GPIO_MODE_ALTFUNC, GPIO_OTYPE_PUSH_PULL, GPIO_PUPD_NONE, GPIO_SPEED_HIGH);
    gpio_set_alt_func(spi_sck,  GPIO_AF5);
    gpio_set_alt_func(spi_miso, GPIO_AF5);
    gpio_set_alt_func(spi_mosi, GPIO_AF5);

    spi_config_t spi_cfg;
    memset(&spi_cfg, 0, sizeof(spi_cfg));
    spi_cfg.spi_base       = SPI1_BASE;
    spi_cfg.mode           = SPI_MODE_MASTER;
    spi_cfg.direction      = SPI_DIRECTION_2LINES;
    spi_cfg.data_size      = SPI_DATA_SIZE_8BIT;
    spi_cfg.cpol           = SPI_CPOL_LOW;
    spi_cfg.cpha           = SPI_CPHA_1EDGE;
    spi_cfg.baud_prescaler = SPI_BAUD_PRESCALER_16;
    spi_cfg.first_bit      = SPI_FRAME_MSB_FIRST;

    spi_init(&spi_cfg);
    printf("  SPI1 initialized: Master, 8-bit, CPOL=0 CPHA=0, PCLK/16\n");

    uint8_t rx_data = spi_transmit_receive_8(SPI1_BASE, 0x9F);
    printf("  SPI exchange: TX=0x9F RX=0x%02X\n", rx_data);

    spi_enable_interrupt(SPI1_BASE, 0x01);
    printf("  SPI RXNE interrupt enabled\n");

    bool busy = spi_is_busy(SPI1_BASE);
    printf("  SPI busy: %s\n", busy ? "yes" : "no");

    spi_deinit(SPI1_BASE);
    printf("  SPI1 deinitialized\n");
}

static void demo_i2c(void) {
    printf("\n=== I2C Demo ===\n");

    gpio_pin_t i2c_scl = { GPIOB_BASE, GPIO_PIN_6 };
    gpio_pin_t i2c_sda = { GPIOB_BASE, GPIO_PIN_7 };
    gpio_init(i2c_scl, GPIO_MODE_ALTFUNC, GPIO_OTYPE_OPEN_DRAIN, GPIO_PUPD_UP, GPIO_SPEED_HIGH);
    gpio_init(i2c_sda, GPIO_MODE_ALTFUNC, GPIO_OTYPE_OPEN_DRAIN, GPIO_PUPD_UP, GPIO_SPEED_HIGH);
    gpio_set_alt_func(i2c_scl, GPIO_AF4);
    gpio_set_alt_func(i2c_sda, GPIO_AF4);

    i2c_config_t i2c_cfg;
    memset(&i2c_cfg, 0, sizeof(i2c_cfg));
    i2c_cfg.i2c_base     = I2C1_BASE;
    i2c_cfg.clock_speed  = 100000;
    i2c_cfg.duty_cycle   = I2C_DUTY_CYCLE_2;
    i2c_cfg.own_address  = 0x30;
    i2c_cfg.addr_mode    = I2C_ADDR_MODE_7BIT;
    i2c_cfg.general_call = false;
    i2c_cfg.no_stretch   = false;

    i2c_init(&i2c_cfg);
    printf("  I2C1 initialized @ 100kHz, own addr 0x30\n");

    uint8_t dev_addr = 0x50;
    bool ready = i2c_is_device_ready(I2C1_BASE, dev_addr, 3);
    printf("  EEPROM at 0x%02X ready: %s\n", dev_addr, ready ? "yes" : "no");

    uint8_t tx_buf[] = {0x00, 0x10};
    bool tx_ok = i2c_master_transmit(I2C1_BASE, dev_addr, tx_buf, sizeof(tx_buf));
    printf("  I2C master transmit (addr=0, data=0x10): %s\n", tx_ok ? "ACK" : "NACK");

    uint8_t reg = 0x00;
    uint8_t rx_buf[4] = {0};
    bool wr_ok = i2c_master_write_read(I2C1_BASE, dev_addr, reg, NULL, 0, rx_buf, 4);
    printf("  I2C read from reg 0x00: %s\n", wr_ok ? "OK" : "FAIL");

    if (wr_ok) {
        printf("  Data: %02X %02X %02X %02X\n", rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
    }

    i2c_deinit(I2C1_BASE);
    printf("  I2C1 deinitialized\n");
}

int main(void) {
    printf("╔═══════════════════════════════════════╗\n");
    printf("║  mini-mcu GPIO/UART/SPI/I2C Demo     ║\n");
    printf("╚═══════════════════════════════════════╝\n");

    nvic_init();
    demo_gpio();
    demo_uart();
    demo_spi();
    demo_i2c();

    printf("\n=== All peripheral demos complete ===\n");
    return 0;
}
