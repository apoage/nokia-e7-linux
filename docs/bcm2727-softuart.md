# BCM2727B1 soft-UART console — genio42 ("TxCDa") bit-bang TX

**Date:** 2026-08-23 (task 100)
**Sources:** docs/bcm2727-xti-trace.md §5 (no APE UART exists),
docs/genio-padtable.md §5 (factory pad 42 = code 0 = GPIO),
docs/bcm2727-icu-timer.md §3.2 (the frc counter at sys+0x3644),
docs/gpio-static-pinmux.md (DATA/direction register sequences).
**Status:** driver + earlycon shipped; validated end-to-end in the QEMU
chain boot (kernel log decoded byte-exact off the pin).  Hardware
bring-up needs one wire on the "TxCDa" test point.

---

## 0. Executive summary

1. **There is no UART block to drive** (task 095), so the hardware
   console is *made*: a TX-only bit-banged 8N1 UART on **genio42**
   (bank 1, pin 10 — the "TxCDa" UI-flex test point, left as GPIO by
   the factory pad table).  Bit timing comes from the OS timer's
   free-running 32-bit counter (`bcm2727-frc`, sys+0x3644, 200 MHz
   class): 1736 ticks per bit at 115200 Bd.
2. **Linux side** (`drivers/tty/serial/bcm2727_softuart.c`): a serial-core
   driver exposing `/dev/ttySU0` + a `console=ttySU0` console + an
   `earlycon=bcm2727su` earlycon that drives the registers raw before
   pinctrl exists.  Each frame uses absolute per-bit deadlines and
   *leads* them by the measured write latency, so software overhead
   cancels instead of accumulating; an invisible all-ones warm-up frame
   runs after idle periods so the first real frame is not paying cold
   i-cache/TLB/emulator costs.
3. **QEMU side** (task-100 section of `nokia_e7.c`): a wire tap on the
   pin — timestamps every DATSET/DATCLR transition of genio42 in
   virtual ns and decodes the waveform like a logic analyser:
   mid-bit sampling, light digital-PLL phase tracking, baud-change
   detection, mute-until-relock after framing errors.  Output:
   `su42: <line>` per decoded text line.
