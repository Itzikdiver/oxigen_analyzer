#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

#define TFT_MOSI 10
#define TFT_SCLK 8
#define TFT_CS   -1    // If CS is tied to GND
#define TFT_DC    3
#define TFT_RST   4
#define TFT_BL   -1    // BL tied to 3.3V

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_GFXFF

#define SPI_FREQUENCY  4000000
