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

These chips have fewer pins than a BU0836X, so you get fewer inputs. The protocol and
behaviour are the same.

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

## Optional modes (like the Bodnar configuration utility)

On a fresh BU0836X every input is a plain button. The same input modes you'd set in Leo
Bodnar's configuration utility can be turned on in `config.h`. They use **input numbers**
(= button numbers), so button numbering never shifts:

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
  src/main.c            input scanning, debouncing, encoders, ADC
  src/usb_descriptors.c USB device/config/string descriptors
arduino/HIDJoystick/    Arduino sketch for ATmega32U4 boards
.github/workflows/      CI that builds the .uf2 files and checks the sketch compiles
```
