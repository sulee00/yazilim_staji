#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <stdbool.h>

/* DeviceTree tanımları */
#define SENSOR_NODE DT_ALIAS(sensor_uart)
#define LED0_NODE   DT_ALIAS(led0) // Lojik analizör D2 kanalı için debug pini

static const struct device *sensor_uart = DEVICE_DT_GET(SENSOR_NODE);
static const struct gpio_dt_spec debug_pin = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Buffer ve Bayrak Yapısı */
#define BUFFER_SIZE 64
static uint8_t rx_buffer[BUFFER_SIZE];
static volatile int rx_index = 0;
static volatile bool paket_hazir = false;


void sensor_uart_interrupt_handler(const struct device *dev, void *user_data)
{
    /* ANALİZÖR D2: Kesmeye girildi -> PIN HIGH */
    gpio_pin_set_dt(&debug_pin, 1);

    uint8_t c;

    if (!uart_irq_update(dev)) {
        gpio_pin_set_dt(&debug_pin, 0);
        return;
    }

    if (uart_irq_rx_ready(dev)) {
        while (uart_fifo_read(dev, &c, 1) == 1) {
            if (c == '\n' || c == '\r') {
                if (rx_index > 0) { // Boş paketleri engelle
                    rx_buffer[rx_index] = '\0'; 
                    paket_hazir = true;         
                    rx_index = 0;              
                }
            } 
            else if (rx_index < BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = c;  
            }
        }
    }

    /* ANALİZÖR D2: Kesme bitti -> PIN LOW */
    gpio_pin_set_dt(&debug_pin, 0);
}

int main(void)
{
    printk("STM32 Zephyr UART Kesme Projesi Baslatiliyor...\n");
    if (!device_is_ready(sensor_uart)) {
        printk("HATA: sensor_uart cihazı hazır degil!\n");
        return 0;
    }

    if (!gpio_is_ready_dt(&debug_pin)) {
        printk("HATA: Debug GPIO hazır degil!\n");
        return 0;
    }
    gpio_pin_configure_dt(&debug_pin, GPIO_OUTPUT_INACTIVE);

    uart_irq_callback_user_data_set(sensor_uart, sensor_uart_interrupt_handler, NULL);
    uart_irq_rx_enable(sensor_uart);

    printk("Sistem hazır. Leonardo'dan veri bekleniyor...\n");

    while (1) {
        if (paket_hazir) {
            printk("GELEN PAKET: %s\n", rx_buffer);
            paket_hazir = false;
        }

        k_msleep(10); 
    }

    return 0;
}