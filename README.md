# RP2040-HID

Firmware that turns a cheap microcontroller board into a **native USB joystick that works
like a Leo Bodnar BU0836X**. Flash it, plug it in, and Windows, macOS, Linux and every game
or sim see a standard HID game controller. No drivers or configuration needed.

As on the BU0836X, **every usable pin is an input** and its job depends on what the pin can do:

- **Analog-capable pins are axes**: X, Y, Z, Rx, Ry, Rz, Slider, Dial, in pin order.
  Wire a potentiometer's ends to 3V3/VCC and GND, wiper to the pin.
- **Every other pin is a button**: wire a switch between the pin and GND. Internal pull-ups
  are used, so no resistors are needed. Input N is always joystick button N.

The device reports the same layout as a BU0836X: 8 axes (0–4095), one 8-way hat and 32
buttons, polled every 1 ms. Axes without a pin report their centre value.

| Board | Buttons | Axes |
|---|---|---|
| Raspberry Pi Pico / Pico W / Pico 2 / Pico 2 W | 23 (GP0–GP22) | 3 (GP26–GP28), 12-bit |
| Waveshare RP2040-Zero | 25 (GP0–GP15, GP17–GP25) | 4 (GP26–GP29), 12-bit |
| Arduino Leonardo / Micro | 17 (D0–D13, MISO, SCK, MOSI) | 6 (A0–A5), 10-bit |
| Arduino Pro Micro | 14 (0–10, 14–16) | 4 (A0–A3), 10-bit |