4. **Validation** (chain boot, `-icount shift=auto`): with
   `earlycon=bcm2727su console=ttySU0` the kernel's boot log comes out
   of the tap **byte-exact** ("Booting Linux on physical CPU 0x0",
   "Linux version 6.12.0-…", "earlycon: bcm2727su0 …", "printk: legacy
   bootconsole [bcm2727su0] enabled", …); interactive boots decode
   `echo … > /dev/ttySU0` payloads perfectly and follow
   `stty … 9600/115200` baud changes.

---

## 1. Linux driver

`drivers/tty/serial/bcm2727_softuart.c`
(CONFIG_SERIAL_BCM2727_SOFTUART[_CONSOLE], =y in
`bcm2727_nokia_e7_defconfig`):

- **DT node** (board dts):
  ```dts
  softuart: softuart {
      compatible = "brcm,bcm2727b1-softuart";
      gpios = <&genio 42 0>;               /* TX pin, active high */
      brcm,frc-counter = <&ostimer 0x44>;  /* counter reg + offset */
      current-speed = <115200>;
  };
  ```
  Binding: `Documentation/devicetree/bindings/serial/brcm,bcm2727-softuart.yaml`.
  The counter *rate* is read from the timer node's `clock-frequency`
  (200 MHz class) — one source of truth for both timer and bit timing.
- **TX engine** (`bcm2727su_send_char`): per character — optional
  warm-up frame after ≥5 char-times idle (all ones, invisible on the
  wire), prime both hot paths (one idle-level gpio write + one counter
  read), then t0 = counter; start bit; 8 data bits LSB-first; stop;
  every deadline absolute from t0 minus a constant `lead` = the start
  bit's measured overshoot (clamped to ¼ bit).  IRQs off per character;
  the SoC is single-core and console writes take the serial-core port
  lock, so frames cannot interleave.
- **serial core glue**: `uart_port_tx()` drains the xmit ring
  synchronously (TX is always complete; `tx_empty` = TEMT, `stop_tx` =
  noop).  `set_termios` forces 8N1 and honours the requested baud
  (bit ticks recomputed) — **note the tty layer defaults to B9600**, so
  `stty -F /dev/ttySU0 115200` before writing, or list `console=ttySU0`
  (console setup applies the DT baud).
- **console**: `ttySU` / `console=ttySU0`, CON_PRINTBUFFER (boot log
  buffered until probe replays through the wire).
- **earlycon**: `EARLYCON_DECLARE(bcm2727su, …)` — raw MMIO through the
  **permanent earlycon fixmap slots** (`set_fixmap_io` into
  `FIX_EARLYCON_MEM_BASE`/`_BASE2`; never `early_ioremap`, which ARM's
  `paging_init()` tears down at mm init — §3), configures the pin with the
  CONFIRMED Symbian output sequence (Q+0x04 gate=1, Q+0x10 dir-set,
  DATSET idle) and bit-bangs exactly like the driver.  Options:
  `earlycon=bcm2727su[,<pin>[,<baud>[,<frc_hz>]]]`
  (defaults 42, 115200, 200 MHz).  Runs from `parse_early_param` —
  ARM's `early_ioremap_init` precedes it (setup.c order verified).

## 2. QEMU wire tap (`nokia_e7.c`)

Hook: `bcm2727_gpio_data_write` — any DATSET/DATCLR write that flips
the tap bit (default bank 1 pin 10) feeds `bcm2727_softuart_edge()`
with a virtual-clock timestamp.  The tap arms on the line's first
idle-high assertion (earlycon setup or driver probe) and ignores
earlier traffic (NLoader).

Decoder (per frame = 10 bits, start-edge triggered):
- **mid-bit sampling** at t0 + (k+0.5)·T̂ for k = 0..9 — each edge only
  has to be within ½ bit of its own deadline (the guest's contract:
  absolute deadlines + lead compensation ⇒ late-only, bounded skew).
  This is strictly more tolerant than run-length/gap rounding, which
  constrains *consecutive-edge differences*.
- **digital PLL**: an edge near a nominal boundary nudges t0 by half
  its error (±¼ bit clamped) — slow drift tracked, glitch edges
  (mid-bit) cannot steer.
- **baud changes** from the frame's shortest run: all runs >3×T̂
  (slower baud, e.g. `stty 9600`) or <½·T̂ (faster) jumps the estimate;
  one refinement frame fixes jumps that landed on a 2-bit run.  Downward
  moves are floored at ½ the nominal baud — sub-bit glitch gaps
  (emulator hiccups) cannot collapse the estimate.
- **framing-error handling**: bad start/stop → re-hunt at the next
  falling edge and mute output until two consecutive clean frames
  confirm the char phase (a continuous console dump never idles ≥1.6
  bits, so idle-based resync would deadlock — learned the hard way).

Env knobs: `SUUART_PIN` (default 42), `SUUART_BAUD` (default 115200),
`SUUART_DEBUG=1` (per-frame), `SUUART_TRACE=1` (per-edge gaps),
`SUUART_RAW=1` (untagged stream).

**Run mode matters:** under plain TCG the virtual clock tracks host
time, so host jitter lands on the wire (~one flipped char per burst at
115200 — the same distortion a real wire would see from a jittery
transmitter).  For clean validation run the chain with
**`-icount shift=auto`**: virtual time becomes instruction-count-driven,
the guest's spin pacing and the tap's timestamps share one jitter-free
clock, and decode is deterministic.

## 3. Validation (2026-08-23, chain boot, root=/dev/mmcblk0)

Interactive boot (console=ttyAMA0, icount):
```
stty -F /dev/ttySU0 115200
echo 'The quick brown fox 0123456789 xyz' > /dev/ttySU0
  → su42: The quick brown fox 0123456789 xyz   (byte-exact)
stty -F /dev/ttySU0 9600; echo SU-9600-RATE-TEST > /dev/ttySU0
  → tap jumps to the slower baud and decodes it
stty -F /dev/ttySU0 115200; echo …             → back, decodes again
```
Console boot (`earlycon=bcm2727su console=ttySU0 loglevel=5`, icount):
the whole early log decodes byte-exact off the pin — "Booting Linux on
physical CPU 0x0", the full "Linux version …" line, CPU/model lines,
"earlycon: bcm2727su0 at I/O port 0x0 (options '')", "printk: legacy
bootconsole [bcm2727su0] enabled" (511 frames at kill time, 2 framing
errors total, both during the first-use transient of the session).
Run command in `docs/bcm2727-pinctrl-linux.md` §5.

Offline decoder tests: the tap section extracts into a host harness
(`su_regen.sh` pattern in lore/daily/2026-08-23) — synthetic matrix
(clean, constant offset, cold-first, σ½–2 µs late-only noise,
baud switches) all decode with 0 framing errors.

