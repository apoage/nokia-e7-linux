# Debugging Notes — Solved & Unsolved

## Solved Problems

### SCM CONTROL_STATUS must return GP (0x300)
Without this, PM init enters secure-mode path → SMC call → hangs with IRQs disabled.
Fix: `omap3630_scm_read()` returns 0x300 at offset 0x2F0.

### PRM vs CM register mapping
`osc_sys_ck` and `sys_ck` are under `prm_clocks` DT node, read PRM not CM.
PRM_CLKSEL at 0x48306D40, PRM_CLKSRC_CTRL at 0x48307270.

### DPLL IDLEST must track CLKEN_PLL writes
PM init toggles DPLLs lock/bypass/stop and polls IDLEST. Must track CM_CLKEN_PLL
writes and derive IDLEST state. Four DPLLs: core, periph, MPU, IVA2.

### I2C SYSSTATUS.RDONE=1 required
Without this, PM init spins forever (3.7M reads measured). Return bit 0 = 1
at I2C offset 0x10.

### MMC deferred probes
Without DMA controller, MMC driver gets -EPROBE_DEFER endlessly.
Fix: disable mmc1/2/3 in DTS.

### SYSSTATUS polling (2026-02-28)
`omap_hwmod` and `ti-sysc` poll SYSSTATUS bit 0 at various offsets during softreset.
`create_unimplemented_device()` returns 0 → 10ms timeout × 30+ devices = 1.5s wasted.
Fix: replaced with `omap3_periph_stub` returning 1 for all reads. Boot 2.57s → 1.0s.

### CRLF in init scripts
The Write tool produces CRLF. Kernel exec parses shebang as `#!/bin/sh\r` → ENOENT (-2).
Fix: `sed -i 's/\r$//'` or verify with `file` command. Happened TWICE (Tasks 010, 011).

### virtio-blk FLUSH hang
`sync()` hangs — QEMU async fsync completion not delivered to guest.
Workaround: `cache.no-flush=on` on QEMU drive option.

### SDRC DLL spin loop — SOLVED (Task 016)
**Symptom**: Ext4 root boot hangs at "PM: genpd: Disabling unused power domains".
**Root cause**: `sleep34xx.S` polls SDRC_DLLA_STATUS (0x6D000064) bit 2 (DLL locked)
in idle loop. SDRC returns 0 → infinite spin (8.4M reads/20s measured).
**Fix**: Proper SDRC register model in QEMU — tracks DLLA_CTRL writes, defaults
DLLA_CTRL=0x0C (DLL enabled + lock mode). The `is_dll_in_lock_mode` check in
sleep34xx.S takes the fast exit branch, skipping the DLL polling loop entirely.

### OMAP3 idle handler crashes — SOLVED (Tasks 016+017)
**Symptom**: After SDRC fix, idle handler crashes at c0a8d040 (BSS garbage PC).
**Root cause TWO bugs:**
1. `omap3_pm_idle()` calls `omap3_do_wfi()` directly — but `omap3_do_wfi` ends with
   `ldmfd sp!, {r4-r11, pc}` without a matching push. The matching push is in
   `omap34xx_cpu_suspend` which is only called via cpuidle. Without CONFIG_CPU_IDLE,
   the stack gets corrupted → jumps to garbage PC. **Fix**: Enable CONFIG_CPU_IDLE.
2. `omap3_vc_set_pmic_signaling()` dereferences NULL `vc.timings` when no TWL PMIC
   is configured. The kernel reports "No PMIC info for vdd_core" but doesn't guard
   against it. **Fix**: Added `if (!vd || !c) return;` guard in `vc.c`.

### I2C multi-byte write stall — SOLVED (Session 4)
**Symptom**: `twl: Write failed (mod 10, reg 0x13 count 5)` — GPIO pullup
bulk write (5 bytes) to TWL4030 GPIO module times out. Adds ~1s to boot.
**Root cause**: After kernel I2C driver writes `threshold` bytes to DATA register
and acks XRDY (write-1-to-clear in STAT), QEMU model never re-asserted XRDY.
Real hardware re-asserts XRDY whenever FIFO has space and bytes remain in the
transfer. Without re-assertion, the driver waits for an IRQ that never fires.
**Fix**: In STAT write-1-to-clear handler, after clearing acked bits, check if
transfer is still active with pending bytes. If so, re-set XRDY (TX) or RRDY (RX).
```c
if (s->transfer_active && s->data_pos < s->data_len) {
    if (s->is_tx) s->stat |= I2C_STAT_XRDY;
    else s->stat |= I2C_STAT_RRDY;
}
```
**Impact**: Boot time ~1.9s → ~0.94s. All TWL register writes now succeed.
**Key insight**: The OMAP I2C driver processes `threshold` bytes per XRDY interrupt,
then acks XRDY and expects it to re-appear. Module 10 = TWL GPIO, reg 0x13 =
GPIOPUPDCTR1 (base 0x98 + 0x13 = absolute 0xAB on slave 0x49).

