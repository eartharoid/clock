#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/rtc.h" // for real-time clock handling
#include "ht16k33.h"

// application states
typedef enum
{
    STATE_AWAITING_TIME,
    STATE_CLOSING_INITIAL,
    STATE_TRANSITION,
    STATE_RUNNING
} system_state_t;

typedef enum
{
    DISPLAY_MODE_TIME,
    DISPLAY_MODE_VOLUME
} display_mode_t;

#define STDIN_BUF_SIZE 128
char input_buffer[STDIN_BUF_SIZE];
int buffer_pos = 0;

// runtime state
static system_state_t current_state = STATE_AWAITING_TIME;
static display_mode_t display_mode = DISPLAY_MODE_TIME;
static bool first_time_received = false;
static absolute_time_t volume_end_time;
static int last_hour = -1, last_min = -1; // remember what is showing
static int displayed_vol = 0;             // most recent volume percent
static bool force_update = false;         // force a redisplay on next cycle

// helper that processes a complete line of input
static void handle_line(const char *line)
{
    size_t len = strlen(line);
    /* strip trailing CR/LF so that "T010203\n" still looks like seven bytes */
    while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    {
        --len;
    }

    if (len >= 7 && line[0] == 'T')
    {
        int hh = (line[1] - '0') * 10 + (line[2] - '0');
        int mm = (line[3] - '0') * 10 + (line[4] - '0');
        int ss = (line[5] - '0') * 10 + (line[6] - '0');

        datetime_t dt = {0}; /* start with all-zero */
        if (first_time_received)
        {
            /* keep the previously‑set date/weekday */
            rtc_get_datetime(&dt);
        }
        else
        {
            /* first sync – give the RTC a valid date */
            dt.year = 2021;
            dt.month = 1;
            dt.day = 21;
            first_time_received = true;
            current_state = STATE_CLOSING_INITIAL;
        }
        dt.hour = hh;
        dt.min = mm;
        dt.sec = ss;

        printf("got time %02d:%02d:%02d\n", hh, mm, ss);
        rtc_set_datetime(&dt);

        if (display_mode == DISPLAY_MODE_TIME)
        {
            force_update = true;
        }
    }
    else if (len >= 4 && line[0] == 'V')
    {
        /* volume logic unchanged */
        int vol = (line[1] - '0') * 100 + (line[2] - '0') * 10 + (line[3] - '0');
        if (vol > 100)
            vol = 100;
        display_mode = DISPLAY_MODE_VOLUME;
        volume_end_time = delayed_by_ms(get_absolute_time(), 2000);
        displayed_vol = vol; // remember for display
        force_update = true; // ensure the display is updated right away
    }
    else
    {
        /* optionally print ignored input for debugging */
        // printf("ignored line '%.*s'\n", (int)len, line);
    }
}

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

    // initialize peripheral hardware
    i2c_init(i2c_default, 250 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C); // GPIO4/Pin6
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C); // GPIO5/Pin7
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    // set up display
    display_init();
    display_set_brightness(3); // 25%

    // enable RTC (default date/time is unspecified until we receive a message)
    rtc_init();

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

    // run the startup animation until first time message is parsed and transition completes
    while (!(first_time_received && current_state == STATE_RUNNING))
    {
        // always listen for lines
        if (get_line_non_blocking())
        {
            // debug output is still useful
            printf("Received config: %s\n", input_buffer);
            handle_line(input_buffer);
        }

        // animation heartbeat
        if (time_reached(next_frame_time))
        {
            if (current_state == STATE_AWAITING_TIME || current_state == STATE_CLOSING_INITIAL)
            {
                display_set_buffer(frames_awaiting[frame_index]);
                display_write();

                if (current_state == STATE_CLOSING_INITIAL && frame_index == 5)
                {
                    current_state = STATE_TRANSITION;
                    frame_index = 0;
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
                    current_state = STATE_RUNNING;
                    // prepare for normal operation
                    last_hour = last_min = -1;
                    force_update = true;
                }
            }
            next_frame_time = delayed_by_ms(next_frame_time, frame_delay_ms);
        }
        sleep_us(100);
    }

    // main operation loop
    while (true)
    {
        if (get_line_non_blocking())
        {
            handle_line(input_buffer);
        }

        absolute_time_t now = get_absolute_time();

        if (display_mode == DISPLAY_MODE_TIME)
        {
            datetime_t dt;
            rtc_get_datetime(&dt);
            if (dt.hour != last_hour || dt.min != last_min || force_update)
            {
                /* always show both digits of the hour */
                char h_tens = '0' + (dt.hour / 10);
                char h_ones = '0' + (dt.hour % 10);
                char m_tens = '0' + (dt.min / 10);
                char m_ones = '0' + (dt.min % 10);

                display_char(0, h_tens);
                display_char(1, h_ones);
                display_colon(true);
                display_char(3, m_tens);
                display_char(4, m_ones);
                display_write();

                last_hour = dt.hour;
                last_min = dt.min;
                force_update = false;
            }
        }
        else if (display_mode == DISPLAY_MODE_VOLUME)
        {
            int vol = displayed_vol;

            if (vol == 100)
            {
                /* “_100.” */
                display_char(0, ' ');
                display_char(1, '1');
                display_colon(false);
                display_char(3, '0');
                display_char_with_dot(4, '0');
            }
            else
            {
                int tens = vol / 10;
                int ones = vol % 10;
                display_char(0, ' ');
                display_char(1, ' ');
                display_colon(false);
                display_char(3, (tens > 0) ? ('0' + tens) : ' ');
                display_char_with_dot(4, '0' + ones);
            }
            display_write();

            if (time_reached(volume_end_time))
            {
                display_mode = DISPLAY_MODE_TIME;
                force_update = true;
            }
        }

        sleep_us(100);
    }
}