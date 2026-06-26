#include "gpio_uart.h"
#include <stddef.h>
#include <string.h>

/* ── GPIO register offsets ───────────────────────────── */
#define GPIO_MODER_OFFSET     0x00UL
#define GPIO_OTYPER_OFFSET    0x04UL
#define GPIO_OSPEEDR_OFFSET   0x08UL
#define GPIO_PUPDR_OFFSET     0x0CUL
#define GPIO_IDR_OFFSET       0x10UL
#define GPIO_ODR_OFFSET       0x14UL
#define GPIO_BSRR_OFFSET      0x18UL
#define GPIO_LCKR_OFFSET      0x1CUL
#define GPIO_AFRL_OFFSET      0x20UL
#define GPIO_AFRH_OFFSET      0x24UL

/* ── UART register offsets ───────────────────────────── */
#define UART_SR_OFFSET        0x00UL
#define UART_DR_OFFSET        0x04UL
#define UART_BRR_OFFSET       0x08UL
#define UART_CR1_OFFSET       0x0CUL
#define UART_CR2_OFFSET       0x10UL
#define UART_CR3_OFFSET       0x14UL

/* ── SPI register offsets ────────────────────────────── */
#define SPI_CR1_OFFSET        0x00UL
#define SPI_CR2_OFFSET        0x04UL
#define SPI_SR_OFFSET         0x08UL
#define SPI_DR_OFFSET         0x0CUL
#define SPI_CRCPR_OFFSET      0x10UL
#define SPI_TXCRCR_OFFSET     0x14UL
#define SPI_I2SCFGR_OFFSET    0x1CUL

/* ── I2C register offsets ────────────────────────────── */
#define I2C_CR1_OFFSET        0x00UL
#define I2C_CR2_OFFSET        0x04UL
#define I2C_OAR1_OFFSET       0x08UL
#define I2C_OAR2_OFFSET       0x0CUL
#define I2C_DR_OFFSET         0x10UL
#define I2C_SR1_OFFSET        0x14UL
#define I2C_SR2_OFFSET        0x18UL
#define I2C_CCR_OFFSET        0x1CUL
#define I2C_TRISE_OFFSET      0x20UL

/* ── GPIO ────────────────────────────────────────────── */

void gpio_init(gpio_pin_t pin, gpio_mode_t mode, gpio_output_type_t otype,
    gpio_pull_config_t pull, uint32_t speed)
{
    volatile uint32_t *moder = (volatile uint32_t *)(pin.port_base + GPIO_MODER_OFFSET);
    volatile uint32_t *otyper = (volatile uint32_t *)(pin.port_base + GPIO_OTYPER_OFFSET);
    volatile uint32_t *ospeedr = (volatile uint32_t *)(pin.port_base + GPIO_OSPEEDR_OFFSET);
    volatile uint32_t *pupdr = (volatile uint32_t *)(pin.port_base + GPIO_PUPDR_OFFSET);
    uint32_t pin_pos;
    for (pin_pos = 0; pin_pos < 16; pin_pos++) {
        if (pin.pin_mask & (1U << pin_pos)) {
            *moder   &= ~(3UL << (pin_pos * 2));
            *moder   |= ((uint32_t)mode << (pin_pos * 2));
            *ospeedr &= ~(3UL << (pin_pos * 2));
            *ospeedr |= (speed << (pin_pos * 2));
            *otyper  &= ~(1UL << pin_pos);
            *otyper  |= ((uint32_t)otype << pin_pos);
            *pupdr   &= ~(3UL << (pin_pos * 2));
            *pupdr   |= ((uint32_t)pull << (pin_pos * 2));
        }
    }
}

void gpio_set_alt_func(gpio_pin_t pin, gpio_alt_func_t af) {
    volatile uint32_t *afr;
    uint32_t pin_pos;
    for (pin_pos = 0; pin_pos < 16; pin_pos++) {
        if (pin.pin_mask & (1U << pin_pos)) {
            afr = (pin_pos < 8)
                ? (volatile uint32_t *)(pin.port_base + GPIO_AFRL_OFFSET)
                : (volatile uint32_t *)(pin.port_base + GPIO_AFRH_OFFSET);
            uint32_t shift = (pin_pos & 0x7U) * 4;
            *afr &= ~(0xFUL << shift);
            *afr |= ((uint32_t)af << shift);
        }
    }
}

