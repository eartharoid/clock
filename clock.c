#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ht16k33.h"

int main()
{
    stdio_init_all();

    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C); // GPIO4/Pin6
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C); // GPIO5/Pin7
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    printf("Hello Clock!\n");

    display_init();
    display_set_brightness(3); // 25%

again:
    display_char(0, '-');
    display_char(1, '-');
    display_colon(false);
    display_char(3, '-');
    display_char(4, '-');
    display_write();

    sleep_ms(2000);

    const uint8_t frames[8][5] = {
        {0x40, 0x00, 0x00, 0x00, 0x00},
        {0x00, 0x40, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x40, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x40},
        {0x00, 0x00, 0x00, 0x40, 0x00},
        {0x00, 0x40, 0x00, 0x00, 0x00},
        {0x40, 0x00, 0x00, 0x00, 0x00},
    };
    display_colon(false);
    for (int r = 0; r < 2; r++)
    {
        for (int f = 0; f < 8; f++)
        {
            display_set_buffer(frames[f], 5);
            display_write();
            sleep_ms(125);
        }
    }

    display_char(0, '1');
    display_char(1, '2');
    display_colon(true);
    display_char(3, '3');
    display_char(4, '4');
    display_write();

    sleep_ms(2000);

    display_char(0, ' ');
    display_char(1, ' ');
    display_colon(false);
    display_char(3, '4');
    display_char_with_dot(4, '5');
    display_write();
    
    sleep_ms(2000);

    goto again;

    // while (1) {
    //     for (int i = 0; i < 15; i++) {
    //         display_set_brightness(i);
    //         sleep_ms(50);
    //     }
    //     for (int i = 14; i >= 0; i--) {
    //         display_set_brightness(i);
    //         sleep_ms(50);
    //     }
    // }
}