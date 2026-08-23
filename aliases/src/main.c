#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include <string.h>
#include "lcd.h"

#define ADC_NODE   DT_ALIAS(lm35_adc)
#define I2C_NODE   DT_ALIAS(lcd_i2c)
#define UART_NODE  DT_ALIAS(sensor_uart)

#define SLEEP_TIME_SECONDS 3

static const struct device *adc_dev  = DEVICE_DT_GET(ADC_NODE);
static const struct device *i2c_dev  = DEVICE_DT_GET(I2C_NODE);
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

#define ADC_CHANNEL 1
static const struct adc_channel_cfg adc_cfg = {
    .gain             = ADC_GAIN_1,
    .reference        = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id       = ADC_CHANNEL,
    .differential     = 0
};

void send_uart_string(const struct device *dev, const char *str)
{
    for (size_t i = 0; i < strlen(str); i++) {
        uart_poll_out(dev, str[i]);
    }
}

int main(void)
{
    int err;
    int16_t sample_buffer[1];
    char send_buf[64];
    char line_buf[17];
    int sayac = 0;

    struct adc_sequence sequence = {
        .channels    = BIT(ADC_CHANNEL),
        .buffer      = sample_buffer,
        .buffer_size = sizeof(sample_buffer),
        .resolution  = 12,
    };

    k_msleep(1000);
   
    printk("[STM32] Ana Dongu Baslatiliyor...\n");
    
    if (!device_is_ready(adc_dev)) {
        printk("[HATA] ADC aygiti hazir degil!\n");
    }
    if (!device_is_ready(uart_dev)) {
        printk("[HATA] UART aygiti hazir degil!\n");
    }

    adc_channel_setup(adc_dev, &adc_cfg);

    if (device_is_ready(i2c_dev)) {
        printk("[BILGI] LCD baslatiliyor...\n");
        lcd_init(i2c_dev);
        lcd_clear(i2c_dev);
        lcd_set_cursor(i2c_dev, 0, 0);
        lcd_print(i2c_dev, "Sistem Basladi  ");
        printk("[BILGI] LCD baslatildi.\n");
    } else {
        printk("[HATA] I2C cihazi bulunamadi!\n");
    }

    while (1) {
        sayac++;

        /* 1. Sıcaklık Oku */
        err = adc_read(adc_dev, &sequence);
        int temp_c = 0;
        if (err == 0) {
            int16_t raw_val = sample_buffer[0];
            int32_t mv_val = raw_val;
            adc_raw_to_millivolts(3300, ADC_GAIN_1, 12, &mv_val);
            temp_c = (int)(mv_val / 10);
        } else {
            printk("[HATA] ADC okuma hatasi: %d\n", err);
        }

        /* 2. ESP32'ye UART4 ile veri gonder */
        snprintf(send_buf, sizeof(send_buf), "SICAKLIK:%d (Paket:%d)\n", temp_c, sayac);
        send_uart_string(uart_dev, send_buf);

        /* 3. Konsola printk ile bas */
        printk("[STM32 -> ESP32 Gonderildi]: %s", send_buf);

        /* 4. LCD Ekranına Yaz */
        if (device_is_ready(i2c_dev)) {
            snprintf(line_buf, sizeof(line_buf), "Sicaklik: %2d C ", temp_c);
            lcd_set_cursor(i2c_dev, 0, 0);
            lcd_print(i2c_dev, line_buf);

            lcd_set_cursor(i2c_dev, 1, 0);
            lcd_print(i2c_dev, "Durum: AKTIF    ");
        }

        k_msleep(1500);

        if (device_is_ready(i2c_dev)) {
            lcd_set_cursor(i2c_dev, 1, 0);
            lcd_print(i2c_dev, "Durum: UYKUDA   ");
        }

        printk("[STM32] Uykuya gecildi (%d sn)...\n", SLEEP_TIME_SECONDS);
        k_sleep(K_SECONDS(SLEEP_TIME_SECONDS));
    }

    return 0;
}