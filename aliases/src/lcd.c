#include "lcd.h"
#include <zephyr/kernel.h>

#define LCD_BACKLIGHT 0x08
#define ENABLE        0x04

static void lcd_write_nibble(const struct device *i2c_dev, uint8_t nibble, uint8_t mode)
{
    uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;
    uint8_t buf[2];

    buf[0] = data | ENABLE;
    buf[1] = data & ~ENABLE;

    
    i2c_write(i2c_dev, &buf[0], 1, LCD_I2C_ADDR);
    k_busy_wait(100);
    i2c_write(i2c_dev, &buf[1], 1, LCD_I2C_ADDR);
    k_busy_wait(100);
}

void lcd_send_cmd(const struct device *i2c_dev, uint8_t cmd)
{
    lcd_write_nibble(i2c_dev, cmd & 0xF0, 0);
    lcd_write_nibble(i2c_dev, (cmd << 4) & 0xF0, 0);
    k_msleep(2);
}

void lcd_send_data(const struct device *i2c_dev, uint8_t data)
{
    lcd_write_nibble(i2c_dev, data & 0xF0, 1);
    lcd_write_nibble(i2c_dev, (data << 4) & 0xF0, 1);
    k_busy_wait(200);
}

void lcd_clear(const struct device *i2c_dev)
{
    lcd_send_cmd(i2c_dev, 0x01);
    k_msleep(5);
}

void lcd_set_cursor(const struct device *i2c_dev, uint8_t row, uint8_t col)
{
    uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    lcd_send_cmd(i2c_dev, addr);
    k_msleep(2);
}

void lcd_print(const struct device *i2c_dev, const char *str)
{
    while (*str) {
        lcd_send_data(i2c_dev, (uint8_t)(*str));
        str++;
    }
}

void lcd_init(const struct device *i2c_dev)
{
    k_msleep(100);
    lcd_write_nibble(i2c_dev, 0x30, 0);
    k_msleep(10);
    lcd_write_nibble(i2c_dev, 0x30, 0);
    k_msleep(5);
    lcd_write_nibble(i2c_dev, 0x30, 0);
    k_msleep(5);
    lcd_write_nibble(i2c_dev, 0x20, 0);
    k_msleep(5);

    lcd_send_cmd(i2c_dev, 0x28); 
    k_msleep(2);
    lcd_send_cmd(i2c_dev, 0x0C); 
    k_msleep(2);
    lcd_clear(i2c_dev);
    lcd_send_cmd(i2c_dev, 0x06); 
    k_msleep(2);
}