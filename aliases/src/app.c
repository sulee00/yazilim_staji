#include "app.h"
#include "wifi_mgr.h"
#include "mqtt_mgr.h"
#include "lcd.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static app_config_t app_cfg;
static bool config_set = false;
static bool trigger_mqtt = false;

/* Cihaz Tanımları */
static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc1));
static const struct device *bmp_dev = DEVICE_DT_GET(DT_NODELABEL(bmp280));
static const struct device *gpioa_dev = DEVICE_DT_GET(DT_NODELABEL(gpioa));
static const struct device *gpiob_dev = DEVICE_DT_GET(DT_NODELABEL(gpiob));

/* Röle Pinleri */
#define RELAY1_PIN 4  /* PB4 */
#define RELAY2_PIN 5  /* PB5 */
#define RELAY3_PIN 3  /* PB3 */
#define RELAY4_PIN 10 /* PA10 */

/* Röle Durum Kontrolü (state: 1 = Açık, 0 = Kapalı) */
void set_relay(int relay_no, int state)
{
    int pin_val = (state == 1) ? 0 : 1; // Active-LOW: 0V tetikler, 3.3V kapatır

    switch (relay_no) {
        case 1:
            if (device_is_ready(gpiob_dev)) gpio_pin_set(gpiob_dev, RELAY1_PIN, pin_val);
            break;
        case 2:
            if (device_is_ready(gpiob_dev)) gpio_pin_set(gpiob_dev, RELAY2_PIN, pin_val);
            break;
        case 3:
            if (device_is_ready(gpiob_dev)) gpio_pin_set(gpiob_dev, RELAY3_PIN, pin_val);
            break;
        case 4:
            if (device_is_ready(gpioa_dev)) gpio_pin_set(gpioa_dev, RELAY4_PIN, pin_val);
            break;
        default:
            break;
    }
}

/* Flutter'dan Gelen Röle Komutunu Çözümleme */
static void on_mqtt_command(const char *topic, const char *payload)
{
    char *r_ptr = strstr(payload, "\"relay\":");
    char *s_ptr = strstr(payload, "\"state\":");

    if (r_ptr && s_ptr) {
        int r_num = atoi(r_ptr + 8);
        int r_st = atoi(s_ptr + 8);
        set_relay(r_num, r_st);
        LOG_INF("Röle %d durumu: %d yapildi.", r_num, r_st);
    }
}

static void on_ip_ready(const char *ip_addr)
{
    trigger_mqtt = true;
}

int app_set_config(const app_config_t *config)
{
    if (!config) return -EINVAL;
    app_cfg = *config;
    config_set = true;
    return 0;
}

int app_init(void)
{
    if (!config_set) return -EPERM;

    /* LCD Başlatma */
    if (app_cfg.i2c_dev && device_is_ready(app_cfg.i2c_dev)) {
        lcd_init(app_cfg.i2c_dev);
        lcd_clear(app_cfg.i2c_dev);
        lcd_set_cursor(app_cfg.i2c_dev, 0, 0);
        lcd_print(app_cfg.i2c_dev, "Akilli Tarim");
        lcd_set_cursor(app_cfg.i2c_dev, 1, 0);
        lcd_print(app_cfg.i2c_dev, "Sistem Basliyor");
    } else {
        LOG_WRN("LCD I2C cihazi hazir degil veya atanmadi.");
    }

    /* 4'lü Röle GPIO Kurulumu */
    if (device_is_ready(gpiob_dev)) {
        gpio_pin_configure(gpiob_dev, RELAY1_PIN, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure(gpiob_dev, RELAY2_PIN, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure(gpiob_dev, RELAY3_PIN, GPIO_OUTPUT_INACTIVE);
    }
    if (device_is_ready(gpioa_dev)) {
        gpio_pin_configure(gpioa_dev, RELAY4_PIN, GPIO_OUTPUT_INACTIVE);
    }

    /* Başlangıçta tüm röleleri kapat */
    set_relay(1, 0);
    set_relay(2, 0);
    set_relay(3, 0);
    set_relay(4, 0);

    /* ADC Kurulumu: Kanal 1 (PA1/A1) ve Kanal 4 (PA4/A2) */
    if (device_is_ready(adc_dev)) {
        struct adc_channel_cfg ch1_cfg = {
            .gain = ADC_GAIN_1,
            .reference = ADC_REF_INTERNAL,
            .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 480),
            .channel_id = 1, /* PA1 -> A1 (LM35) */
        };
        struct adc_channel_cfg ch4_cfg = {
            .gain = ADC_GAIN_1,
            .reference = ADC_REF_INTERNAL,
            .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 480),
            .channel_id = 4, /* PA4 -> A2 (Toprak Nemi) */
        };
        adc_channel_setup(adc_dev, &ch1_cfg);
        adc_channel_setup(adc_dev, &ch4_cfg);
    } else {
        LOG_ERR("ADC1 hazir degil!");
    }

    /* BMP280 */
    if (!device_is_ready(bmp_dev)) {
        LOG_WRN("BMP280 hazir degil!");
    }

    wifi_mgr_init(on_ip_ready);

    /* MQTT Yapılandırması */
    mqtt_mgr_config_t m_cfg = {
        .broker_ip = app_cfg.mqtt_broker_ip,
        .broker_port = app_cfg.mqtt_broker_port,
        .client_id = app_cfg.mqtt_client_id,
        .subscribe_topic = "tarim/role/kontrol",
        .on_message = on_mqtt_command,
    };
    mqtt_mgr_init(&m_cfg);

    return 0;
}