void gpio_write_pin(gpio_pin_t pin, bool value) {
    volatile uint32_t *bsrr = (volatile uint32_t *)(pin.port_base + GPIO_BSRR_OFFSET);
    if (value) {
        *bsrr = pin.pin_mask;
    } else {
        *bsrr = (uint32_t)pin.pin_mask << 16;
    }
}

void gpio_write_port(uint32_t port_base, uint16_t value) {
    volatile uint32_t *odr = (volatile uint32_t *)(port_base + GPIO_ODR_OFFSET);
    *odr = value;
}

bool gpio_read_pin(gpio_pin_t pin) {
    volatile uint32_t *idr = (volatile uint32_t *)(pin.port_base + GPIO_IDR_OFFSET);
    return (*idr & pin.pin_mask) != 0;
}

uint16_t gpio_read_port(uint32_t port_base) {
    volatile uint32_t *idr = (volatile uint32_t *)(port_base + GPIO_IDR_OFFSET);
    return (uint16_t)(*idr & 0xFFFFU);
}

void gpio_toggle_pin(gpio_pin_t pin) {
    volatile uint32_t *odr = (volatile uint32_t *)(pin.port_base + GPIO_ODR_OFFSET);
    *odr ^= pin.pin_mask;
}

void gpio_set_pull(gpio_pin_t pin, gpio_pull_config_t pull) {
    volatile uint32_t *pupdr = (volatile uint32_t *)(pin.port_base + GPIO_PUPDR_OFFSET);
    uint32_t pin_pos;
    for (pin_pos = 0; pin_pos < 16; pin_pos++) {
        if (pin.pin_mask & (1U << pin_pos)) {
            *pupdr &= ~(3UL << (pin_pos * 2));
            *pupdr |= ((uint32_t)pull << (pin_pos * 2));
        }
    }
}

void gpio_lock_pin(gpio_pin_t pin) {
    volatile uint32_t *lckr = (volatile uint32_t *)(pin.port_base + GPIO_LCKR_OFFSET);
    *lckr = (1UL << 16) | pin.pin_mask;
    *lckr = pin.pin_mask;
    *lckr = (1UL << 16) | pin.pin_mask;
    (void)*lckr;
}

/* ── UART ────────────────────────────────────────────── */

void uart_init(const uart_config_t *config) {
    volatile uint32_t *cr1 = (volatile uint32_t *)(config->uart_base + UART_CR1_OFFSET);
    volatile uint32_t *cr2 = (volatile uint32_t *)(config->uart_base + UART_CR2_OFFSET);
    volatile uint32_t *brr = (volatile uint32_t *)(config->uart_base + UART_BRR_OFFSET);
    uint32_t temp = 0;
    uint32_t apb_clock = 84000000UL;
    if (config->oversampling == UART_OVERSAMPLING_8) {
        *brr = (apb_clock + config->baud_rate / 2) / config->baud_rate;
    } else {
        *brr = ((apb_clock / 2) + config->baud_rate / 2) / config->baud_rate;
        *brr = (*brr / 2) & 0xFFFFU;
    }
    if (config->word_length == UART_WORD_LENGTH_9) {
        temp |= (1UL << 12);
    }
    if (config->parity != UART_PARITY_NONE) {
        temp |= (1UL << 10);
        if (config->parity == UART_PARITY_ODD) temp |= (1UL << 9);
    }
    if (config->stop_bits > UART_STOP_BITS_1) {
        *cr2 |= ((uint32_t)config->stop_bits << 12);
    }
    if (config->oversampling == UART_OVERSAMPLING_8) {
        temp |= (1UL << 15);
    }
    if (config->flow_ctrl & UART_FLOW_CTRL_RTS) {
        *cr3()| = (1UL << 8);
    }
    if (config->flow_ctrl & UART_FLOW_CTRL_CTS) {
        *cr3()| = (1UL << 9);
    }
    temp |= UART_CR1_UE | UART_CR1_TE | UART_CR1_RE;
    *cr1 = temp;
}

void uart_deinit(uint32_t uart_base) {
    volatile uint32_t *cr1 = (volatile uint32_t *)(uart_base + UART_CR1_OFFSET);
    *cr1 = 0;
}

void uart_transmit_byte(uint32_t uart_base, uint8_t byte) {
    volatile uint32_t *dr = (volatile uint32_t *)(uart_base + UART_DR_OFFSET);
    volatile uint32_t *sr = (volatile uint32_t *)(uart_base + UART_SR_OFFSET);
    while (!(*sr & UART_SR_TXE)) { }
    *dr = byte;
}

void uart_transmit(uint32_t uart_base, const uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        uart_transmit_byte(uart_base, data[i]);
    }
}

