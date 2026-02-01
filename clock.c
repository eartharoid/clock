/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include <ctype.h>

/* Example code to drive a 4 digit 14 segment LED backpack using a HT16K33 I2C
   driver chip

   NOTE: The panel must be capable of being driven at 3.3v NOT 5v. The Pico
   GPIO (and therefore I2C) cannot be used at 5v. In development the particular
   device used allowed the PCB VCC to be 5v, but you can set the I2C voltage
   to 3.3v.

   Connections on Raspberry Pi Pico board, other boards may vary.

   GPIO 4 (pin 6)-> SDA on LED board
   GPIO 5 (pin 7)-> SCL on LED board
   GND (pin 38)  -> GND on LED board
   5v (pin 40)   -> VCC on LED board
   3.3v (pin 36) -> vi2c on LED board
*/

// By default these display drivers are on bus address 0x70. Often there are
// solder on options on the PCB of the backpack to set an address between
// 0x70 and 0x77 to allow multiple devices to be used.
const int I2C_addr = 0x70;

// commands

#define HT16K33_SYSTEM_STANDBY 0x20
#define HT16K33_SYSTEM_RUN 0x21

#define HT16K33_SET_ROW_INT 0xA0

#define HT16K33_BRIGHTNESS 0xE0

// Display on/off/blink
#define HT16K33_DISPLAY_SETUP 0x80
// OR/clear these to display setup register
#define HT16K33_DISPLAY_OFF 0x0
#define HT16K33_DISPLAY_ON 0x1
#define HT16K33_BLINK_2HZ 0x2
#define HT16K33_BLINK_1HZ 0x4
#define HT16K33_BLINK_0p5HZ 0x6

uint8_t patterns[256] = {
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
    // [':'] = 0b00000010, // ? use `display_colon()` instead
    ['A'] = 0b01110111,
    ['B'] = 0b01111100,
    ['C'] = 0b00111001,
    ['D'] = 0b01011110,
    ['E'] = 0b01111001,
    ['F'] = 0b01110001,
};

uint8_t display_buffer[5];

void i2c_write_byte(uint8_t val)
{
#ifdef i2c_default
    i2c_write_blocking(i2c_default, I2C_addr, &val, 1, false);
#endif
}

void ht16k33_init()
{
    i2c_write_byte(HT16K33_SYSTEM_RUN);
    i2c_write_byte(HT16K33_SET_ROW_INT);
    i2c_write_byte(HT16K33_DISPLAY_SETUP | HT16K33_DISPLAY_ON);
}

// // Send a specific binary value to the specified digit
// static inline void ht16k33_display_set(int position, uint16_t bin)
// {
//     uint8_t buf[3];
//     buf[0] = position * 2;
//     buf[1] = bin & 0xff;
//     buf[2] = bin >> 8;
// #ifdef i2c_default
//     i2c_write_blocking(i2c_default, I2C_addr, buf, count_of(buf), false);
// #endif
// }

// static inline void ht16k33_display_char(int position, char ch)
// {
//     ht16k33_display_set(position, patterns[ch]);
// }

static inline void display_colon(bool on)
{
    if (on)
        display_buffer[2] = 0x2;
    else
        display_buffer[2] = 0x0;
}

void ht16k33_set_brightness(uint bright)
{
    i2c_write_byte(HT16K33_BRIGHTNESS | (bright <= 15 ? bright : 15));
}

void ht16k33_set_blink(int blink)
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

void push_display_buffer()
{
    // 1 byte for Address + (5 positions * 2 bytes each) = 11 bytes
    uint8_t buf[11] = {0}; 
    buf[0] = 0x00; // redundant but explicit (start address)

    for (int i = 0; i < 5; i++) {
        // We skip every other byte because the HT16K33 uses 
        // 16-bit slots, but the 7-segment only uses the lower 8 bits.
        buf[1 + (i * 2)] = display_buffer[i];
    }

#ifdef i2c_default
    i2c_write_blocking(i2c_default, I2C_addr, buf, 11, false);
#endif
}

int main()
{

    stdio_init_all();

#if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
#warning i2c/ht16k33_i2c example requires a board with I2C pins
#else
    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    printf("Welcome to HT33k16!\n");

    ht16k33_init();

    // ht16k33_display_set(0, 0);
    // ht16k33_display_set(1, 0);
    // ht16k33_display_set(2, 0);
    // ht16k33_display_set(3, 0);
    // ht16k33_display_set(4, 0);

    ht16k33_set_brightness(3); // 25%

    display_buffer[0] = patterns['3'] | 0x80; // with dot
    display_buffer[1] = patterns['1'];
    display_buffer[2] = patterns[' '];
    display_buffer[3] = patterns['4'];
    display_buffer[4] = patterns['1'];
    push_display_buffer();

// again:
    //     sleep_ms(1000);

    //     // Set all segments on all digits on
    //     ht16k33_display_set(0, 0xffff);
    //     ht16k33_display_set(1, 0xffff);
    //     ht16k33_display_set(2, 0x0002); // :
    //     ht16k33_display_set(3, 0xffff);
    //     ht16k33_display_set(4, 0xffff);

    //     // Fade up and down
    // for (int i = 0; i < 15; i++)
    // {
    //     ht16k33_set_brightness(i);
    //     sleep_ms(50);
    // }

    // for (int i = 14; i >= 0; i--)
    // {
    //     ht16k33_set_brightness(i);
    //     sleep_ms(50);
    // }

        // ht16k33_set_brightness(3);

    //     ht16k33_set_blink(1); // 0 for no blink, 1 for 2Hz, 2 for 1Hz, 3 for 0.5Hz
    //     sleep_ms(5000);
    //     ht16k33_set_blink(0);

    // goto again;
#endif
}