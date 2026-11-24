# Hardware

Waveshare **1.54inch Touch LCD Module** (SKU 34370): 240x240 ST7789S
(4-wire SPI, IPS) + CST816D capacitive touch (I2C @ 0x15).

## Default pin map (Arduino Uno)

`firmware/libraries/OpenCubeWorld/config.h` - every pin can be
overridden by `#define`-ing the `OCW_*` name before including the
library.

| signal | Uno pin | define |
|---|---|---|
| LCD CS | 9 | `OCW_LCD_CS_PIN` |
| LCD DC | 8 | `OCW_LCD_DC_PIN` |
| LCD RST | 7 | `OCW_LCD_RST_PIN` |
| LCD SCK | 13 | `OCW_LCD_SCLK_PIN` |
| LCD MOSI | 11 | `OCW_LCD_MOSI_PIN` |
| LCD backlight | 6 | `OCW_LCD_BL_PIN` |
| TP RST | 2 | `OCW_TP_RST_PIN` |
| TP INT | 3 | `OCW_TP_INT_PIN` (polled; INT optional) |
| TP SDA/SCL | A4/A5 (default I2C) | via Wire |
| Cube data line | 4 | `OCW_CUBE_DATA_PIN` |

The 12-pin JST cable on the module carries: 3V3, GND, MOSI, SCK,
CS, DC, RST, BL, TP_INT, TP_RST, SDA, SCL.

## Wiring the Cube World data line (CubeLink mode)

The toy's data line is 3.3 V logic, pull-up terminated. To attach a
3.3/5 V microcontroller:

- **Best:** 3.3 V MCU (e.g. ESP32-S3, ATtiny85 at 3.3 V) - connect the
  data line directly (plus GND).
- **5 V AVR (Uno):** the line idles high at 3.3 V, which a 5 V pin
  reads fine as HIGH; when the toy drives LOW it pulls the line to
  ~0 V, also fine. The risk is the *reverse* direction (MCU driving
  the toy's line): a 5 V drive can exceed the toy's 3.3 V absolute
  max. If you only **listen** (pollForPeer + readPacket), direct
  connection is safe in practice. If you **transmit** to a real toy,
  level-shift the MCU->toy direction (e.g. 3.3 V GPIO, or a
  voltage-divider on the MCU output).
- Keep the wire short (< 10 cm) and add a 100 nF cap across GND near
  the connector.

## Libraries

- Display: **Arduino_GFX** (`SPI_DataBus` + `Arduino_ST7789`, IPS
  flag on, rotation 0). The Waveshare `GFX_Library_for_Arduino` is a
  fork and works too.
- Touch: the CST816D driver in the repo (`cst816d.{h,cpp}`) - polled
  over I2C, no interrupt needed.
- `Wire` for the touch controller.

## Flashing

Arduino IDE -> Tools: Board "Arduino Uno", port of your choice. Copy
`firmware/libraries/OpenCubeWorld/` into your libraries folder, open
any sketch under `firmware/`, upload.