uint8_t uart_receive_byte(uint32_t uart_base) {
    volatile uint32_t *sr = (volatile uint32_t *)(uart_base + UART_SR_OFFSET);
    volatile uint32_t *dr = (volatile uint32_t *)(uart_base + UART_DR_OFFSET);
    while (!(*sr & UART_SR_RXNE)) { }
    return (uint8_t)*dr;
}

uint32_t uart_receive(uint32_t uart_base, uint8_t *buffer, uint32_t max_size) {
    uint32_t count = 0;
    volatile uint32_t *sr = (volatile uint32_t *)(uart_base + UART_SR_OFFSET);
    while (count < max_size && (*sr & UART_SR_RXNE)) {
        volatile uint32_t *dr = (volatile uint32_t *)(uart_base + UART_DR_OFFSET);
        buffer[count++] = (uint8_t)*dr;
    }
    return count;
}

void uart_enable_interrupt(uint32_t uart_base, uint16_t flags) {
    volatile uint32_t *cr1 = (volatile uint32_t *)(uart_base + UART_CR1_OFFSET);
    *cr1 |= flags;
}

void uart_disable_interrupt(uint32_t uart_base, uint16_t flags) {
    volatile uint32_t *cr1 = (volatile uint32_t *)(uart_base + UART_CR1_OFFSET);
    *cr1 &= ~((uint32_t)flags);
}

uint32_t uart_get_status(uint32_t uart_base) {
    volatile uint32_t *sr = (volatile uint32_t *)(uart_base + UART_SR_OFFSET);
    return *sr;
}

void uart_send_string(uint32_t uart_base, const char *str) {
    while (*str) {
        uart_transmit_byte(uart_base, (uint8_t)*str);
        str++;
    }
}

uint32_t uart_receive_line(uint32_t uart_base, char *buffer, uint32_t max_size) {
    uint32_t count = 0;
    char ch;
    do {
        ch = (char)uart_receive_byte(uart_base);
        if (ch != '\r' && ch != '\n' && count < (max_size - 1)) {
            buffer[count++] = ch;
        }
    } while (ch != '\n' && count < (max_size - 1));
    buffer[count] = '\0';
    return count;
}

/* ── SPI ─────────────────────────────────────────────── */

void spi_init(const spi_config_t *config) {
    volatile uint32_t *cr1 = (volatile uint32_t *)(config->spi_base + SPI_CR1_OFFSET);
    uint32_t reg = 0;
    if (config->mode == SPI_MODE_MASTER) {
        reg |= (1UL << 2);
    }
    if (config->cpol)  reg |= (1UL << 1);
    if (config->cpha)  reg |= (1UL << 0);
    reg |= ((uint32_t)config->baud_prescaler << 3);
    reg |= ((uint32_t)config->data_size & 0xFU) << 8;
    if (config->first_bit) reg |= (1UL << 7);
    reg |= (1UL << 6);
    reg |= (1UL << 9);
    *cr1 = reg;
}

void spi_deinit(uint32_t spi_base) {
    volatile uint32_t *cr1 = (volatile uint32_t *)(spi_base + SPI_CR1_OFFSET);
    *cr1 = 0;
}

uint8_t spi_transmit_receive_8(uint32_t spi_base, uint8_t data) {
    volatile uint32_t *dr = (volatile uint32_t *)(spi_base + SPI_DR_OFFSET);
    volatile uint32_t *sr = (volatile uint32_t *)(spi_base + SPI_SR_OFFSET);
    while (!(*sr & SPI_SR_TXE)) { }
    *dr = data;
    while (!(*sr & SPI_SR_RXNE)) { }
    while (*sr & SPI_SR_BSY) { }
    return (uint8_t)*dr;
}

uint16_t spi_transmit_receive_16(uint32_t spi_base, uint16_t data) {
    volatile uint32_t *dr = (volatile uint32_t *)(spi_base + SPI_DR_OFFSET);
    volatile uint32_t *sr = (volatile uint32_t *)(spi_base + SPI_SR_OFFSET);
    while (!(*sr & SPI_SR_TXE)) { }
    *dr = data;
    while (!(*sr & SPI_SR_RXNE)) { }
    while (*sr & SPI_SR_BSY) { }
    return (uint16_t)*dr;
}

void spi_transmit_8(uint32_t spi_base, uint8_t data) {
    volatile uint32_t *dr = (volatile uint32_t *)(spi_base + SPI_DR_OFFSET);
    volatile uint32_t *sr = (volatile uint32_t *)(spi_base + SPI_SR_OFFSET);
    while (!(*sr & SPI_SR_TXE)) { }
    *dr = data;
}