### sDMA IRQ conflict with L0 — SOLVED (Task 027)
**Symptom**: omap-dma-engine needed a DMA controller but virtio-blk already used IRQ 12
(INTC L0). Disabling all IRQ lines caused missing DMA completion.
**Root cause**: sDMA has 4 IRQ lines (L0=12, L1=13, L2=14, L3=15). L0 conflicted with
virtio-blk. The instant-completion model doesn't need real IRQ delivery since transfers
complete synchronously, but the channel status registers (CSR) must show BLOCK_IE.
**Fix**: 32-channel instant-completion model: CCR START bit triggers immediate DMA_COMPLETE,
sets CSR, clears CCR.ENABLE. L1/L2/L3 (13-15) available for non-conflicting delivery.

### I2C3 IRQ number mismatch — SOLVED (Task 030)
**Symptom**: LM8323 keyboard driver hung in `wait_for_completion_timeout()` (1s per I2C
message x many messages in probe = visible hang after vsim regulator).
**Root cause**: `OMAP3630_IRQ_I2C3` was defined as 30, but OMAP3630 TRM Table 12-1 says
I2C3 = IRQ 61. QEMU fired IRQ 30, kernel waited on IRQ 61 -> I2C completion never arrived.
**Fix**: Changed `OMAP3630_IRQ_I2C3` from 30 to 61 in omap3630.h.

### GPIO interrupt storm — SOLVED (Task 039)
**Symptom**: Any GPIO interrupt enable caused immediate boot hang (infinite interrupt loop).
**Root cause**: GPIO periph stubs returned 1 for ALL register reads including IRQSTATUS.
Enabling a GPIO IRQ -> INTC fires bank IRQ -> handler reads IRQSTATUS=1 -> can't clear
(writes to stub are ignored) -> infinite loop.
**Fix**: Replaced 6 GPIO periph stubs with proper OMAP24XX register model (per-bank state,
W1C IRQSTATUS defaulting to 0, OE=0xFFFFFFFF, REVISION=0x0025, atomic set/clear ops for
IRQENABLE/DATAOUT/WAKE_EN, soft reset via SYSCONFIG bit 1). Each bank wired to its
dedicated INTC line (IRQ 29-34).

## Unsolved Problems

### virtio-blk FLUSH root cause
The async fsync completion is not pumped back to guest in TCG mode.
Workaround (`cache.no-flush=on`) is sufficient for development.

## Debugging Techniques
- `-d unimp` QEMU flag: shows unimplemented device accesses (offset, size, value)
- `ignore_loglevel` kernel param: reveals KERN_DEBUG initcall names
- `driver_deferred_probe_timeout=N`: limits deferred probe retries
- Boot timing analysis: `grep timestamps | awk gap analysis` (see daily/2026-02-28)
- `-d exec -D /tmp/trace.log`: log all translation blocks (can be millions of lines)

### TTY corruption from backgrounded QEMU -nographic
**Symptom**: After `kill`ing a backgrounded QEMU in `-nographic` mode, shell
returns exit code 134 (SIGABRT) or 1 on ALL commands including `echo`.
**Root cause**: QEMU `-nographic` sets terminal to raw mode (disables echo,
intercepts signals). `kill` prevents QEMU from sending ANSI reset codes to
restore TTY state. All subsequent shell I/O is garbled.
**Fix**: Blindly type `reset` + Enter. Or `stty sane`. Both restore the TTY.
**Prevention**: Use `-nographic 2>/tmp/out.log` with explicit redirection,
or use `-serial mon:stdio` instead, or run QEMU with `-monitor unix:/tmp/qemu.sock,server,nowait`
and keep the terminal clean.
