#ifndef LCD_H_
#define LCD_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdint.h>

#define LCD_I2C_ADDR  0x27
#define LCD_BACKLIGHT 0x08
#define EN            0x04
#define RS            0x01

void lcd_init(const struct device *i2c_dev);
void lcd_send_cmd(const struct device *i2c_dev, uint8_t cmd);
void lcd_send_data(const struct device *i2c_dev, uint8_t data);
void lcd_send_nibble(const struct device *i2c_dev, uint8_t data);
void lcd_set_cursor(const struct device *i2c_dev, uint8_t row, uint8_t col);
void lcd_print(const struct device *i2c_dev, const char *str);
void lcd_clear(const struct device *i2c_dev);

#endif /* LCD_H_ */