void spi_transmit_16(uint32_t spi_base, uint16_t data) {
    volatile uint32_t *dr = (volatile uint32_t *)(spi_base + SPI_DR_OFFSET);
    volatile uint32_t *sr = (volatile uint32_t *)(spi_base + SPI_SR_OFFSET);
    while (!(*sr & SPI_SR_TXE)) { }
    *dr = data;
}

uint8_t spi_receive_8(uint32_t spi_base) {
    return spi_transmit_receive_8(spi_base, 0xFF);
}

uint16_t spi_receive_16(uint32_t spi_base) {
    return spi_transmit_receive_16(spi_base, 0xFFFF);
}

void spi_enable_interrupt(uint32_t spi_base, uint16_t flags) {
    volatile uint32_t *cr2 = (volatile uint32_t *)(spi_base + SPI_CR2_OFFSET);
    *cr2 |= flags;
}

void spi_disable_interrupt(uint32_t spi_base, uint16_t flags) {
    volatile uint32_t *cr2 = (volatile uint32_t *)(spi_base + SPI_CR2_OFFSET);
    *cr2 &= ~((uint32_t)flags);
}

bool spi_is_busy(uint32_t spi_base) {
    volatile uint32_t *sr = (volatile uint32_t *)(spi_base + SPI_SR_OFFSET);
    return (*sr & SPI_SR_BSY) != 0;
}

/* ── I2C ─────────────────────────────────────────────── */

void i2c_init(const i2c_config_t *config) {
    volatile uint32_t *cr1 = (volatile uint32_t *)(config->i2c_base + I2C_CR1_OFFSET);
    volatile uint32_t *cr2 = (volatile uint32_t *)(config->i2c_base + I2C_CR2_OFFSET);
    volatile uint32_t *ccr = (volatile uint32_t *)(config->i2c_base + I2C_CCR_OFFSET);
    volatile uint32_t *trise = (volatile uint32_t *)(config->i2c_base + I2C_TRISE_OFFSET);
    volatile uint32_t *oar1 = (volatile uint32_t *)(config->i2c_base + I2C_OAR1_OFFSET);
    *cr1 &= ~I2C_CR1_PE;
    *cr2 = 42;
    uint32_t pclk = 42000000UL;
    if (config->clock_speed <= 100000) {
        *ccr = (pclk / (config->clock_speed << 1)) & 0xFFFUL;
        *trise = 43;
    } else {
        if (config->duty_cycle == I2C_DUTY_CYCLE_2) {
            *ccr = (pclk / (config->clock_speed * 3)) & 0xFFFUL;
        } else {
            *ccr = (pclk / (config->clock_speed * 25)) & 0xFFFUL;
            *ccr |= (1UL << 15);
        }
        *trise = 12;
    }
    *oar1 = (uint32_t)(config->own_address << 1);
    if (config->addr_mode == I2C_ADDR_MODE_10BIT) {
        *oar1 |= (1UL << 15);
    }
    *cr1 = I2C_CR1_PE;
}

void i2c_deinit(uint32_t i2c_base) {
    volatile uint32_t *cr1 = (volatile uint32_t *)(i2c_base + I2C_CR1_OFFSET);
    *cr1 = 0;
}

void i2c_generate_start(uint32_t i2c_base) {
    volatile uint32_t *cr1 = (volatile uint32_t *)(i2c_base + I2C_CR1_OFFSET);
    volatile uint32_t *sr1 = (volatile uint32_t *)(i2c_base + I2C_SR1_OFFSET);
    *cr1 |= I2C_CR1_START;
    while (!(*sr1 & I2C_SR_SB)) { }
}

void i2c_generate_stop(uint32_t i2c_base) {
    volatile uint32_t *cr1 = (volatile uint32_t *)(i2c_base + I2C_CR1_OFFSET);
    *cr1 |= I2C_CR1_STOP;
}

void i2c_send_address(uint32_t i2c_base, uint8_t address, bool is_read) {
    volatile uint32_t *dr = (volatile uint32_t *)(i2c_base + I2C_DR_OFFSET);
    volatile uint32_t *sr1 = (volatile uint32_t *)(i2c_base + I2C_SR1_OFFSET);
    volatile uint32_t *sr2 = (volatile uint32_t *)(i2c_base + I2C_SR2_OFFSET);
    *dr = (uint32_t)((address << 1) | (is_read ? 1U : 0U));
    while (!(*sr1 & I2C_SR_ADDR)) { }
    (void)*sr2;
}

