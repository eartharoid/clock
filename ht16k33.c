#include "ht16k33.h"
#include "hardware/i2c.h"
#include <string.h>

#define count_of(a) (sizeof(a) / sizeof((a)[0]))

// commands
#define HT16K33_SYSTEM_STANDBY 0x20
#define HT16K33_SYSTEM_RUN 0x21
#define HT16K33_SET_ROW_INT 0xA0
#define HT16K33_BRIGHTNESS 0xE0
#define HT16K33_DISPLAY_SETUP 0x80

// command states
#define HT16K33_DISPLAY_OFF 0x0
#define HT16K33_DISPLAY_ON 0x1
#define HT16K33_BLINK_2HZ 0x2
#define HT16K33_BLINK_1HZ 0x4
#define HT16K33_BLINK_0p5HZ 0x6

#define HT16K33_DOT_BIT 0x80

// 7-segment character patterns
static const uint8_t patterns[256] = {
    ['-'] = 0b01000000,
    [' '] = 0b00000000,
    ['0'] = 0b00111111,
    ['1'] = 0b00000110,
    ['2'] = 0b01011011,
    ['3'] = 0b01001111,
    ['4'] = 0b01100110,
    ['5'] = 0b01101101,
    ['6'] = 0b01111101,
    ['7'] = 0b00000111,
    ['8'] = 0b01111111,
    ['9'] = 0b01101111,
    // [':'] = 0b00000010,
    // ? use `display_colon()` instead; 0x2 will only look as intended at display_buffer[2]
    ['A'] = 0b01110111,
    ['B'] = 0b01111100,
    ['C'] = 0b00111001,
    ['D'] = 0b01011110,
    ['E'] = 0b01111001,
    ['F'] = 0b01110001,
};


// Internal display buffer (5 positions * 1 byte each)
static uint8_t display_buffer[5] = {0};

/**
 * Write a single byte to the HT16K33 device.
 */
static void i2c_write_byte(uint8_t val)
{
    i2c_write_blocking(i2c_default, HT16K33_I2C_ADDR, &val, 1, false);
}

void display_init(void)
{
    i2c_write_byte(HT16K33_SYSTEM_RUN);
    i2c_write_byte(HT16K33_SET_ROW_INT);
    i2c_write_byte(HT16K33_DISPLAY_SETUP | HT16K33_DISPLAY_ON);
}

void display_set_brightness(uint8_t brightness)
{
    i2c_write_byte(HT16K33_BRIGHTNESS | (brightness <= 15 ? brightness : 15));
}

void display_set_blink(int blink)
{
    int s = 0;
    switch (blink)
    {
    default:
        break;
    case 1:
        s = HT16K33_BLINK_2HZ;
        break;
    case 2:
        s = HT16K33_BLINK_1HZ;
        break;
    case 3:
        s = HT16K33_BLINK_0p5HZ;
        break;
    }

    i2c_write_byte(HT16K33_DISPLAY_SETUP | HT16K33_DISPLAY_ON | s);
}

void display_char(int position, char ch)
{
    if (position >= 0 && position < 5)
    {
        display_buffer[position] = patterns[(unsigned char)ch];
    }
}

void display_char_with_dot(int position, char ch)
{
    if (position >= 0 && position < 5)
    {
        display_buffer[position] = patterns[(unsigned char)ch] | HT16K33_DOT_BIT;
    }
}

void display_colon(bool on)
{
    display_buffer[2] = on ? 0x2 : 0x0;
}

void display_clear(void)
{
    memset(display_buffer, 0, sizeof(display_buffer));
}

void display_set_buffer(const uint8_t *buffer)
{
    if (buffer != NULL)
    {
        memcpy(display_buffer, buffer, 5);
        display_write();
    }
}

uint8_t *display_get_buffer(void)
{
    return display_buffer;
}

void display_write(void)
{
   // 1 byte for the start address + (5 positions * 2 bytes each) = 11 bytes
    uint8_t buf[11] = {0}; 
    buf[0] = 0; // redundant but explicit (start address)

    for (int i = 0; i < 5; i++) {
        // only set the first byte of each 2-byte display position
        buf[1 + (i * 2)] = display_buffer[i];
    }
    
    i2c_write_blocking(i2c_default, HT16K33_I2C_ADDR, buf, 11, false);
}
