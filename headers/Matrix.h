#ifndef HEADERS_MATRIX
#define HEADERS_MATRIX

#include "mbed.h"

#define NOP_10 __asm__("NOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP")
#define NOP_20 __asm__("NOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP")
#define NOP_86 __asm__("NOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP\n\tNOP")
#define NOP_3 __asm__("NOP\n\tNOP\n\tNOP")


typedef struct _NeoColor {
    uint8_t blue;
    uint8_t red;
    uint8_t green;

    _NeoColor(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) : red(red), green(green), blue(blue) {}

    _NeoColor operator+(_NeoColor other) {
        return _NeoColor(red + other.red, green + other.green, blue + other.blue);
    }

    _NeoColor operator*(uint8_t k) {
        return _NeoColor(red * k, green * k, blue * k);
    }
} NeoColor;

class Matrix {
public:
    Matrix(PinName pin) {
        gpio_init_out(&gpio, pin);
        fill(20, 20, 20);
    }

    void flush() {
        for (uint8_t byte_loop = 64*3; byte_loop != 0; --byte_loop) {
            uint8_t byte_to_send = ((uint8_t*)pixels)[byte_loop-1];
            for (uint8_t bit_mask = 0x80; bit_mask != 0; bit_mask >>= 1) {
                if ((byte_to_send & bit_mask) == 0) {
                    // send 0
                    // Output a NeoPixel zero, composed of a short
                    // HIGH pulse and a long LOW pulse
                    gpio_write(&gpio, 1);
                    // NOP 10 times
                    NOP_10;
                    gpio_write(&gpio, 0);
                    // NOP 20 times
                    NOP_20;
                } else {
                    // send 1
                    gpio_write(&gpio, 1);
                    // NOP 86 times
                    NOP_86;
                
                    gpio_write(&gpio, 0);
                    // NOP 3 times
                    NOP_3;
                }
            }
        }
        gpio_write(&gpio, 0);
        // wait for the reset pulse
        wait_us(50);
        // ThisThread::sleep_for(1ms);
    }

    void fill(uint8_t green, uint8_t red, uint8_t blue) {
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                pixels[row][col].green = green;
                pixels[row][col].red = red;
                pixels[row][col].blue = blue;
            }
        }
    }

    int fill_rect(NeoColor color, uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
        if (x + width > 8 || y + height > 8) { return 1; }
        // print them in row-major order
        for (uint8_t row = 0; row < height; ++row) {
            for (uint8_t col = 0; col < width; ++col) {
                pixels[y + row][x + col] = color;
            }
        }

        return 0;
    }

    void fill(NeoColor color) {
        for (uint8_t row = 0; row < 8; ++row) {
            for (uint8_t col = 0; col < 8; ++col) {
                pixels[row][col] = color;
            }
        }
    }


private:
    gpio_t gpio;
    NeoColor pixels[8][8];
};

#endif // HEADERS_MATRIX