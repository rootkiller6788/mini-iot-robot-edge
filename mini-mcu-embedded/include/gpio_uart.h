#ifndef GPIO_UART_H
#define GPIO_UART_H

#include <stdbool.h>
#include <stdint.h>

/* ── GPIO base addresses ─────────────────────────────── */
#define GPIOA_BASE            0x40020000UL
#define GPIOB_BASE            0x40020400UL
#define GPIOC_BASE            0x40020800UL
#define GPIOD_BASE            0x40020C00UL
#define GPIOE_BASE            0x40021000UL
#define GPIOF_BASE            0x40021400UL
#define GPIOG_BASE            0x40021800UL
#define GPIOH_BASE            0x40021C00UL

#define GPIO_PIN_0            0x0001U
#define GPIO_PIN_1            0x0002U
#define GPIO_PIN_2            0x0004U
#define GPIO_PIN_3            0x0008U
#define GPIO_PIN_4            0x0010U
#define GPIO_PIN_5            0x0020U
#define GPIO_PIN_6            0x0040U
#define GPIO_PIN_7            0x0080U
#define GPIO_PIN_8            0x0100U
#define GPIO_PIN_9            0x0200U
#define GPIO_PIN_10           0x0400U
#define GPIO_PIN_11           0x0800U
#define GPIO_PIN_12           0x1000U
#define GPIO_PIN_13           0x2000U
#define GPIO_PIN_14           0x4000U
#define GPIO_PIN_15           0x8000U
#define GPIO_PIN_ALL          0xFFFFU

#define GPIO_SPEED_LOW        0x0U
#define GPIO_SPEED_MEDIUM     0x1U
#define GPIO_SPEED_HIGH       0x2U
#define GPIO_SPEED_VERY_HIGH  0x3U

typedef enum {
    GPIO_MODE_INPUT          = 0x0,
    GPIO_MODE_OUTPUT         = 0x1,
    GPIO_MODE_ALTFUNC       = 0x2,
    GPIO_MODE_ANALOG        = 0x3
} gpio_mode_t;

typedef enum {
    GPIO_OTYPE_PUSH_PULL    = 0x0,
    GPIO_OTYPE_OPEN_DRAIN   = 0x1
} gpio_output_type_t;

typedef enum {
    GPIO_PUPD_NONE          = 0x0,
    GPIO_PUPD_UP            = 0x1,
    GPIO_PUPD_DOWN          = 0x2
} gpio_pull_config_t;

typedef enum {
    GPIO_AF0                = 0x0,
    GPIO_AF1                = 0x1,
    GPIO_AF2                = 0x2,
    GPIO_AF3                = 0x3,
    GPIO_AF4                = 0x4,
    GPIO_AF5                = 0x5,
    GPIO_AF6                = 0x6,
    GPIO_AF7                = 0x7,
    GPIO_AF8                = 0x8,
    GPIO_AF9                = 0x9,
    GPIO_AF10               = 0xA,
    GPIO_AF11               = 0xB,
    GPIO_AF12               = 0xC,
    GPIO_AF13               = 0xD,
    GPIO_AF14               = 0xE,
    GPIO_AF15               = 0xF
} gpio_alt_func_t;

typedef struct {
    uint32_t port_base;
    uint16_t pin_mask;
} gpio_pin_t;

/* ── GPIO API ────────────────────────────────────────── */
void gpio_init(gpio_pin_t pin, gpio_mode_t mode, gpio_output_type_t otype,
    gpio_pull_config_t pull, uint32_t speed);
void gpio_set_alt_func(gpio_pin_t pin, gpio_alt_func_t af);
void gpio_write_pin(gpio_pin_t pin, bool value);
void gpio_write_port(uint32_t port_base, uint16_t value);
bool gpio_read_pin(gpio_pin_t pin);
uint16_t gpio_read_port(uint32_t port_base);
void gpio_toggle_pin(gpio_pin_t pin);
void gpio_set_pull(gpio_pin_t pin, gpio_pull_config_t pull);
void gpio_lock_pin(gpio_pin_t pin);