bool i2c_send_data(uint32_t i2c_base, uint8_t data) {
    volatile uint32_t *dr = (volatile uint32_t *)(i2c_base + I2C_DR_OFFSET);
    volatile uint32_t *sr1 = (volatile uint32_t *)(i2c_base + I2C_SR1_OFFSET);
    while (!(*sr1 & I2C_SR_TXE)) { }
    *dr = data;
    while (!(*sr1 & (I2C_SR_TXE | I2C_SR_BTF))) { }
    return (*sr1 & I2C_SR_AF) == 0;
}

uint8_t i2c_receive_data(uint32_t i2c_base, bool ack) {
    volatile uint32_t *cr1 = (volatile uint32_t *)(i2c_base + I2C_CR1_OFFSET);
    volatile uint32_t *dr = (volatile uint32_t *)(i2c_base + I2C_DR_OFFSET);
    volatile uint32_t *sr1 = (volatile uint32_t *)(i2c_base + I2C_SR1_OFFSET);
    if (ack) {
        *cr1 |= I2C_CR1_ACK;
    } else {
        *cr1 &= ~I2C_CR1_ACK;
    }
    while (!(*sr1 & I2C_SR_RXNE)) { }
    return (uint8_t)*dr;
}

bool i2c_master_transmit(uint32_t i2c_base, uint8_t dev_addr,
    const uint8_t *data, uint32_t size)
{
    i2c_generate_start(i2c_base);
    i2c_send_address(i2c_base, dev_addr, false);
    for (uint32_t i = 0; i < size; i++) {
        if (!i2c_send_data(i2c_base, data[i])) {
            i2c_generate_stop(i2c_base);
            return false;
        }
    }
    i2c_generate_stop(i2c_base);
    return true;
}

bool i2c_master_receive(uint32_t i2c_base, uint8_t dev_addr,
    uint8_t *buffer, uint32_t size)
{
    i2c_generate_start(i2c_base);
    i2c_send_address(i2c_base, dev_addr, true);
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = i2c_receive_data(i2c_base, (i < (size - 1)));
    }
    i2c_generate_stop(i2c_base);
    return true;
}

bool i2c_master_write_read(uint32_t i2c_base, uint8_t dev_addr,
    uint8_t reg_addr, const uint8_t *tx_data, uint32_t tx_size,
    uint8_t *rx_buffer, uint32_t rx_size)
{
    i2c_generate_start(i2c_base);
    i2c_send_address(i2c_base, dev_addr, false);
    if (!i2c_send_data(i2c_base, reg_addr)) {
        i2c_generate_stop(i2c_base);
        return false;
    }
    if (tx_data && tx_size > 0) {
        for (uint32_t i = 0; i < tx_size; i++) {
            if (!i2c_send_data(i2c_base, tx_data[i])) {
                i2c_generate_stop(i2c_base);
                return false;
            }
        }
    }
    i2c_generate_start(i2c_base);
    i2c_send_address(i2c_base, dev_addr, true);
    for (uint32_t i = 0; i < rx_size; i++) {
        rx_buffer[i] = i2c_receive_data(i2c_base, (i < (rx_size - 1)));
    }
    i2c_generate_stop(i2c_base);
    return true;
}

bool i2c_is_device_ready(uint32_t i2c_base, uint8_t dev_addr,
    uint32_t retries)
{
    volatile uint32_t *cr1 = (volatile uint32_t *)(i2c_base + I2C_CR1_OFFSET);
    volatile uint32_t *sr1 = (volatile uint32_t *)(i2c_base + I2C_SR1_OFFSET);
    for (uint32_t i = 0; i < retries; i++) {
        i2c_generate_start(i2c_base);
        *cr1 |= I2C_CR1_START;
        while (!(*sr1 & I2C_SR_SB)) { }
        volatile uint32_t *dr = (volatile uint32_t *)(i2c_base + I2C_DR_OFFSET);
        volatile uint32_t *sr2 = (volatile uint32_t *)(i2c_base + I2C_SR2_OFFSET);
        *dr = (uint32_t)(dev_addr << 1);
        uint32_t timeout = 0;
        while (!(*sr1 & (I2C_SR_ADDR | I2C_SR_AF)) && timeout < 10000) {
            timeout++;
        }
        if (*sr1 & I2C_SR_ADDR) {
            (void)*sr2;
            i2c_generate_stop(i2c_base);
            return true;
        }
        *cr1 |= I2C_CR1_STOP;
    }
    return false;
}