These chips have fewer pins than a BU0836X. To get the full **8 axes and 32 buttons**, add
an MCP3208 ADC and/or 74HC165 shift registers. See [Expansion](#expansion-full-bu0836x-count)
below.

> **Uno / Nano / Mega won't work.** Their ATmega328P/2560 has no native USB, so it can't
> act as a joystick. Use a 32U4 board or an RP2040.

---

## RP2040 / RP2350

### 1. Get the firmware

**Easiest:** on GitHub, open **Actions → Build firmware**, pick the latest run and download
the `.uf2` for your board (`pico`, `pico2` or `waveshare_rp2040_zero`). Tagged releases
(`v*`) also attach the `.uf2` files to a GitHub Release.

**Or build it yourself:**

```sh
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
git clone --depth 1 --branch 2.1.1 https://github.com/raspberrypi/pico-sdk.git
git -C pico-sdk submodule update --init lib/tinyusb
export PICO_SDK_PATH=$PWD/pico-sdk

cmake -S rp2040 -B build -DPICO_BOARD=pico     # or pico2, pico_w, waveshare_rp2040_zero
cmake --build build -j
# -> build/rp2040_hid.uf2
```

Other RP2040 boards use the Pico pin layout. Edit the pin lists in
[`rp2040/src/config.h`](rp2040/src/config.h) if yours exposes different pins.

### 2. Flash it

1. Hold the **BOOTSEL** button and plug the board into USB.
2. A drive called `RPI-RP2` (or `RP2350`) appears.
3. Drag the `.uf2` onto it. The board reboots as **"RP2040 Joystick"**.

### 3. Wiring (Pico)

| Pins | Joystick |
|---|---|
| GP0 … GP22 | Buttons 1 … 23 (GPn = button n+1) |
| GP26, GP27, GP28 | X, Y, Z axes (pot ends to **3V3(OUT)** and **AGND**) |

---

## Arduino Leonardo / Micro / Pro Micro

1. Open `arduino/HIDJoystick/HIDJoystick.ino` in the Arduino IDE.
2. **Pro Micro only:** in the `config.h` tab, uncomment `#define BOARD_PRO_MICRO`.
3. **Tools → Board → Arduino Leonardo** (also for a Pro Micro), or **Arduino Micro**.
4. Click **Upload**.

You don't need any extra libraries. The board still shows up as a serial port, so normal
uploads keep working. In `joy.cpl` it appears under the board's name (e.g. "Arduino Leonardo").

---

## Expansion: full BU0836X count

Two cheap chips, each switched on with one line in `config.h`. The pins they use are
removed from the button list automatically.

### MCP3208: 8 × 12-bit axes (`MCP3208_ENABLED 1`)

Its channels become the first axes (CH0 = X … CH7 = Dial). The board's own analog pins fill
any axes still free; if there are none, they become extra buttons. An MCP3204 (4 channels)
works too: set `MCP3208_CHANNELS 4`.

| MCP3208 pin | Pico (3.3 V) | RP2040-Zero | Leonardo / Micro / Pro Micro (5 V) |
|---|---|---|---|
| VDD (16), VREF (15) | 3V3(OUT) | 3V3 | VCC / 5V |
| AGND (14), DGND (9) | GND | GND | GND |
| CLK (13) | GP18 | GP6 | SCK (15) |
| DOUT (12) | GP16 | GP4 | MISO (14) |
| DIN (11) | GP19 | GP7 | MOSI (16) |
| CS/SHDN (10) | GP17 | GP5 | 10 |
| CH0–CH7 (1–8) | pot wipers | pot wipers | pot wipers |

Fit a 100 nF capacitor between VDD and GND close to the chip, and wire each pot's ends to
the same VREF supply and GND. On the RP2040 you can move the SPI pins, but they must belong
to one hardware SPI block (the build tells you if they don't). On a Leonardo, SCK, MISO and
MOSI are on the 6-pin ICSP header.

### 74HC165 shift registers: 8 buttons per chip (`SHIFT_REG_ENABLED 1`)

Four chips give 32 buttons from 3 pins. Shift register inputs come first: chip 1's A–H are
inputs/buttons 1–8, chip 2's are 9–16, and so on. The board's own pins come after them.
Once all 32 buttons are taken, those pins can still be used for the hat or encoders.

| 74HC165 pin | Pico | RP2040-Zero | Arduino | Notes |
|---|---|---|---|---|
| QH (9), chip 1 | GP20 | GP8 | 5 | DATA |
| CLK (2), all chips | GP21 | GP9 | 6 | |
| SH/LD (1), all chips | GP22 | GP10 | 7 | LOAD |
| CLK INH (15) | GND | GND | GND | |
| SER (10) | | | | chip *n*'s SER ← chip *n+1*'s QH; last chip's SER → VCC |
| VCC (16) / GND (8) | **3V3** | **3V3** | 5V | |
| A–H (11–14, 3–6) | switches | switches | switches | 10 kΩ pull-up to VCC on each, switch to GND |

**On an RP2040, power the 74HC165s from 3V3, not 5 V.** RP2040 pins aren't 5 V tolerant.
The 74HC165 has no internal pull-ups, so every input needs its own resistor; 9-pin 10 kΩ
resistor networks (SIP) make this tidy.

Encoders wired to shift register inputs work too: the chain is re-read on every pass of
the main loop when an encoder uses it.

## Optional modes (like the Bodnar configuration utility)

On a fresh BU0836X every input is a plain button. The same input modes you'd set in Leo
Bodnar's configuration utility can be turned on in `config.h`. They use **input numbers**
(= button numbers), so button numbering never shifts. With shift registers enabled, input 1
is shift register input 1; otherwise it's the board's first pin.

- `ENCODER_INPUTS { {1, 2} }`: inputs 1 and 2 read a rotary encoder. Each detent gives
  a short press on button 1 (one way) or button 2 (the other way). `ENCODER_STEPS_PER_DETENT`
  is 4 for typical EC11 encoders; try 2 or 1 if one click needs several detents.
- `HAT_INPUTS { 1, 2, 3, 4 }`: inputs 1–4 (up, right, down, left) drive the POV hat
  instead of buttons 1–4.
- `AXIS_INVERT`: bitmask to flip axes (bit 0 = X … bit 7 = Dial).
- `AXIS_SMOOTHING`, `AXIS_DEADZONE_EDGE`, `DEBOUNCE_MS`: tuning.

Rebuild/re-upload after changing them.

## Testing

- **Windows:** `Win+R` → `joy.cpl` → select the device → **Properties**.
- **Linux:** `jstest /dev/input/js0` or `evtest`.
- **Any OS:** <https://hardwaretester.com/gamepad>.

## Project layout

```
rp2040/                 Pico SDK + TinyUSB firmware (CMake)
  src/config.h          pin layout per board, optional modes
  src/hid_report.h      HID report descriptor (BU0836X layout)
  src/main.c            input scanning, debouncing, encoders, ADC, MCP3208, 74HC165
  src/usb_descriptors.c USB device/config/string descriptors
arduino/HIDJoystick/    Arduino sketch for ATmega32U4 boards
.github/workflows/      CI that builds the .uf2 files and checks the sketch compiles
```
