# RP2040-HID

Firmware that turns a cheap microcontroller board into a **native USB joystick**, like a
Leo Bodnar BU0836 card. Plug it in and Windows, macOS, Linux and every game or sim see a
standard HID game controller. No drivers or helper software needed.

| Feature | RP2040 / RP2350 (`rp2040/`) | Arduino 32U4 (`arduino/`) |
|---|---|---|
| Boards | Raspberry Pi Pico, Pico 2, any RP2040 board (Zero, Tiny, Pro Micro RP2040…) | Leonardo, Micro, Pro Micro (ATmega32U4) |
| Buttons | 32 (direct pins, matrix and encoders combined) | 32 |
| Analog axes | 3 (GP26–GP28), 12-bit | up to 8 (A0–A11), 10-bit scaled to 12-bit |
| Hat switch (POV) | 1 × 8-way | 1 × 8-way |
| Rotary encoders | yes, each detent pulses a button | yes |
| Button matrix | yes (diodes recommended) | yes |
| USB poll rate | 1 ms | 1 ms |

Both versions send the same HID report as a BU0836A: 8 axes (X, Y, Z, Rx, Ry, Rz, Slider,
Dial, 0–4095), one hat and 32 buttons. Axes you don't wire report their centre value.

> **Uno / Nano / Mega won't work.** Their ATmega328P/2560 has no native USB, so it can't
> act as a joystick. Use a 32U4 board or an RP2040.

---

## RP2040 (Raspberry Pi Pico and friends)

### 1. Get the firmware

**Easiest:** on GitHub, open **Actions → Build firmware**, pick the latest run and
download `rp2040_hid-pico` (RP2040 boards) or `rp2040_hid-pico2` (RP2350 boards). Tagged
releases (`v*`) also attach the `.uf2` files to a GitHub Release.

**Or build it yourself** (needed if you change `config.h`):

```sh
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
git clone --depth 1 --branch 2.1.1 https://github.com/raspberrypi/pico-sdk.git
git -C pico-sdk submodule update --init lib/tinyusb
export PICO_SDK_PATH=$PWD/pico-sdk

cmake -S rp2040 -B build -DPICO_BOARD=pico     # or pico2, waveshare_rp2040_zero, ...
cmake --build build -j
# -> build/rp2040_hid.uf2
```

The Raspberry Pi Pico VS Code extension works too: open the `rp2040` folder.

### 2. Flash it

1. Hold the **BOOTSEL** button and plug the board into USB.
2. A drive called `RPI-RP2` (or `RP2350`) appears.
3. Drag `rp2040_hid.uf2` onto it. The board reboots as **"RP2040 Joystick"**.

To reflash later, hold BOOTSEL while plugging it in again.

### 3. Default wiring

Every switch goes between a GPIO and **GND**. Internal pull-ups are used, so no resistors are needed.

| Input | Pins | Joystick |
|---|---|---|
| Buttons | GP0 – GP15 | Buttons 1 – 16 |
| Hat switch | GP16 up, GP17 right, GP18 down, GP19 left | POV hat |
| Rotary encoder | GP20 (A), GP21 (B), common to GND | Buttons 17 (CCW) / 18 (CW) |
| Potentiometers | GP26, GP27, GP28 (wiper). Ends to **3V3** and **AGND** | X, Y, Z |

Change any of this in [`rp2040/src/config.h`](rp2040/src/config.h) and rebuild.

---

## Arduino Leonardo / Micro / Pro Micro

1. Open `arduino/HIDJoystick/HIDJoystick.ino` in the Arduino IDE.
2. Edit `config.h` (it opens as a tab) to match your wiring.
3. **Tools → Board → Arduino Leonardo** (also use this for a Pro Micro), or **Arduino Micro**.
4. Click **Upload**.

It only uses the HID library built into the Arduino AVR core, so you don't need to install
any extra libraries. The board still shows up as a serial port too, so normal uploads keep
working.

Default wiring (Pro Micro pin labels; switches to GND, pots to VCC/GND):

| Input | Pins | Joystick |
|---|---|---|
| Buttons | 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 14, 15, 16 | Buttons 1 – 14 |
| Potentiometers | A0, A1, A2, A3 | X, Y, Z, Rx |

Hat switch and encoders are off by default on the Arduino because the Pro Micro has few
pins. Enable them in `config.h` and take those pins out of `DIRECT_BUTTON_PINS`.

---

## Configuration reference

Both versions use the same options in `config.h`:

- `DIRECT_BUTTON_PINS`: one switch per pin.
- `MATRIX_ROW_PINS` / `MATRIX_COL_PINS`: a button matrix (rows × cols buttons from
  rows + cols pins), like the BU0836. Fit a diode in series with each switch, cathode
  towards the row, if you'll hold 3+ buttons at once.
- `HAT_PINS`: four pins for up, right, down, left. Comment it out to disable it.
- `ENCODER_PINS`: `{A, B}` pairs. If the direction is backwards, swap A and B.
  `ENCODER_STEPS_PER_DETENT` is 4 for typical EC11 encoders. Try 2 or 1 if one click
  needs several detents.
- `ANALOG_AXES`: `{pin, AXIS_X…AXIS_DIAL, inverted}`. `AXIS_SMOOTHING` trades jitter
  for lag. `AXIS_DEADZONE_EDGE` makes sure the ends of travel are reachable.
- `DEBOUNCE_MS`: how long a switch must be stable before it counts.
- RP2040 only: `USB_VID`/`USB_PID`/`USB_PRODUCT`. Each board also reports its unique flash
  ID as its USB serial number, so several identical boards on one PC stay separate.

Button numbers are assigned in this order: direct buttons, then matrix, then encoders
(two buttons each).

## Testing

- **Windows:** `Win+R` → `joy.cpl` → select the device → **Properties**.
- **Linux:** `jstest /dev/input/js0` or `evtest`.
- **Any OS:** <https://hardwaretester.com/gamepad>.

## Project layout

```
rp2040/                 Pico SDK + TinyUSB firmware (CMake)
  src/config.h          <- your pin configuration
  src/hid_report.h      HID report descriptor (BU0836A-style)
  src/main.c            input scanning, debouncing, encoders, ADC
  src/usb_descriptors.c USB device/config/string descriptors
arduino/HIDJoystick/    Arduino sketch for ATmega32U4 boards
  config.h              <- your pin configuration
.github/workflows/      CI that builds the .uf2 files and checks the sketch compiles
```
