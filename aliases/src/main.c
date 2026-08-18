/*Run Mode: Mikrodenetleyici normal şekilde çalışır. CPU ve çevre birimleri aktiftir. Güç tüketimi en yüksek moddur.
Sleep Mode: CPU durur, ancak RAM ve birçok çevre birimi çalışmaya devam eder. Interrupt geldiğinde çok hızlı uyanır. → UART bekleme için uygun.
Stop Mode: CPU ve çoğu clock durur. RAM korunur ama çevre birimlerinin çoğu kapanır. Sleep'ten daha az güç tüketir, uyanması biraz daha farklıdır.
Standby Mode: Sistemin büyük kısmı kapanır. En düşük güç tüketimine yakın moddur. Uyanınca sistem genellikle resetlenmiş gibi yeniden başlar.
*/

/*Stop Modu kullanıyoruz. 
PM_STATE_SUSPEND_TO_RAM çağrısı yaparak Standard Stop Mode 
(Ana regülatör devrede, RAM ve Register verileri korunmuş, çekirdek ve çevre birim osilatörleri durmuş)*/
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"

/*DEVICE TREE*/
#define SW0_NODE   DT_ALIAS(sw0)
#define ADC_NODE   DT_ALIAS(lm35_adc)
#define I2C_NODE   DT_ALIAS(lcd_i2c)
#define UART_NODE  DT_ALIAS(sensor_uart)

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct device *adc_dev     = DEVICE_DT_GET(ADC_NODE);
static const struct device *i2c_dev     = DEVICE_DT_GET(I2C_NODE);
static const struct device *uart_dev    = DEVICE_DT_GET(UART_NODE);

/*ADC*/
#define ADC_CHANNEL 1

static const struct adc_channel_cfg adc_cfg = {
    .gain             = ADC_GAIN_1,
    .reference        = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id       = ADC_CHANNEL,
    .differential     = 0
};
static struct gpio_callback button_cb_data;
static volatile bool wake_up_event = false;

/* Butona basıldığında CPU uyanır ve bu callback çalışır */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    wake_up_event = true;
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

    if (!gpio_is_ready_dt(&button)) {
        printk("Buton (sw0) hazir degil!\n");
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
    if (!device_is_ready(uart_dev)) {
        printk("UART hazir degil!\n");
        return 0;
    }

    /* Buton GPIO Yapılandırması */
    err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (err < 0) {
        printk("Buton pin ayari hatasi: %d\n", err);
        return 0;
    }
    err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (err < 0) {
        printk("Buton kesme ayari hatasi: %d\n", err);
        return 0;
    }
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    err = adc_channel_setup(adc_dev, &adc_cfg);
    if (err < 0) {
        printk("ADC channel setup hatasi: %d\n", err);
        return 0;
    }

    /* LCD Başlatma */
    lcd_init(i2c_dev);

    printk("\n=\n");
    printk("SISTEM BASLATILDI (STOP MODU PROJESI)\n");
    printk("Uyanmak icin butona basiniz.\n");
    printk("=\n");

    while (1) {
        lcd_clear(i2c_dev);
        lcd_set_cursor(i2c_dev, 0, 0);
        lcd_print(i2c_dev, "STOP MODU...");

        printk("STOP (SUSPEND TO RAM) MODUNA GIRILIYOR...\n");
        wake_up_event = false;

        /*Stop Modu */
        pm_state_force(0, &(struct pm_state_info){PM_STATE_SUSPEND_TO_RAM, 0, 0});

        /* Buton kesmesi gelene kadar bekle */
        while (!wake_up_event) {
            k_msleep(10);
        }
        printk("UYANDI! Buton kesmesi algilandi.\n");
        /* Sıcaklık Ölçümü ve Ekrana Basma */
        err = adc_read(adc_dev, &sequence);
        if (err == 0) {
            int16_t ham_deger = sample_buffer[0];
            int32_t mv_value = ham_deger;
            adc_raw_to_millivolts(3300, ADC_GAIN_1, 12, &mv_value);
            int temperature = (int)(mv_value / 10);

            sprintf(uart_buf, "SICAKLIK:%d\r\n", temperature);
            for (int i = 0; i < strlen(uart_buf); i++) {
                uart_poll_out(uart_dev, uart_buf[i]);
            }
            sprintf(lcd_buf, "Ham:%d T:%dC", ham_deger, temperature);
            lcd_clear(i2c_dev);
            lcd_set_cursor(i2c_dev, 0, 0);
            lcd_print(i2c_dev, lcd_buf);
            printk("HAM ADC: %d | VOLTAJ: %d mV | SICAKLIK: %d C\n",
                   ham_deger, mv_value, temperature);
        } else {
            lcd_clear(i2c_dev);
            lcd_set_cursor(i2c_dev, 0, 0);
            lcd_print(i2c_dev, "ADC Hata!");
            printk("ADC okuma hatasi: %d\n", err);
        }

        /* 5 saniye  */
        k_msleep(5000);
    }

    return 0;
}