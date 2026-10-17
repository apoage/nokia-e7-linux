# BCM2727B1 pad-mux table — recovered in plaintext from the OneNAND image

**Date:** 2026-08-22
**Source:** `firmware-re/nokia-e7-onenand.img` @ **0x1C5000–0x1C838E** (640 lines of
ASCII), found by sweeping the image for `0xNNNNNNNN \t` patterns.
**Artifacts:** `firmware-re/analysis/genio-padtable.txt` (raw),
`docs/genio-padtable.tsv (copy; original in firmware-re/analysis)` (parsed register/bit/value).
**Status:** extraction CONFIRMED (the bytes are in the image); semantic decode
(function-code meanings, pad numbering) INFERRED/pending.

This answers open question #1 of `docs/gpio-static-pinmux.md` §8: the pre-boot
pin-function configuration — which Symbian never performs — exists as a
**factory-written plaintext record** in the firmware image, even though the
GENIO_INIT partition itself is BB5-encrypted.

---

## 1. Format

Each line is `0xREGISTER \tBIT \tVALUE  \r\n` (note: space+TAB separators,
two trailing spaces). 640 lines, 41 unique registers, all in
`0x00031810–0x00031AB0`.

```json
{"register": "0x00031970", "bit": 5, "value": 0}
```

## 2. The two blocks

### 2.1 Pad function/mode grid — the pinmux itself

Registers `0x00031810–0x00031960`, stride 0x10 (**22 rows**); bit indices
0,4,8,12,16,20,24,28 (**8 columns**); values **0–4** (function codes).
**22 × 8 = 176 pads**, each with one of 5 functions.

| reg | b0 | b4 | b8 | b12 | b16 | b20 | b24 | b28 |
|-----|----|----|----|----|-----|-----|-----|-----|
| 0x31810 | 0 | 0 | 0 | 0 | 0 | 2 | 3 | 0 |
| 0x31840 | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 1 |
| 0x31850 | 1 | 0 | 2 | 0 | 0 | 0 | 0 | 1 |
| 0x31870 | 1 | 1 | 1 | 2 | 0 | 0 | 0 | 0 |
| 0x31880 | 0 | 4 | 4 | 4 | 4 | 4 | 4 | 0 |
| 0x318a0 | 1 | 0 | 1 | 1 | 1 | 1 | 0 | 0 |
| 0x318d0 | 1 | 0 | 1 | 1 | 1 | 1 | 1 | 2 |
| 0x318e0 | 2 | 1 | 4 | 4 | 4 | 4 | 4 | 4 |
| 0x318f0 | 4 | 4 | 4 | 4 | 4 | 4 | 4 | 4 |
| 0x31900 | 4 | 4 | 2 | 2 | 4 | 4 | 4 | 2 |
| 0x31920 | 0 | 1 | 1 | 1 | 1 | 0 | 2 | 2 |
| 0x31940 | 0 | — | 3 | 3 | 3 | 3 | 3 | 3 |
| 0x31950 | 3 | 3 | — | 0 | — | — | — | — |

(full grid: genio-padtable.tsv; `—` = no entry = default/unconfigured)

Code 0 dominates (likely GPIO mode); solid runs of 4 (rows 0x318f0, 0x318e0,
0x31880, 0x31900) look like a whole pad group in one alternate function
(candidates: DDR/memory, DSI display, camera CSI). Code 3 block at 0x31940/50,
code 2 scattered.

### 2.2 Boolean per-bank rails

Registers `0x00031970–0x00031AB0`, stride 0x10, per-bit values 0/1:

| register range | writes | bits | interpretation (INFERRED) |
|---|---|---|---|
| 0x31970/0x31980 | sparse | 2–19 | stray/non-bank config |
| 0x31990–0x319F0 | 32,32,32,32,30,19,32 | 0–31 | **wake-class banks 0–6** (0x319E0 = bank 5 = 19 active pins) |
| 0x319F0–0x31A30 | 32,32,24,22,24 | 0–31 | **data/edge-class banks** |
| 0x31A50–0x31A90 | 32,32,32,32,11 | 0–31 | further banks (edge/int class) |
| 0x31AB0 | 12 | all val 0 | clear-only |

## 3. Address base

Printed addresses are offsets; the constant that reconciles them with the
register file decoded in `docs/gpio-static-pinmux.md` §3 is:

```
phys = 0x20200000 + (printed − 0x30000)
```

Under this mapping: mode grid = phys 0x20201810–0x20201960, boolean rails
0x20201970–0x20201AB0 — i.e. one register file whose upper part
(0x20201990+) is exactly the WAKE/DATA/EDGE slot array gpio_static decoded
from the kernel driver (WAKE_SLOT 0x…1990+0x10·N, DATA_SLOT 0x…19F0+0x10·N).
This corroborates the §1.1 page-linear VA→phys inference (0xC6030000 page →
0x2020xxxx) that was previously unverified.

Caveats: the sub-slot layout differs in small offsets (+4) — the factory
table writes slot bases while the kernel driver uses +4 sub-registers
(status/set/clr); treat the exact sub-offset assignment as open.

## 4. Provenance

The table sits in a factory-data region of the OneNAND image:

| offset | content |
|---|---|
| 0x1A4000 | "PA_ESN / PA_SD / PA_OTI / PA_SIMLOC30 / PA_SWD" records (platform-auth data) |
| 0x1B8100 | 39 KB binary blob |
| 0x1C3000 | "UPAP" + "RAP Certificate v1 248" |
| 0x1C4000 | 0x809795A3 sub-image container (id 0xfd) |
| **0x1C5000** | **this pad table (ASCII)** |
| 0x200000 | SOS+CORE begins |

