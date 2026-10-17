# Task 100: genio42 soft-UART driver + QEMU tap — the hardware console

## Status
**DONE (2026-08-23).** Handoff desk-queue item #1 executed end-to-end:
Linux bit-bang TX driver (ttySU0 + console + earlycon) on genio42
("TxCDa"), QEMU wire-tap decoder, and a chain-boot validation where the
kernel's own console output is decoded byte-exact off the pin.  Full
detail: **docs/bcm2727-softuart.md**.

## What landed

1. **Linux** `drivers/tty/serial/bcm2727_softuart.c` (+ Kconfig/Makefile,
   =y in defconfig, binding yaml, board-dts node): serial-core TX-only
   8N1 bit-bang on the genio42 gpio, timed by the OS-timer frc counter
   (brcm,frc-counter phandle + offset; rate from the timer node's
   clock-frequency).  Absolute per-bit deadlines, measured-lead
   compensation, idle warm-up frame.  `/dev/ttySU0`, `console=ttySU0`,
   and `earlycon=bcm2727su[,pin[,baud[,frc_hz]]]]` (raw MMIO, runs from
   parse_early_param — ARM's early_ioremap_init precedes it).
2. **QEMU** (`nokia_e7.c`, task-100 section): genio42 wire tap —
   timestamps DATSET/DATCLR transitions of the tap pin in virtual ns
   and decodes like a logic analyser: mid-bit sampling, digital-PLL
   phase tracking, baud-change jumps (floored at ½ nominal), framing-
   error mute-until-relock.  Prints `su42: <line>`; env knobs
   SUUART_PIN/SUART_BAUD/SUUART_DEBUG/SUUART_TRACE/SUUART_RAW.
3. **Validation mode**: `-icount shift=auto` makes guest pacing and
   tap timestamps share the instruction-count clock — deterministic
   waveform (plain TCG shows ~1 flipped char/burst from host jitter).

## Validation (chain boot, 2026-08-23)

- `earlycon=bcm2727su console=ttySU0` (icount): boot log decoded
  byte-exact off the pin — "Booting Linux on physical CPU 0x0", full
  "Linux version …" line, "earlycon: bcm2727su0 …", "printk: legacy
  bootconsole [bcm2727su0] enabled" (511 frames, 2 framing errors,
  both in the first-use transient).
- Interactive (console=ttyAMA0): `stty -F /dev/ttySU0 115200; echo
  'The quick brown fox 0123456789 xyz' > /dev/ttySU0` → decoded
  byte-exact; `stty … 9600` → tap follows the baud change and decodes.
- Offline decoder harness (extracted from the QEMU file): clean /
  constant-offset / cold-first / σ½-2µs late-only noise / baud-switch
  matrix — 0 framing errors throughout.

## Bugs the bring-up caught

1. Run-length gap rounding breaks on software-transmitter jitter;
   mid-bit sampling + guest lead compensation is the working pair.
2. n_tty defaults to B9600 — writes to /dev/ttySU0 silently transmit
   at 9600 without stty/console= (cost a confusing first boot).
3. Idle-based resync deadlocks under CON_PRINTBUFFER's continuous log
   replay — re-hunt + double-validate instead.
4. Sub-bit glitch gaps can collapse naive auto-baud — floor downward
   moves at ½ nominal.
5. 6.12 serial core: xmit is a kfifo (`uart_port_tx()`), console
   locking via `uart_port_lock_irqsave()`.

## Hardware remaining (one wire)

Probe "TxCDa" into a USB-UART RX @115200; boot with
`earlycon=bcm2727su console=ttySU0`.  TODO-FOR-HARDWARE: confirm the
frc counter rate (earlycon 3rd arg + ostimer clock-frequency fix if
not 200 MHz-class).  RX path (half-bit sampling on the gpio irqchip)
is future work, not needed for a console.

## Next

Queue item #2 (SD channel ch1: flip mmc@20202900 okay + `-drive
id=sd`), then IRQ-driven mmc, ARMv6 toolchain.
