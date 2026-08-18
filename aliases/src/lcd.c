#include "lcd.h"

void lcd_send_nibble(const struct device *i2c_dev, uint8_t data)
{
    uint8_t tx_data;

    tx_data = data | LCD_BACKLIGHT;
    i2c_write(i2c_dev, &tx_data, 1, LCD_I2C_ADDR);

    tx_data = data | EN | LCD_BACKLIGHT;
    i2c_write(i2c_dev, &tx_data, 1, LCD_I2C_ADDR);
    k_busy_wait(1000);

    tx_data = (data & ~EN) | LCD_BACKLIGHT;
    i2c_write(i2c_dev, &tx_data, 1, LCD_I2C_ADDR);
    k_busy_wait(100);
}

void lcd_send_cmd(const struct device *i2c_dev, uint8_t cmd)
{
    lcd_send_nibble(i2c_dev, cmd & 0xF0);
    lcd_send_nibble(i2c_dev, (cmd << 4) & 0xF0);
}

void lcd_send_data(const struct device *i2c_dev, uint8_t data)
{
    lcd_send_nibble(i2c_dev, (data & 0xF0) | RS);
    lcd_send_nibble(i2c_dev, ((data << 4) & 0xF0) | RS);
}

void lcd_init(const struct device *i2c_dev)
{
    k_msleep(50);
    lcd_send_nibble(i2c_dev, 0x30);
    k_msleep(5);
    lcd_send_nibble(i2c_dev, 0x30);
    k_msleep(1);
    lcd_send_nibble(i2c_dev, 0x30);
    lcd_send_nibble(i2c_dev, 0x20);

    lcd_send_cmd(i2c_dev, 0x28);
    lcd_send_cmd(i2c_dev, 0x0C);
    lcd_send_cmd(i2c_dev, 0x01);
    k_msleep(2);
}

void lcd_set_cursor(const struct device *i2c_dev, uint8_t row, uint8_t col)
{
    uint8_t offsets[] = {0x00, 0x40};
    lcd_send_cmd(i2c_dev, 0x80 + col + offsets[row]);
}

void lcd_print(const struct device *i2c_dev, const char *str)
{
    while (*str) {
        lcd_send_data(i2c_dev, *str);
        str++;
    }
}

void lcd_clear(const struct device *i2c_dev)
{
    lcd_send_cmd(i2c_dev, 0x01);
    k_msleep(2);
}