/* ADC Ortalama Alan Filtreli Okuma */
static int read_adc_raw_avg(uint8_t channel, int sample_count)
{
    int32_t sum = 0;
    int16_t sample_buffer;
    struct adc_sequence seq = {
        .channels = BIT(channel),
        .buffer = &sample_buffer,
        .buffer_size = sizeof(sample_buffer),
        .resolution = 12,
    };

    for (int i = 0; i < sample_count; i++) {
        if (adc_read(adc_dev, &seq) == 0) {
            sum += sample_buffer;
        }
        k_busy_wait(100);
    }
    return (int)(sum / sample_count);
}

void app_start_and_wait(void)
{
    char payload[128];
    uint32_t elapsed_time = 0;
    uint32_t lcd_elapsed_time = 0;

    while (1) {
        if (!wifi_mgr_is_connected()) {
            wifi_mgr_connect(app_cfg.wifi_ssid, app_cfg.wifi_password);
            k_sleep(K_MSEC(2000));
            continue;
        }

        if (trigger_mqtt && !mqtt_mgr_is_connected()) {
            mqtt_mgr_connect();
        }

        mqtt_mgr_process();

        /* --- SENSÖR OKUMALARI --- */
        /* LM35 -> Kanal 1 (A1 pini) */
        int raw_lm35 = read_adc_raw_avg(1, 10);
        float mv_lm35 = ((float)raw_lm35 * 3300.0f) / 4095.0f;
        float lm35_temp = mv_lm35 / 10.0f;

        /* Toprak Nemi -> Kanal 4 (A2 pini) */
        int raw_moist = read_adc_raw_avg(4, 5);
        int moist_percent = 100 - ((raw_moist * 100) / 4095);
        if (moist_percent < 0) moist_percent = 0;
        if (moist_percent > 100) moist_percent = 100;

        /* BMP280 */
        double bmp_temp = 0.0;
        double bmp_press = 0.0;
        if (device_is_ready(bmp_dev)) {
            struct sensor_value val;
            if (sensor_sample_fetch(bmp_dev) == 0) {
                sensor_channel_get(bmp_dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
                bmp_temp = sensor_value_to_double(&val);

                sensor_channel_get(bmp_dev, SENSOR_CHAN_PRESS, &val);
                bmp_press = sensor_value_to_double(&val) * 10.0;
            }
        }

        /* --- MQTT YAYINI --- */
        if (mqtt_mgr_is_connected()) {
            elapsed_time += 100;
            if (elapsed_time >= app_cfg.publish_interval_ms) {
                snprintf(payload, sizeof(payload),
                         "{\"lm35\":%.1f,\"nem\":%d,\"bmp_t\":%.1f,\"bmp_p\":%.1f}",
                         (double)lm35_temp, moist_percent, bmp_temp, bmp_press);
                mqtt_mgr_publish(app_cfg.mqtt_publish_topic, payload);
                elapsed_time = 0;
            }
        }

        /* --- LCD EKRAN GÜNCELLEME (2 Saniyede Bir Sırayla Değişir) --- */
        if (app_cfg.i2c_dev && device_is_ready(app_cfg.i2c_dev)) {
            lcd_elapsed_time += 100;
            if (lcd_elapsed_time >= 2000) {
                lcd_display_sensors_cycle(app_cfg.i2c_dev, lm35_temp, (float)bmp_temp, moist_percent, (float)bmp_press);
                lcd_elapsed_time = 0;
            }
        }

        k_sleep(K_MSEC(100));
    }
}