/* ── UART base addresses ─────────────────────────────── */
#define USART1_BASE           0x40011000UL
#define USART2_BASE           0x40004400UL
#define USART3_BASE           0x40004800UL
#define UART4_BASE            0x40004C00UL
#define UART5_BASE            0x40005000UL
#define USART6_BASE           0x40011400UL

#define UART_WORD_LENGTH_8    0x0U
#define UART_WORD_LENGTH_9    0x1U

#define UART_STOP_BITS_1      0x0U
#define UART_STOP_BITS_0_5    0x1U
#define UART_STOP_BITS_2      0x2U
#define UART_STOP_BITS_1_5    0x3U

#define UART_PARITY_NONE      0x0U
#define UART_PARITY_EVEN      0x1U
#define UART_PARITY_ODD       0x2U

#define UART_FLOW_CTRL_NONE   0x0U
#define UART_FLOW_CTRL_RTS    0x1U
#define UART_FLOW_CTRL_CTS    0x2U
#define UART_FLOW_CTRL_RTS_CTS 0x3U

#define UART_OVERSAMPLING_16  0x0U
#define UART_OVERSAMPLING_8   0x1U

/* ── UART interrupt flags ────────────────────────────── */
#define UART_SR_TXE           0x80U
#define UART_SR_TC            0x40U
#define UART_SR_RXNE          0x20U
#define UART_SR_IDLE          0x10U
#define UART_SR_ORE           0x08U
#define UART_SR_NE            0x04U
#define UART_SR_FE            0x02U
#define UART_SR_PE            0x01U

#define UART_CR1_UE           0x2000U
#define UART_CR1_TE           0x0008U
#define UART_CR1_RE           0x0004U
#define UART_CR1_RXNEIE       0x0020U
#define UART_CR1_TXEIE        0x0080U
#define UART_CR1_TCIE         0x0040U
#define UART_CR1_IDLEIE       0x0010U
#define UART_CR1_PEIE         0x0100U

typedef struct {
    uint32_t uart_base;
    uint32_t baud_rate;
    uint8_t word_length;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t flow_ctrl;
    uint8_t oversampling;
} uart_config_t;

/* ── UART API ────────────────────────────────────────── */
void uart_init(const uart_config_t *config);
void uart_deinit(uint32_t uart_base);
void uart_transmit(uint32_t uart_base, const uint8_t *data, uint32_t size);
void uart_transmit_byte(uint32_t uart_base, uint8_t byte);
uint32_t uart_receive(uint32_t uart_base, uint8_t *buffer, uint32_t max_size);
uint8_t uart_receive_byte(uint32_t uart_base);
void uart_enable_interrupt(uint32_t uart_base, uint16_t flags);
void uart_disable_interrupt(uint32_t uart_base, uint16_t flags);
uint32_t uart_get_status(uint32_t uart_base);
void uart_send_string(uint32_t uart_base, const char *str);
uint32_t uart_receive_line(uint32_t uart_base, char *buffer, uint32_t max_size);

/* ── SPI base addresses ──────────────────────────────── */
#define SPI1_BASE             0x40013000UL
#define SPI2_BASE             0x40003800UL
#define SPI3_BASE             0x40003C00UL
#define SPI4_BASE             0x40013400UL

#define SPI_MODE_MASTER       0x1U
#define SPI_MODE_SLAVE        0x0U

#define SPI_DIRECTION_2LINES  0x0U
#define SPI_DIRECTION_1LINE_TX 0x8000U
#define SPI_DIRECTION_1LINE_RX 0xC000U

#define SPI_DATA_SIZE_8BIT    0x7U
#define SPI_DATA_SIZE_16BIT   0xFU

#define SPI_CPOL_LOW          0x0U
#define SPI_CPOL_HIGH         0x1U
#define SPI_CPHA_1EDGE        0x0U
#define SPI_CPHA_2EDGE        0x1U

#define SPI_BAUD_PRESCALER_2   0x0U
#define SPI_BAUD_PRESCALER_4   0x1U
#define SPI_BAUD_PRESCALER_8   0x2U
#define SPI_BAUD_PRESCALER_16  0x3U
#define SPI_BAUD_PRESCALER_32  0x4U
#define SPI_BAUD_PRESCALER_64  0x5U
#define SPI_BAUD_PRESCALER_128 0x6U
#define SPI_BAUD_PRESCALER_256 0x7U