**Task-107 note (2026-08-26), task-109 RESOLUTION (2026-08-28):**
the wedge described above is **root-caused and fixed**.  The icount
su-console boot decodes the same early log **byte-exact — 511 frames, 0
framing errors** — through "earlycon: bcm2727su0 at I/O port 0x0
(options '')"; the "printk: legacy bootconsole … enabled" line listed
above does not reproduce on the loglevel-5 command because the record
replay runs before `loglevel=` is applied and later INFO records are
suppressed — one line of the task-100 extent claim was optimistic.
What followed was never a printk deadlock: it was an **infinite Oops
recursion**, and the "park in cpu_v6_do_idle" was the storm's end
state.  Chain, in order:

1. **The earlycon's MMIO died at mm init.**  The driver's earlycon
   mapped GENIO + the FRC counter with `early_ioremap()` (BTMAP fixmap
   slots).  ARM's `paging_init()` → `early_fixmap_shutdown()`
   (arch/arm/mm/mmu.c) clears the whole `bm_pte` fixmap pmd and
   re-creates *only* the permanent slots (`FIX_EARLYCON_MEM_BASE`, via
   `create_mapping`) — the contract is right there in
   asm/fixmap.h: "early_ioremap() is only available before
   paging_init()".  The bcm2727su console keeps writing until the real
   ttySU driver registers, long past that point.  The first
   `con->write` after the boundary did `t0 = readl(frc)` on the dead
   mapping — the storm's repeating fault address **0xffe3f644 is
   literally `bcm2727su_early.frc`** (verified by dumping the struct
   from the wedged VM).  The GPIO writes kept working for a while off
   *stale TLB entries* (early_fixmap_shutdown flushes only one page),
   which is why the wire showed garbled bursts during the storm.
2. **The Oops recursed.**  Engine one: `__show_regs_alloc_free()` →
   `mem_dump_obj()` → `kmem_dump_obj()` — ARM's `pfn_valid()` is
   memblock-based and passes during early boot while the FLATMEM
   `mem_map[]` array is still NULL (before `set_max_mapnr()`), so
   `virt_to_slab()` faults at `mem_map[pfn]` (0x9fe4) and *every*
   register dump faults again.  Engine two (once engine one is
   disabled): `show_pte()` itself reads page tables through the dead
   early-fixmap window and faults at the same 0xffe3f644.  loglevel 7
   spins in the storm (99.9 % CPU, ~1200 nested `__dabt_svc` frames,
   the stack descending ~600 KB through .data); loglevel 5 ends parked
   in `cpu_v6_do_idle` with IRQs masked.  ttyAMA0 boots never saw any
   of it: the generic pl011 earlycon maps through `FIX_EARLYCON_MEM_BASE`
   — the permanent slot — by construction.
3. **The forensic key** was a pmemsave dump of the printk ring out of
   the wedged VM (the storm keeps ~the last 250 records legible) plus
   one temporary kernel instrumentation (a first-line `die()` printk
   and disabling `mem_dump_obj` in `__show_regs_alloc_free`), which
   exposed the primary Oops text.

**The fix (kernel/linux, task 109; rebuild zImage only, no QEMU
change):**

- `arch/arm/include/asm/fixmap.h`: second permanent earlycon slot
  `FIX_EARLYCON_MEM_BASE2` (earlycon re-creation loop covers everything
  below `__end_of_permanent_fixed_addresses`).
- `bcm2727su_earlycon_setup()`: both pages (GENIO DATA trio page +
  shared IRQ-gate/DIRSET/FRC page) mapped with `set_fixmap_io()` into
  the permanent slots; no early_ioremap anywhere.
- `bcm2727su_probe()`: set `port->mapbase` — `uart_configure_port()`
  returns immediately when iobase/mapbase/membase are all zero, so the
  ttySU0 *real* console never registered (the "wedge" had always hidden
  this second bug: the handoff line never printed, and
  `console_on_rootfs()` warned "unable to open an initial console",
  leaving userspace silent).
- `bcm2727su_console_setup()`: use `uart_set_options()` so `co->cflag`
  is seeded — otherwise the console tty opens at n_tty's default baud
  and the wire silently changes speed at userspace hand-off.
- `arch/arm/kernel/process.c`: `__show_regs_alloc_free()` register
  provenance dump disabled (engine one above; an Oops during early boot
  must not recurse forever on this ARM32+FLATMEM config).

