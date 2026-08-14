#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/sys/printk.h>
#include <soc.h>
#include <stdio.h>
#include <string.h>

/* Device Tree */
#define UART_NODE DT_ALIAS(sensor_uart)
#define ADC_NODE  DT_ALIAS(lm35_adc)
#define I2C_NODE  DT_ALIAS(lcd_i2c)

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);
static const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);
static const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

/* LM35 Sensörü PA1 pininde (ADC Kanal 1) */
#define ADC_CHANNEL 1
static const struct adc_channel_cfg m_1st_channel_cfg = {
    .gain             = ADC_GAIN_1,
    .reference        = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id       = ADC_CHANNEL,
    .differential     = 0
};

/* I2C LCD */
#define LCD_I2C_ADDR 0x27
#define LCD_BACKLIGHT 0x08
#define EN 0x04
#define RS 0x01

void lcd_send_nibble(uint8_t data) {
    uint8_t tx_data = data | LCD_BACKLIGHT;
    i2c_write(i2c_dev, &tx_data, 1, LCD_I2C_ADDR);
    
    tx_data = data | EN | LCD_BACKLIGHT;
    i2c_write(i2c_dev, &tx_data, 1, LCD_I2C_ADDR);
    k_busy_wait(1000); 
    
    tx_data = (data & ~EN) | LCD_BACKLIGHT;
    i2c_write(i2c_dev, &tx_data, 1, LCD_I2C_ADDR);
    k_busy_wait(100);
}

void lcd_send_cmd(uint8_t cmd) {
    lcd_send_nibble(cmd & 0xF0);
    lcd_send_nibble((cmd << 4) & 0xF0);
}

void lcd_send_data(uint8_t data) {
    lcd_send_nibble((data & 0xF0) | RS);
    lcd_send_nibble(((data << 4) & 0xF0) | RS);
}

void lcd_init(void) {
    k_msleep(50);
    lcd_send_nibble(0x30);
    k_msleep(5);
    lcd_send_nibble(0x30);
    k_msleep(1);
    lcd_send_nibble(0x30);
    lcd_send_nibble(0x20); 
    
    lcd_send_cmd(0x28); 
    lcd_send_cmd(0x0C); 
    lcd_send_cmd(0x01); 
    k_msleep(2);
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
    uint8_t offsets[] = {0x00, 0x40};
    lcd_send_cmd(0x80 | (col + offsets[row]));
}

void lcd_print(const char *str) {
    while (*str) {
        lcd_send_data(*str++);
    }
}

void lcd_clear(void) {
    lcd_send_cmd(0x01);
    k_msleep(2);
}

int main(void) {
    int err;
    int16_t sample_buffer[1];
    char uart_buf[50];
    char lcd_buf[32];
    
    struct adc_sequence sequence = {
        .channels    = BIT(ADC_CHANNEL),
        .buffer      = sample_buffer,
        .buffer_size = sizeof(sample_buffer),
        .resolution  = 12,
    };

    if (!device_is_ready(uart_dev) || !device_is_ready(adc_dev) || !device_is_ready(i2c_dev)) {
        return 0;
    }

    adc_channel_setup(adc_dev, &m_1st_channel_cfg);
    lcd_init();

    /* 1. Sensörden Sıcaklığı Oku */
    err = adc_read(adc_dev, &sequence);
    if (err == 0) {
        int32_t mv_value = sample_buffer[0];
        int32_t adc_vref = 3300; 
        
        adc_raw_to_millivolts(adc_vref, ADC_GAIN_1, 12, &mv_value);
        int temperature = (int)(mv_value / 10);

        /* 2. UART ile Leonardo'ya Gönder */
        sprintf(uart_buf, "SICAKLIK:%d\r\n", temperature);
        for (int i = 0; i < strlen(uart_buf); i++) {
            uart_poll_out(uart_dev, uart_buf[i]);
        }
        
        /* 3. LCD Ekrana Yazdır */
        sprintf(lcd_buf, "Sicaklik: %d C ", temperature);
        lcd_set_cursor(0, 0); 
        lcd_print(lcd_buf);
    }

    /*  5 saniye */
    k_msleep(5000); 

    
    lcd_clear();


    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= (PWR_CR_CWUF | PWR_CR_CSBF);
    
    PWR->CSR |= PWR_CSR_EWUP1;



    pm_state_force(0, &(struct pm_state_info){.state = PM_STATE_SOFT_OFF});

    return 0;
}