#define SPI_FRAME_MSB_FIRST    0x0U
#define SPI_FRAME_LSB_FIRST    0x1U

#define SPI_SR_RXNE           0x01U
#define SPI_SR_TXE            0x02U
#define SPI_SR_BSY            0x80U

/* ── SPI pin mapping ─────────────────────────────────── */
typedef struct {
    gpio_pin_t mosi;
    gpio_pin_t miso;
    gpio_pin_t sck;
    gpio_pin_t nss;
} spi_pins_t;

typedef struct {
    uint32_t spi_base;
    uint8_t mode;
    uint8_t direction;
    uint8_t data_size;
    uint8_t cpol;
    uint8_t cpha;
    uint8_t baud_prescaler;
    uint8_t first_bit;
    bool crc_enable;
} spi_config_t;

/* ── SPI API ─────────────────────────────────────────── */
void spi_init(const spi_config_t *config);
void spi_deinit(uint32_t spi_base);
uint16_t spi_transmit_receive_16(uint32_t spi_base, uint16_t data);
uint8_t spi_transmit_receive_8(uint32_t spi_base, uint8_t data);
void spi_transmit_16(uint32_t spi_base, uint16_t data);
void spi_transmit_8(uint32_t spi_base, uint8_t data);
uint16_t spi_receive_16(uint32_t spi_base);
uint8_t spi_receive_8(uint32_t spi_base);
void spi_enable_interrupt(uint32_t spi_base, uint16_t flags);
void spi_disable_interrupt(uint32_t spi_base, uint16_t flags);
bool spi_is_busy(uint32_t spi_base);

/* ── I2C base addresses ──────────────────────────────── */
#define I2C1_BASE             0x40005400UL
#define I2C2_BASE             0x40005800UL
#define I2C3_BASE             0x40005C00UL

#define I2C_DUTY_CYCLE_2      0x0U
#define I2C_DUTY_CYCLE_16_9   0x1U

#define I2C_ACK_ENABLE        0x1U
#define I2C_ACK_DISABLE       0x0U

#define I2C_ADDR_MODE_7BIT    0x0U
#define I2C_ADDR_MODE_10BIT   0x1U

#define I2C_SR_SB             0x0001U
#define I2C_SR_ADDR           0x0002U
#define I2C_SR_BTF            0x0004U
#define I2C_SR_TXE            0x0080U
#define I2C_SR_RXNE           0x0040U
#define I2C_SR_STOPF          0x0010U
#define I2C_SR_AF             0x0400U

#define I2C_CR1_PE            0x0001U
#define I2C_CR1_START         0x0100U
#define I2C_CR1_STOP          0x0200U
#define I2C_CR1_ACK           0x0400U
#define I2C_CR1_POS           0x0800U

typedef struct {
    uint32_t i2c_base;
    uint32_t clock_speed;
    uint8_t duty_cycle;
    uint8_t own_address;
    uint8_t addr_mode;
    bool general_call;
    bool no_stretch;
} i2c_config_t;

/* ── I2C API ─────────────────────────────────────────── */
void i2c_init(const i2c_config_t *config);
void i2c_deinit(uint32_t i2c_base);
void i2c_generate_start(uint32_t i2c_base);
void i2c_generate_stop(uint32_t i2c_base);
void i2c_send_address(uint32_t i2c_base, uint8_t address, bool is_read);
bool i2c_send_data(uint32_t i2c_base, uint8_t data);
uint8_t i2c_receive_data(uint32_t i2c_base, bool ack);
bool i2c_master_transmit(uint32_t i2c_base, uint8_t dev_addr,
    const uint8_t *data, uint32_t size);
bool i2c_master_receive(uint32_t i2c_base, uint8_t dev_addr,
    uint8_t *buffer, uint32_t size);
bool i2c_master_write_read(uint32_t i2c_base, uint8_t dev_addr,
    uint8_t reg_addr, const uint8_t *tx_data, uint32_t tx_size,
    uint8_t *rx_buffer, uint32_t rx_size);
bool i2c_is_device_ready(uint32_t i2c_base, uint8_t dev_addr,
    uint32_t retries);

#endif /* GPIO_UART_H */
