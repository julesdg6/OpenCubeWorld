# Hardware

## Primary target: Waveshare ESP32-S3-LCD-1.54 (and ESP32-S3-Touch-LCD-1.54)

ESP32-S3R8 (dual-core 240 MHz, 16 MB QIO Flash, 8 MB OPI PSRAM) with onboard
1.54" 240×240 ST7789 IPS SPI LCD, QMI8658 6-axis IMU, 3 push buttons, and
optional CST816 capacitive touch (auto-detected at boot).

### Verified GPIO Pin Map (`OCW_BOARD_WAVESHARE_ESP32S3_LCD_154`)

Sourced from `waveshareteam/ESP32-S3-Touch-LCD-1.54` and defined in
`firmware/libraries/OpenCubeWorld/config.h`:

| Subsystem | Signal | ESP32-S3 GPIO | Define |
|---|---|---|---|
| **ST7789 LCD (SPI)** | SCLK | `38` | `OCW_LCD_SCLK_PIN` |
| | MOSI | `39` | `OCW_LCD_MOSI_PIN` |
| | CS | `21` | `OCW_LCD_CS_PIN` |
| | DC | `45` | `OCW_LCD_DC_PIN` |
| | RST | `40` | `OCW_LCD_RST_PIN` |
| | Backlight | `46` (active HIGH) | `OCW_LCD_BL_PIN` |
| **Shared I2C Bus** | SDA | `42` | `OCW_I2C_SDA_PIN` |
| | SCL | `41` | `OCW_I2C_SCL_PIN` |
| **QMI8658 6-Axis IMU** | I2C Addr | `0x6B` (fallback `0x6A`) | on shared I2C |
| **CST816 Touch** *(Touch model)* | I2C Addr | `0x15` (auto-detected) | on shared I2C |
| | TP_RST | `47` | `OCW_TP_RST_PIN` |
| | TP_INT | `48` | `OCW_TP_INT_PIN` |
| **3 Physical Buttons** | Left (BOOT) | `0` (active LOW) | `OCW_BTN_LEFT_PIN` |
| | Center (PWR) | `5` (active LOW) | `OCW_BTN_CENTER_PIN` |
| | Right (KEY) | `4` (active LOW) | `OCW_BTN_RIGHT_PIN` |
| **Cube World Link** | Data Line | `11` (native 3.3 V!) | `OCW_CUBE_DATA_PIN` |

### Controls (matching the real Radica Cube World 3-button + motion layout)

- **Left button (GPIO 0 / BOOT)**: Short press cycles character (`WHIP`, `DODGER`, `HANS`, `HANDY`, `SCOOP`); long press cycles monochrome LCD palette (`STN` olive-grey / `B&W` / `INV`).
- **Center button (GPIO 5 / PWR)**: Plays character signature trick (or triggers WHIP in Linked mode, or cycles LCD test patterns in Debug mode); long press triggers Dizzy.
- **Right button (GPIO 4 / KEY)**: Short press cycles view mode (`Single Cube 32x32` → `Linked World + Protocol Monitor` → `Debug / LCD Test`); long press triggers Linked-Cube transfer (lowering Venetian blind).
- **QMI8658 IMU**: Tilt left/right shifts the stick figure; flip upside-down triggers `CUBE TURN` tumble; vigorous shake triggers `DIZZY`.
- **Touch (optional)**: Auto-detected on Touch-LCD-1.54 models; skipped cleanly on non-touch LCD-1.54 boards.

## Build & Flash (PlatformIO)

```bash
pio run -e waveshare_esp32s3_lcd_154 -t upload
pio device monitor -b 115200
```

## Fallback: Arduino Uno + Waveshare 1.54" Touch LCD Module (SKU 34370)

When built for AVR (`__AVR__`), `config.h` automatically falls back to the
original Arduino Uno 12-pin cable pinout (CS=9, DC=8, RST=7, SCK=13, MOSI=11,
BL=6, TP_RST=2, TP_INT=3, Cube Data=4).

## Wiring the Cube World data line (CubeLink mode)

The Radica Cube World single-wire bus is **3.3 V pull-up terminated**.
On ESP32-S3, GPIO 11 is natively 3.3 V so you can wire DATA + GND directly
(keep lead < 10 cm). On a 5 V AVR (Uno), level-shift the MCU→toy direction.