Likely a recorded dump of the GENIO apply (NLoader prints "Address : Value"
pairs during boot — lore 2026-03-08) from factory programming. The whole
0x1A3000–0x1C8400 area is factory-written data, which is why it is plaintext
while the shipped GENIO_INIT partition is BB5-encrypted.

Also mapped this session (context): the TOC entry "SOS+IVE3B" (nand 0xC0000)
actually covers the sub-image container area holding the main TOC (0x140000)
and NLoader (0x140800) plus sub-images at 0x144000/0x164000/0x190000/0x19C000/
0x1C4000; PRIMAPP (0x4000) and RAP3NAND (0x24000) are **plaintext ARM**
boot-chain stages (0x809795A3 container header, payload size at +0x20);
SOS+IVE3A/IVE3B payloads are fully encrypted (entropy 7.99); CCC/HWC/NPC/ADA
partitions ship empty (0xFF). Nokia-Cooker DES key (416ce871c3bd0a28) does
**not** decrypt GENIO_INIT or IVE3A (verified 2026-08-22, ECB, header skips).

## 5. Pad-numbering correlation (2026-08-22, second pass)

**Result: grid pad N (row-major, pad = row_index*8 + col) = kernel global
pin N (bank*32+pin) = schematic GPIO N — strongly indicated.** Every
correctly-identified anchor lands on a consistent code:

| schematic signal | pins | grid code | reading |
|---|---|---|---|
| pure-input GPIOs (vol_up 2, AccInt1 12, Hall 13, FMIRQ 17, TE0 33, CaptureKey 36, vol_down 65) | 7 pins | **all 0** | **code 0 = GPIO** (input or output) |
| GPIO-driven outputs (HMC_3V0_EN 19, DISP_RESET 37, EXTCLK 38) | 3 pins | all 0 | 0 = GPIO, direction is runtime DATSET/DIR policy |
| USB OTG ID/VBUS group (129–132, Symbian EDGE-driven → must be GPIO) | 4 | all 0 | consistent with GPIO mode |
| PriCam I2C pair (28/29) | 2 | 1,1 | peripheral-owned (code 1) |
| touch-int pair (122/123, if gpio-map id is right) | 2 | 2,2 | peripheral-owned (code 2) |
| PM handshake group (SysClk192 57, SleepX 58, VMEM2_EN 59, PM 60–62) | 6 | solid 4 | one peripheral function owns the group |

Full decoded grid (162 of 176 pads explicitly configured; `.` = not in
factory table):

```
reg       pad range  values (pad 8n .. 8n+7)
0x31810   0– 7    0 0 0 0 0 2 3 0
0x31820   8–15    0 0 0 0 0 0 0 0
0x31830  16–23    0 0 0 0 0 0 0 0
0x31840  24–31    0 1 1 1 1 1 1 1
0x31850  32–39    1 0 2 0 0 0 0 1
0x31860  40–47    0 0 0 0 0 0 0 1
0x31870  48–55    1 1 1 2 0 0 0 0
0x31880  56–63    0 4 4 4 4 4 4 0
0x31890  64–71    0 0 0 0 0 0 0 0
0x318a0  72–79    1 0 1 1 1 1 0 0
0x318b0  80–87    0 0 1 1 1 1 0 0
0x318c0  88–95    0 0 0 0 0 0 0 0
0x318d0  96–103   1 0 1 1 1 1 1 2
0x318e0 104–111   2 1 4 4 4 4 4 4
0x318f0 112–119   4 4 4 4 4 4 4 4
0x31900 120–127   4 4 2 2 4 4 4 2
0x31910 128–135   0 0 0 0 0 0 0 0
0x31920 136–143   0 1 1 1 1 0 2 2
0x31930 144–151   2 . 1 1 1 2 . 0
0x31940 152–159   0 . 3 3 3 3 3 3
0x31950 160–167   3 3 . 0 . . . .
0x31960 168–175   0 0 . . . . . .
```

Open anomalies (2): SecCam I2C pair splits 46=0 / 47=1 (a GPIO SDA with
peripheral SCL is odd — schematic pin id or our SecCam reading is suspect),
and FlashEnable 52=0 vs FlashTrigger 49=1. Also unresolved: the Symbian MMC
driver EDGE-writes kernel pins 58–62 while the grid puts 57–62 in code 4 —
either edge-detect blocks watch pads regardless of mux (plausible for
wakeup), or one of the two numberings is locally offset.

**Code-to-function names remain open** (codes 1–4 = four peripheral
functions, groupable by the runs above; candidates: code 4 = memory/PM
group incl. eMMC bus at 106–121, code 3 = one 8+2-pin block at 154–161,
code 1 = control/serial cluster). Decisive experiment once bare-metal
Linux runs: read all FSEL nibbles and toggle DATSET pins one bank at a
time.

## 6. What this unblocks

- **Linux pinctrl driver + DTS: DONE 2026-08-22** —
  `kernel/linux/drivers/pinctrl/bcm/pinctrl-bcm2727.c` (pinmux + 160 GPIOs,
  IRQ deferred), `arch/arm/boot/dts/broadcom/bcm2727{,-nokia-e7}.dts`,
  defconfig `bcm2727_nokia_e7_defconfig`; design + open items in
  `docs/bcm2727-pinctrl-linux.md`.
- Function-code → function-name table: still open (see §5); decisive data
  comes from the first bare-metal Linux run.
- UART capture on hardware: optional for pinmux; remaining value is
  DDR/clock config and validating the −0x30000 base-offset assumption.

## 7. Regeneration

```sh
python3 - <<'EOF'
import re
img = open('firmware-re/nokia-e7-onenand.img','rb').read()
raw = img[0x1C5000:0x1C8400]
open('firmware-re/analysis/genio-padtable.txt','wb').write(raw)
EOF
```
