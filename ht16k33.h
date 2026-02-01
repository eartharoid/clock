#include <stdint.h>
#include <stdbool.h>

#define HT16K33_I2C_ADDR 0x70

static uint8_t display_buffer[5];

/**
 * Initialize the HT16K33 display.
 * Must be called before any other ht16k33 functions.
 * Assumes i2c_default is already initialized.
 */
void display_init(void);

/**
 * Set the brightness of the display.
 * 
 * @param brightness Level from 0-15 (0 = minimum, 15 = maximum).
 *                    Values > 15 are clamped to 15.
 */
void display_set_brightness(uint8_t brightness);

/**
 * Set the blink rate of the display.
 * 
 * @param blink Blink mode:
 *              0 = no blink
 *              1 = 2 Hz blink
 *              2 = 1 Hz blink
 *              3 = 0.5 Hz blink
 */
void display_set_blink(int blink);

/**
 * Display a single character at the specified position.
 * 
 * @param position Position on display (0-4 for typical 5-digit displays)
 * @param ch       Character to display ('0'-'9', 'A'-'F', '-', ' ')
 */
void display_char(int position, char ch);

/**
 * Display a character with a dot/decimal point at the specified position.
 * 
 * @param position Position on display (0-4)
 * @param ch       Character to display
 */
void display_char_with_dot(int position, char ch);

/**
 * Control the colon (decimal point at position 2).
 * Typically used for time displays.
 * 
 * @param on true to show colon, false to hide
 */
void display_colon(bool on);

/**
 * Clear the display (all segments off).
 */
void display_clear(void);

/**
 * Write raw segment data to all 5 display positions.
 * Useful for animations and custom patterns.
 * 
 * @param buffer Pointer to 5-byte buffer with raw segment patterns
 * @param len Should be 5 (for safety)
 */
void display_set_buffer(const uint8_t *buffer, int len);

/**
 * Get pointer to the internal display buffer for direct manipulation.
 * Remember to call display_write() after modification.
 * 
 * @return Pointer to 5-byte display buffer
 */
uint8_t *display_get_buffer(void);

/**
 * Send the display buffer to the device.
 * Must be called after modifying display content to update the actual display.
 */
void display_write(void);

