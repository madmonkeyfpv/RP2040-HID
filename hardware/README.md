# Hardware (EasyEDA Standard)

`SCH_rp2040-basic_expansion.json` and `PCB_rp2040-basic_expansion.json` are the
rp2040-basic board with the full BU0836X input count added:

- **U9 MCP3208** on GP16-19: 8 x 12-bit axes, 100 nF + 1 uF decoupling.
- **U10-U13 74HC165** chained on GP20-22: 32 buttons, two 10k x4 pull-up
  arrays and 100 nF per chip. U10 is chip 1 (buttons 1-8).
- **J1-J9 WAGO 233 (2.54 mm)** push-in terminals: axes (3V3 / signal / GND),
  buttons with GND points, and the spare GP0-15 and GP26-28.

The RP2040 section, the USB-B socket and every existing track are unchanged.
New parts are on schematic sheet 2 ("IO_expansion"). The board is about
120 x 100 mm with three M3 holes. Use the `rp2040_hid-pico-expansion.uf2`
firmware.

## Before ordering

1. Import both files into EasyEDA Standard.
2. Route → Auto Router, then Shift+B to refill the ground pours, then run DRC.
3. Check the WAGO 233 footprint (one 1.0 mm hole per pole, 9 mm body) against
   WAGO's 233-5xx datasheet, or swap in a library footprint.
4. Fill in LCSC part numbers for U9-U13, RN1-RN8 and J1-J9 (left blank).

`preview_*.png` are rough renders, not EasyEDA screenshots.

`tools/gen_easyeda.py` rebuilds the two JSON files from the original
rp2040-basic exports:

    python3 tools/gen_easyeda.py SCH_in.json PCB_in.json out_dir
