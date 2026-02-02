#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ht16k33.h"

#define STDIN_BUF_SIZE 128
char input_buffer[STDIN_BUF_SIZE];
int buffer_pos = 0;

// Returns true only when a full line is ready
bool get_line_non_blocking()
{
    int c = getchar_timeout_us(0);

    while (c != PICO_ERROR_TIMEOUT)
    {
        // If we see a newline, terminate the string and return true
        if (c == '\n' || c == '\r')
        {
            input_buffer[buffer_pos] = '\0'; // Null terminator
            buffer_pos = 0;                  // Reset for next time
            return true;
        }
        // Otherwise, add the char to the buffer if there's room
        else if (buffer_pos < STDIN_BUF_SIZE - 1)
        {
            input_buffer[buffer_pos++] = (char)c;
        }

        // Check for more chars immediately
        c = getchar_timeout_us(0);
    }
    return false;
}

int main()
{
    stdio_init_all();

    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C); // GPIO4/Pin6
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C); // GPIO5/Pin7
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    display_init();
    display_set_brightness(3); // 25%

    const uint8_t frames_awaiting[6][5] = {
        {0x40, 0x00, 0x00, 0x00, 0x00},
        {0x00, 0x40, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x40, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x40},
        {0x00, 0x00, 0x00, 0x40, 0x00},
        {0x00, 0x40, 0x00, 0x00, 0x00},
    };

    const uint8_t frames_transition[8][5] = {
        {0x40, 0x40, 0x00, 0x40, 0x40},
        {0x40, 0x40, 0x00, 0x40, 0x40},
        {0x00, 0x00, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00},
        {0x40, 0x40, 0x00, 0x40, 0x40},
        {0x40, 0x40, 0x00, 0x40, 0x40},
        {0x00, 0x00, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00},
    };

    int frame_index = 0;
    absolute_time_t next_frame_time = get_absolute_time();
    const uint32_t frame_delay_ms = 250;

    typedef enum
    {
        STATE_AWAITING_CONFIG,
        STATE_CLOSING_INITIAL,
        STATE_TRANSITION,
        STATE_READY
    } system_state_t;
    system_state_t current_state = STATE_AWAITING_CONFIG;

    while (current_state != STATE_READY)
    {
        // 1. Check for a full configuration line
        if (get_line_non_blocking())
        {
            printf("Received config: %s\n", input_buffer);
            if (strncmp(input_buffer, "time", 4) == 0)
            {
                // ! todo
                current_state = STATE_CLOSING_INITIAL;
            }
        }

        // 2. Run animation logic
        if (time_reached(next_frame_time))
        {

            if (current_state == STATE_AWAITING_CONFIG || current_state == STATE_CLOSING_INITIAL)
            {
                display_set_buffer(frames_awaiting[frame_index]);
                display_write();

                // If we were closing and just hit the last frame, move to transition
                if (current_state == STATE_CLOSING_INITIAL && frame_index == 5)
                {
                    current_state = STATE_TRANSITION;
                    frame_index = 0; // Reset index for the new animation
                }
                else
                {
                    frame_index = (frame_index + 1) % 6;
                }
            }
            else if (current_state == STATE_TRANSITION)
            {
                display_set_buffer(frames_transition[frame_index]);
                display_write();

                frame_index++;
                if (frame_index >= 8)
                {
                    current_state = STATE_READY; // This will exit the while loop next iteration
                }
            }

            next_frame_time = delayed_by_ms(get_absolute_time(), 250);
        }
    }

    // again:
    //     display_clear();
    //     const uint8_t frames[6][5] = {
    //         {0x40, 0x00, 0x00, 0x00, 0x00},
    //         {0x00, 0x40, 0x00, 0x00, 0x00},
    //         {0x00, 0x00, 0x00, 0x40, 0x00},
    //         {0x00, 0x00, 0x00, 0x00, 0x40},
    //         {0x00, 0x00, 0x00, 0x40, 0x00},
    //         {0x00, 0x40, 0x00, 0x00, 0x00},
    //     };
    //     for (int r = 0; r < 4; r++)
    //     {
    //         for (int f = 0; f < 6; f++)
    //         {
    //             display_set_buffer(frames[f]);
    //             display_write();
    //             sleep_ms(250);
    //         }
    //     }

    //     display_char(0, '-');
    //     display_char(1, '-');
    //     display_char(3, '-');
    //     display_char(4, '-');
    //     display_write();
    //     display_set_blink(2);
    //     sleep_ms(2000);
    //     display_set_blink(0);

    //     display_char(0, '1');
    //     display_char(1, '2');
    //     display_colon(true);
    //     display_char(3, '3');
    //     display_char(4, '4');
    //     display_write();

    //     sleep_ms(2000);

    //     display_char(0, ' ');
    //     display_char(1, ' ');
    //     display_colon(false);
    //     display_char(3, '4');
    //     display_char_with_dot(4, '5');
    //     display_write();

    //     sleep_ms(2000);

    //     goto again;
}