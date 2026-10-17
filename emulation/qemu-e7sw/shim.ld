/* shim.ld — the SWBL-role shim at the bottom of the 64 KiB SRAM. */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(shim_entry)

SECTIONS
{
    . = 0x40200000;
    .text : {
        KEEP(*(.text.shim_entry))
        *(.text*)
        *(.rodata*)
        *(.data*)
    }
    /DISCARD/ : { *(.comment) *(.ARM.attributes) *(.eh_frame) }
}