**Validation (2026-08-28):** `console=ttySU0`-only boots now reach
`e7:~#` with the su42 decode intact (docs/boot-109-su-full.txt):
loglevel 7 + `-icount shift=auto` boots clean to the prompt with every
boot-invariant line byte-identical to the ttyAMA0 text (the loglevel-5
canonical command decodes clean to the prompt under `-icount shift=3`).
**icount caveat, emulator-only:** with `shift=auto`, long console
idles (the loglevel-5 silent stretch before userspace) grow the
auto-shift until one poll-loop iteration costs more than ~¼ bit of
virtual time — the bit-bang's absolute deadlines then land up to one
iteration late and framing collapses (bits stretch 8.68→~14.4 µs).
Hardware is unaffected (a poll loop is ~tens of ns there); use a fixed
shift for idle-heavy captures.  The ttyAMA0 regression diff vs
`emulation/qemu-e7sw/boot-baseline-task104.log` is clean apart from
the new build strings and one new informative line ("softuart: ttySU0
at MMIO 0x202019f0 … is a BCM2727SU" — uart_report_port now runs).
Old logs: docs/boot-107-su-icount.txt (wedge),
docs/boot-107-icount-ttyama0.txt (control).

## 4. Bugs the bring-up caught (worth keeping)

1. **Run-length gap rounding is the wrong decoder** for a software
   transmitter: it breaks when consecutive-edge *differences* exceed ½
   bit, which TCG produces even when each edge is individually fine.
   Mid-bit sampling + the guest's lead compensation is the pair that
   works.
2. **The tty layer defaults to B9600** — a plain `echo > /dev/ttySU0`
   transmits at 9600 unless `stty` or `console=` set the rate; the
   first validation boot "decoded nothing" because of this, not
   because of the wire.
3. **Idle-based resync deadlocks on a continuous console dump** —
   CON_PRINTBUFFER replays the whole buffered log back-to-back; the
   tap must re-hunt at falling edges and gate output on re-validation
   instead.
4. **Sub-bit glitch gaps** (host hiccups, ~0.4 bit) can collapse an
   auto-baud estimate that trusts per-frame minimums — downward moves
   need a floor (½ nominal).
5. **First-frame transients** cost 1–2 garbled chars per session (cold
   TB/i-cache under TCG).  The guest's warm-up frame + priming absorbs
   most of it; the remainder is an emulator artifact (bare metal has
   none of these caches-of-caches).
6. Kernel-serial API drift (6.12): xmit is a kfifo — use
   `uart_port_tx()`; `uart_port_lock_irqsave()` for the console lock.
7. **`early_ioremap()` cannot back a console** (task 109): ARM kills
   the whole BTMAP fixmap pmd at `paging_init()`.  Anything that must
   keep doing MMIO across mm init needs a permanent fixmap slot
   (`set_fixmap_io`) — the generic earlycon has always done exactly
   that; now so does this driver.
8. **serial core silently skips console registration** for a uart_port
   with no iobase/mapbase/membase (`uart_configure_port()` returns
   before `register_console()` — task 109): a GPIO-only soft-UART must
   still set `port->mapbase` (informational) or `console=` never
   binds and userspace gets no `/dev/console`.
9. **ARM early-boot Oops register dumps recurse** (task 109):
   `mem_dump_obj()`/`kmem_dump_obj()` fault while FLATMEM `mem_map` is
   NULL even though `pfn_valid()` passes (memblock-based); disabled in
   `__show_regs_alloc_free()` on this kernel.  Worth an upstream
   conversation if the project ever rebases.
10. **icount `shift=auto` inflates the poll quantum after long idle**
    (task 109, emulator-only): auto-shift grows while the console is
    silent until one bit-bang poll iteration exceeds ~¼ bit of virtual
    time, and framing collapses.  Fixed-shift icount (e.g. `shift=3`)
    keeps post-idle userspace decodable; hardware is unaffected.

## 5. Hardware bring-up (when the phone is on the bench)

- **One wire**: probe/clamp the "TxCDa" test point (UI-flex area;
  docs/gpio-map.md lists GPIO42 = TxCDa) into any USB-UART RX at
  115200 8N1.  The factory pad table leaves pad 42 at code 0 (GPIO).
- Boot with `earlycon=bcm2727su console=ttySU0` — kernel text from the
  first printk onward.
- TODO-FOR-HARDWARE (same as the timer): the counter's true rate — if
  it is not 200 MHz-class, pass the real rate as the third earlycon
  option and fix the `clock-frequency` in the ostimer node; both timer
  and soft-UART then re-time together.
- **RX (future)**: half-bit sampling on the pin's edge irq through the
  gpio irqchip (task 099) would give a receive path (buttons-side
  first use: a shell).  Not needed for a console.
