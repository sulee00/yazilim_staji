#ifndef LCD_H_
#define LCD_H_

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#define LCD_I2C_ADDR 0x27 /* veya sendeki adres: 0x3F */

void lcd_init(const struct device *i2c_dev);
void lcd_send_cmd(const struct device *i2c_dev, uint8_t cmd);
void lcd_send_data(const struct device *i2c_dev, uint8_t data);
void lcd_clear(const struct device *i2c_dev);
void lcd_set_cursor(const struct device *i2c_dev, uint8_t row, uint8_t col);
void lcd_print(const struct device *i2c_dev, const char *str);

/* Sırayla sayfa döndüren fonksiyon */
void lcd_display_sensors_cycle(const struct device *i2c_dev, float lm35_temp, float bmp_temp, int soil_moist, float pressure);

#endif