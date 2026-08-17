#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <cmsis_core.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/*DEVICE TREE ALIASLARI*/
#define UART_NODE DT_ALIAS(sensor_uart)
#define ADC_NODE  DT_ALIAS(lm35_adc)
#define I2C_NODE  DT_ALIAS(lcd_i2c)

static const struct device *uart_dev =
    DEVICE_DT_GET(UART_NODE);

static const struct device *adc_dev =
    DEVICE_DT_GET(ADC_NODE);

static const struct device *i2c_dev =
    DEVICE_DT_GET(I2C_NODE);

/*ADC AYARLARI*/
#define ADC_CHANNEL 1
static const struct adc_channel_cfg adc_cfg = {
    .gain             = ADC_GAIN_1,
    .reference        = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id       = ADC_CHANNEL,
    .differential     = 0
};
/*LCD AYARLARI*/
#define LCD_I2C_ADDR  0x27
#define LCD_BACKLIGHT 0x08
#define EN            0x04
#define RS            0x01

static volatile bool data_received = false;

static unsigned char rx_char = 0;

static void uart_cb(const struct device *dev, void *user_data)
{
    /* UART interrupt durumunu güncelle */
    uart_irq_update(dev);

    /* UART'tan veri geldi mi? */
    if (uart_irq_rx_ready(dev)) {
        int recv = uart_fifo_read(
            dev,
            &rx_char,
            1
        );
        if (recv > 0) {
        
            if (rx_char == 'W' || rx_char == 'w') {

                data_received = true;
            }
        }
    }
}
void lcd_send_nibble(uint8_t data)
{
    uint8_t tx_data;
    tx_data = data | LCD_BACKLIGHT;

    i2c_write(
        i2c_dev,
        &tx_data,
        1,
        LCD_I2C_ADDR
    );

    tx_data = data | EN | LCD_BACKLIGHT;

    i2c_write(
        i2c_dev,
        &tx_data,
        1,
        LCD_I2C_ADDR
    );

    k_busy_wait(1000);

    tx_data = (data & ~EN) | LCD_BACKLIGHT;

    i2c_write(
        i2c_dev,
        &tx_data,
        1,
        LCD_I2C_ADDR
    );

    k_busy_wait(100);
}

void lcd_send_cmd(uint8_t cmd)
{
    lcd_send_nibble(
        cmd & 0xF0
    );

    lcd_send_nibble(
        (cmd << 4) & 0xF0
    );
}
void lcd_send_data(uint8_t data)
{
    lcd_send_nibble(
        (data & 0xF0) | RS
    );

    lcd_send_nibble(
        ((data << 4) & 0xF0) | RS
    );
}

void lcd_init(void)
{
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
void lcd_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t offsets[] = {
        0x00,
        0x40
    };
    lcd_send_cmd(
        0x80 + col + offsets[row]
    );
}
void lcd_print(const char *str)
{
    while (*str) {

        lcd_send_data(*str);

        str++;
    }
}
void lcd_clear(void)
{
    lcd_send_cmd(0x01);

    k_msleep(2);
}
int main(void)
{
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
    if (!device_is_ready(uart_dev)) {
        printk("UART hazir degil!\n");
        return 0;
    }
    if (!device_is_ready(adc_dev)) {
        printk("ADC hazir degil!\n");
        return 0;
    }
    if (!device_is_ready(i2c_dev)) {
        printk("I2C hazir degil!\n");
        return 0;
    }
    err = adc_channel_setup(
        adc_dev,
        &adc_cfg
    );
    if (err < 0) {
        printk(
            "ADC channel setup hatasi: %d\n",
            err
        );
        return 0;
    }
    lcd_init();
    uart_irq_callback_set(
        uart_dev,
        uart_cb
    );
    uart_irq_rx_enable(uart_dev);
    printk("\n");
    printk("==============================\n");
    printk("STM32F446RE BASLADI\n");
    printk("UART W bekleniyor...\n");
    printk("==============================\n");

    while (1) {
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_print("W Bekleniyor");
        data_received = false;
        printk("SLEEP MODE'A GIRILIYOR...\n");

        while (!data_received) {

            __WFI();
        }
        printk("UART W GELDI!\n");
        printk("CPU SLEEP MODE'DAN UYANDI.\n");
        err = adc_read(
            adc_dev,
            &sequence
        );
        if (err == 0) {
            int16_t ham_deger =
                sample_buffer[0];
            int32_t mv_value =
                ham_deger;
            adc_raw_to_millivolts(
                3300,
                ADC_GAIN_1,
                12,
                &mv_value
            );
            int temperature =
                (int)(mv_value / 10);

            sprintf(
                uart_buf,
                "SICAKLIK:%d\r\n",
                temperature
            );
            for (int i = 0;
                 i < strlen(uart_buf);
                 i++) {

                uart_poll_out(
                    uart_dev,
                    uart_buf[i]
                );
            }
            sprintf(
                lcd_buf,
                "Ham:%d T:%dC",
                ham_deger,
                temperature
            );


            lcd_clear();
            lcd_set_cursor(0, 0);
            lcd_print(lcd_buf);
            printk(
                "HAM ADC: %d | VOLTAJ: %d mV | SICAKLIK: %d C\n",
                ham_deger,
                mv_value,
                temperature
            );
        }
        else {
            lcd_clear();
            lcd_set_cursor(0, 0);
            lcd_print("ADC Hata!");
            printk(
                "ADC okuma hatasi: %d\n",
                err
            );
        }
        k_msleep(5000);
    }
    return 0;
}