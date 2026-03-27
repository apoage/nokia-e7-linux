#!/usr/bin/env python3
"""Parse Symbian ROM dump (romdumpplus.dmp) - TRomHeader, file table, executables."""

import struct
import sys
import os

ROM_PATH = '/home/lukas/ps3/e7/docs/romdumpplus.dmp'
OUTPUT_PATH = '/home/lukas/ps3/e7/docs/rom-file-inventory.md'

with open(ROM_PATH, 'rb') as f:
    data = f.read()

rom_size = len(data)
print(f"ROM dump size: {rom_size} bytes ({rom_size/1024/1024:.1f} MB)")

def r8(off):
    return data[off]

def r16(off):
    return struct.unpack_from('<H', data, off)[0]

def r32(off):
    return struct.unpack_from('<I', data, off)[0]

def r64(off):
    return struct.unpack_from('<Q', data, off)[0]

def read_string_at(off, max_len=256):
    """Read null-terminated UTF-16LE string."""
    chars = []
    for i in range(max_len):
        c = r16(off + i*2)
        if c == 0:
            break
        chars.append(chr(c))
    return ''.join(chars)

# ============================================================
# TASK 1: Parse TRomHeader
# ============================================================
# The ROM starts with ARM exception vector table (0x00-0x7F)
# TRomHeader begins at offset 0x80

HDR = 0x80

print("\n=== TRomHeader (file offset 0x80) ===")

# Parse using standard EKA2 TRomHeader layout
# Reference: Symbian source e32rom.h

# First, let's identify iRomBase to calculate virt-to-file offset
# At +0x0C from header: iRomBase
rom_base = r32(HDR + 0x0C)
print(f"iRomBase = 0x{rom_base:08X}")

def virt_to_file(vaddr):
    """Convert virtual address to file offset."""
    if vaddr >= rom_base:
        return vaddr - rom_base
    return vaddr

# Dump all header fields
hdr_fields = {}

def parse_hdr():
    h = HDR
    fields = []

    # Standard TRomHeader for EKA2/Symbian^3
    off = 0

    # iVersion: TVersion (major:8, minor:8, build:16)
    vmaj = r8(h + 0)
    vmin = r8(h + 1)
    vbuild = r16(h + 2)
    fields.append((0, 'iVersion', f'{vmaj}.{vmin}.{vbuild}', r32(h)))

    # iTime: TInt64 (microseconds since 0AD)
    itime = r64(h + 4)
    fields.append((4, 'iTime', f'0x{itime:016X}', itime))

    # iRomBase
    val = r32(h + 0x0C)
    fields.append((0x0C, 'iRomBase', f'0x{val:08X}', val))

    # iRomSize
    val = r32(h + 0x10)
    fields.append((0x10, 'iRomSize', f'0x{val:08X} ({val/1024/1024:.1f} MB)', val))

    # iKernDataAddress
    val = r32(h + 0x14)
    fields.append((0x14, 'iKernDataAddress', f'0x{val:08X}', val))

    # iKernelLimit
    val = r32(h + 0x18)
    fields.append((0x18, 'iKernelLimit', f'0x{val:08X}', val))

    # iPrimaryFile (virtual addr of primary/kernel file TRomImageHeader)
    val = r32(h + 0x1C)
    fields.append((0x1C, 'iPrimaryFile', f'0x{val:08X}', val))
    hdr_fields['iPrimaryFile'] = val

    # iSecondaryFile
    val = r32(h + 0x20)
    fields.append((0x20, 'iSecondaryFile', f'0x{val:08X}', val))
    hdr_fields['iSecondaryFile'] = val

    # iCheckSum
    val = r32(h + 0x24)
    fields.append((0x24, 'iCheckSum', f'0x{val:08X}', val))

    # iHardware
    val = r32(h + 0x28)
    fields.append((0x28, 'iHardware', f'0x{val:08X}', val))

    # iLanguage (64-bit bitmask)
    val = r64(h + 0x2C)
    fields.append((0x2C, 'iLanguage', f'0x{val:016X}', val))

    # iKernelConfigFlags
    val = r32(h + 0x34)
    fields.append((0x34, 'iKernelConfigFlags', f'0x{val:08X}', val))

    # iRomExceptionSearchTable
    val = r32(h + 0x38)
    fields.append((0x38, 'iRomExceptionSearchTable', f'0x{val:08X}', val))

    # iRomHeaderSize
    val = r32(h + 0x3C)
    fields.append((0x3C, 'iRomHeaderSize', f'0x{val:08X} ({val})', val))
    hdr_fields['iRomHeaderSize'] = val

    # iRomSectionHeader
    val = r32(h + 0x40)
    fields.append((0x40, 'iRomSectionHeader', f'0x{val:08X}', val))

    # iTotalSvDataSize
    val = r32(h + 0x44)
    fields.append((0x44, 'iTotalSvDataSize', f'0x{val:08X} ({val})', val))

    # iVariantFile
    val = r32(h + 0x48)
    fields.append((0x48, 'iVariantFile', f'0x{val:08X}', val))
    hdr_fields['iVariantFile'] = val

    # iExtensionFile
    val = r32(h + 0x4C)
    fields.append((0x4C, 'iExtensionFile', f'0x{val:08X}', val))
    hdr_fields['iExtensionFile'] = val

    # iRelocInfo
    val = r32(h + 0x50)
    fields.append((0x50, 'iRelocInfo', f'0x{val:08X}', val))

    # iOldTraceMask
    val = r32(h + 0x54)
    fields.append((0x54, 'iOldTraceMask', f'0x{val:08X}', val))

    # iUserDataAddress
    val = r32(h + 0x58)
    fields.append((0x58, 'iUserDataAddress', f'0x{val:08X}', val))

    # iTotalUserDataSize
    val = r32(h + 0x5C)
    fields.append((0x5C, 'iTotalUserDataSize', f'0x{val:08X} ({val})', val))

    # iDebugPort
    val = r32(h + 0x60)
    fields.append((0x60, 'iDebugPort', f'0x{val:08X} ({val})', val))

    # More fields after 0x64
    # iCompressInfo at various offsets depending on version
    # iRomRootDirectoryList
    # Let's scan for it - it should be a virtual address pointing into ROM

    # Extended header fields (offset varies by Symbian version)
    # In Symbian^3, the header is quite large. Let's dump remaining fields.
    for extra_off in range(0x64, 0x100, 4):
        val = r32(h + extra_off)
        if val != 0:
            fields.append((extra_off, f'field_{extra_off:02X}', f'0x{val:08X} ({val})', val))

    return fields

hdr_result = parse_hdr()
for off, name, display, raw in hdr_result:
    print(f"  +0x{off:02X} {name:30s} = {display}")

# ============================================================
# Find iRomRootDirectoryList
# ============================================================
# The root directory list pointer is typically at a known offset in the header.
# In EKA2, it's at offset 0xC4 from the start of TRomHeader.
# But our header starts at 0x80, so absolute file offset = 0x80 + 0xC4 = 0x144

# Let's check several possible offsets for the root directory list
print("\n=== Searching for iRomRootDirectoryList ===")

# The RomRootDirectoryList should be a virtual address in ROM space
# It typically appears in the header area. Let's check likely offsets.
# In the hex dump, at offset 0x148 (header+0xC8) we see 0x00aa4000
# which would be file offset 0x00aa4000 if rom_base=0x80000000
# -> virtual addr 0x80aa4000

# Let's look for TRomRootDirectoryList signature:
# It starts with a count of directory entries, then entries follow.
# Each entry has iAddressLin (virtual addr of TRomDir) and iDir (UTF16 drive letter path)

# Scan header for plausible root dir list pointers
for off in range(0xC0, 0x180, 4):
    val = r32(HDR + off)
    if rom_base <= val < rom_base + rom_size:
        foff = virt_to_file(val)
        # Check if this could be a root directory list
        # First field would be size/count
        if foff < rom_size - 4:
            first_word = r32(foff)
            print(f"  Header+0x{off:02X}: 0x{val:08X} -> file 0x{foff:06X}, first word: 0x{first_word:08X}")

# Also check the specific value at header+0xC8 area
print(f"\n  At header+0xC4 (0x144): 0x{r32(HDR+0xC4):08X}")
print(f"  At header+0xC8 (0x148): 0x{r32(HDR+0xC8):08X}")

# ============================================================
# TASK 2: Scan for E32 image headers (E32ImageHeader)
# ============================================================
# Instead of (or in addition to) the directory approach, we can scan for
# E32 executable signatures in the ROM.
#
# TRomImageHeader has a known structure. But let's first try to find
# E32 executables by their UID pattern.
#
# Symbian UIDs:
#   UID1 for EXE = 0x1000007A
#   UID1 for DLL = 0x10000079
#   UID1 for LDD = 0x100000AF (logical device driver)
#   UID1 for PDD = 0x100039D0 (physical device driver)

print("\n=== Scanning for E32 executables by UID1 ===")

uid_types = {
    0x1000007A: 'EXE',
    0x10000079: 'DLL',
    0x100000AF: 'LDD',
    0x100039D0: 'PDD',
}

# In ROM images, the TRomImageHeader precedes the actual code.
# TRomImageHeader layout (simplified):
# +0x00: iUid1 (4 bytes)
# +0x04: iUid2 (4 bytes)
# +0x08: iUid3 (4 bytes)
# +0x0C: iUidChecksum (4 bytes)
# +0x10: iEntryPoint (4 bytes, virtual)
# +0x14: iCodeAddress (4 bytes, virtual)
# +0x18: iDataAddress (4 bytes, virtual)
# +0x1C: iCodeSize (4 bytes)
# +0x20: iTextSize (4 bytes)
# +0x24: iDataSize (4 bytes)
# +0x28: iBssSize (4 bytes)
# +0x2C: iHeapSizeMin (4 bytes)
# +0x30: iHeapSizeMax (4 bytes)
# +0x34: iStackSize (4 bytes)
# +0x38: iDllRefTable (4 bytes, virtual)
# +0x3C: iExportDirCount (4 bytes)
# +0x40: iExportDir (4 bytes, virtual)
# +0x44: iS (TSecurityInfo - capabilities etc.)
# ... more fields

# Scan entire ROM for UID1 values
exe_entries = []

for uid1_val, uid_type in uid_types.items():
    # Pack the UID value to search
    needle = struct.pack('<I', uid1_val)
    pos = 0
    count = 0
    while True:
        pos = data.find(needle, pos)
        if pos == -1:
            break

        # Validate: check if this looks like a real header
        # iUid2 and iUid3 should be reasonable values
        if pos + 0x80 <= rom_size:
            uid2 = r32(pos + 4)
            uid3 = r32(pos + 8)
            uid_check = r32(pos + 0x0C)
            entry_point = r32(pos + 0x10)
            code_addr = r32(pos + 0x14)
            code_size = r32(pos + 0x1C)

            # Sanity checks:
            # - entry_point should be in ROM range
            # - code_size should be reasonable (< 10MB)
            # - code_addr should be in ROM range
            valid = True
            if code_size > 10*1024*1024 or code_size == 0:
                valid = False
            if entry_point != 0 and not (rom_base <= entry_point < rom_base + rom_size):
                valid = False
            if code_addr != 0 and not (rom_base <= code_addr < rom_base + rom_size):
                # Could be RAM address for data
                if not (0xC0000000 <= code_addr < 0xD0000000):  # Typical Symbian RAM range
                    if code_addr > 0x1000:  # Allow small/zero
                        valid = False

            if valid:
                exe_entries.append({
                    'offset': pos,
                    'vaddr': pos + rom_base,
                    'uid1': uid1_val,
                    'uid2': uid2,
                    'uid3': uid3,
                    'uid_check': uid_check,
                    'type': uid_type,
                    'entry_point': entry_point,
                    'code_addr': code_addr,
                    'code_size': code_size,
                    'data_size': r32(pos + 0x24),
                    'bss_size': r32(pos + 0x28),
                })
                count += 1
        pos += 4
    print(f"  {uid_type} (0x{uid1_val:08X}): {count} found")

print(f"  Total: {len(exe_entries)} executables")

# ============================================================
# Find the ROM directory structure
# ============================================================
# TRomDir starts with iSize (4 bytes) then iCount/entries
# A TRomEntry has:
#   iSize (2 bytes) - size of this entry
#   iAddressLin (4 bytes) - virtual address
#   iAtt (1 byte) - attributes
#   iNameLength (1 byte) - name length in characters
#   iName[] - UTF-16LE name

print("\n=== Searching for ROM Directory Structure ===")

# The root directory list format:
# TRomRootDirectoryList:
#   iNumRootDirs (4 bytes)
#   entries[]: { iHardwareVariant(4), iAddressLin(4) }

# Let's try offset 0xC8 from header (common in Symbian^3)
# From the hex: at file 0x148 we have values 0x00aa4000 and 0x013ada50

# Actually let me look more carefully at the header structure
# The fields at various offsets:
print("Scanning header for directory list candidates...")

root_dir_list_addr = None

# Try to find by checking each ROM-range pointer in the header
# and seeing if it points to a valid TRomRootDirectoryList
for off in range(0x00, 0x100, 4):
    val = r32(HDR + off)
    if val == 0:
        continue
    if not (rom_base <= val < rom_base + rom_size):
        continue
    foff = virt_to_file(val)
    if foff >= rom_size - 16:
        continue

    # Check if this could be a TRomRootDirectoryList
    num_dirs = r32(foff)
    if 1 <= num_dirs <= 10:  # Reasonable number of root dirs (z:, c:, etc.)
        # Check entries
        all_valid = True
        for i in range(num_dirs):
            entry_off = foff + 4 + i * 8
            hw_variant = r32(entry_off)
            dir_addr = r32(entry_off + 4)
            if dir_addr != 0 and not (rom_base <= dir_addr < rom_base + rom_size):
                all_valid = False
                break
        if all_valid:
            print(f"  Candidate at header+0x{off:02X} = 0x{val:08X}: {num_dirs} root dirs")
            root_dir_list_addr = val
            # Print entries
            for i in range(num_dirs):
                entry_off = foff + 4 + i * 8
                hw_variant = r32(entry_off)
                dir_addr = r32(entry_off + 4)
                print(f"    Dir {i}: hwVariant=0x{hw_variant:08X}, addr=0x{dir_addr:08X}")

# ============================================================
# Parse ROM directory tree recursively
# ============================================================
all_files = []

def parse_rom_dir(dir_vaddr, path=""):
    """Parse a TRomDir structure and return file entries."""
    foff = virt_to_file(dir_vaddr)
    if foff >= rom_size - 4:
        return

    # TRomDir:
    # +0x00: iSize (4 bytes) - total size of this directory block
    # +0x04: entries start
    # Each TRomEntry:
    #   +0x00: iSize (2 bytes) - size of this entry
    #   +0x02: iAddressLin (4 bytes) - OR could be different layout
    # Actually in Symbian, TRomDir = { iSize, then array of TRomEntry }
    # TRomEntry = { iSize:16, iAddressLin:32, iAtt:8, iNameLength:8, iName[]:UTF16 }
    # Wait - let me check the actual struct layout more carefully.

    # TRomEntry:
    #   TInt iSize;       // 4 bytes: size of entry including name
    #   TLinAddr iAddressLin; // 4 bytes: virtual address of file/subdir
    #   TUint8 iAtt;      // 1 byte: attributes (KEntryAttDir=0x10 for dir)
    #   TUint8 iNameLength; // 1 byte: name length in chars
    #   TUint16 iName[];  // UTF-16LE name, padded to 4-byte boundary

    dir_size = r32(foff)
    if dir_size == 0 or dir_size > 0x100000:  # Sanity
        return

    # Also: TRomDir has iSize (size of file entries area) then iSortedTable maybe
    # Actually the exact layout: TRomDir::iSize tells total size of entries part

    pos = foff + 4  # Skip iSize
    end = foff + 4 + dir_size

    while pos < end and pos < rom_size - 12:
        entry_size = r32(pos)
        if entry_size < 12 or entry_size > 0x10000:
            break

        addr_lin = r32(pos + 4)
        att = r8(pos + 8)
        name_len = r8(pos + 9)

        if name_len == 0 or name_len > 128:
            break

        # Read name (UTF-16LE)
        name_chars = []
        for i in range(name_len):
            if pos + 10 + i*2 + 1 < rom_size:
                c = r16(pos + 10 + i*2)
                name_chars.append(chr(c))
        name = ''.join(name_chars)

        full_path = path + name if path else name

        is_dir = (att & 0x10) != 0

        if is_dir:
            # Recurse into subdirectory
            if rom_base <= addr_lin < rom_base + rom_size:
                parse_rom_dir(addr_lin, full_path + "\\")
        else:
            # File entry - addr_lin points to TRomImageHeader for executables
            # or TRomEntry for regular files
            file_info = {
                'path': full_path,
                'addr_lin': addr_lin,
                'att': att,
                'entry_offset': pos,
            }

            # Try to read file header to determine type
            if rom_base <= addr_lin < rom_base + rom_size:
                file_foff = virt_to_file(addr_lin)
                if file_foff + 0x10 < rom_size:
                    uid1 = r32(file_foff)
                    uid2 = r32(file_foff + 4)
                    uid3 = r32(file_foff + 8)
                    file_info['uid1'] = uid1
                    file_info['uid2'] = uid2
                    file_info['uid3'] = uid3

                    if uid1 in uid_types:
                        file_info['exe_type'] = uid_types[uid1]
                        # Read more E32 header fields
                        file_info['entry_point'] = r32(file_foff + 0x10)
                        file_info['code_addr'] = r32(file_foff + 0x14)
                        file_info['code_size'] = r32(file_foff + 0x1C)
                        file_info['text_size'] = r32(file_foff + 0x20)
                        file_info['data_size'] = r32(file_foff + 0x24)
                        file_info['bss_size'] = r32(file_foff + 0x28)
                        file_info['heap_min'] = r32(file_foff + 0x2C)
                        file_info['heap_max'] = r32(file_foff + 0x30)
                        file_info['stack_size'] = r32(file_foff + 0x34)
                        file_info['dll_ref_table'] = r32(file_foff + 0x38)
                        file_info['export_dir_count'] = r32(file_foff + 0x3C)
                        file_info['export_dir'] = r32(file_foff + 0x40)

                        # TSecurityInfo starts at +0x44
                        # iSecureId at +0x44
                        # iVendorId at +0x48
                        # iCaps at +0x4C (two 32-bit words = 64-bit capability bitmask)
                        file_info['secure_id'] = r32(file_foff + 0x44)
                        file_info['vendor_id'] = r32(file_foff + 0x48)
                        file_info['caps_lo'] = r32(file_foff + 0x4C)
                        file_info['caps_hi'] = r32(file_foff + 0x50)

            all_files.append(file_info)

        # Advance to next entry (entries are 4-byte aligned)
        pos += (entry_size + 3) & ~3

if root_dir_list_addr:
    foff = virt_to_file(root_dir_list_addr)
    num_dirs = r32(foff)
    for i in range(num_dirs):
        entry_off = foff + 4 + i * 8
        hw_variant = r32(entry_off)
        dir_addr = r32(entry_off + 4)
        if rom_base <= dir_addr < rom_base + rom_size:
            parse_rom_dir(dir_addr, "")

print(f"\nFiles found via directory walk: {len(all_files)}")

# If directory walk didn't find much, also try a different approach
# ============================================================
# Alternative: scan for TRomImageHeader structures
# ============================================================
# TRomImageHeader has a specific pattern:
# The iCodeAddress field points into ROM, and there's a consistent layout

# Let's also try to find files by scanning for common filenames in UTF-16LE
if len(all_files) < 10:
    print("\nDirectory walk found few files, trying filename scan...")

    # Search for ".exe\0" in UTF-16LE
    for ext in ['.exe', '.dll', '.ldd', '.pdd']:
        needle = ext.encode('utf-16-le')
        pos = 0
        count = 0
        while True:
            pos = data.find(needle, pos)
            if pos == -1:
                break
            # Try to find the start of the filename (scan backwards for non-ASCII)
            name_start = pos
            for back in range(128):
                if pos - (back+1)*2 < 0:
                    break
                c = r16(pos - (back+1)*2)
                if c < 0x20 or c > 0x7E:
                    break
                name_start = pos - (back+1)*2

            name_len = (pos + len(needle) - name_start) // 2
            name = ''
            for i in range(name_len):
                name += chr(r16(name_start + i*2))

            if name and count < 3:
                pass  # Don't spam
            count += 1
            pos += 2
        print(f"  '{ext}' occurrences: {count}")

# ============================================================
# Let me try another approach: scan for the TRomEntry pattern
# The directory might use a different layout than I assumed
# ============================================================

# Let's look for the string "sys" or "system" in UTF-16LE near potential directory areas
print("\n=== Looking for directory structures via string patterns ===")

# Search for "sys\\" in UTF-16LE
for search_str in ["sys\\", "system\\", "private\\", "resource\\"]:
    needle = search_str.encode('utf-16-le')
    pos = data.find(needle)
    while pos != -1 and pos < rom_size:
        # Print context
        # Look back for entry header
        context_start = max(0, pos - 16)
        print(f"  '{search_str}' found at 0x{pos:06X}")
        # Show surrounding data
        break  # Just first occurrence
        pos = data.find(needle, pos + 2)

# ============================================================
# Better approach: find all filenames ending in .exe/.dll/.ldd
# by scanning for the UTF-16LE patterns and extracting full names
# ============================================================

print("\n=== Scanning for all executable filenames ===")

def find_all_filenames():
    """Find all filenames in the ROM by scanning for extensions in UTF-16LE."""
    results = []

    for ext in ['.exe', '.dll', '.ldd', '.pdd', '.fsy', '.nif', '.csy', '.tsy', '.prt', '.drv']:
        # Null-terminated version: ext + \x00\x00
        needle = ext.encode('utf-16-le')
        pos = 0
        while True:
            pos = data.find(needle, pos)
            if pos == -1:
                break

            # Check if followed by null terminator or non-alpha
            after = pos + len(needle)
            is_end = True
            if after + 1 < rom_size:
                next_char = r16(after)
                if 0x20 < next_char < 0x7F:
                    is_end = False  # Part of longer name

            if is_end:
                # Scan backwards to find start of name
                name_start = pos
                for back in range(256):
                    check_pos = pos - (back + 1) * 2
                    if check_pos < 0:
                        break
                    c = r16(check_pos)
                    # Valid filename chars (including path separators)
                    if c < 0x20 or c > 0x7E:
                        break
                    name_start = check_pos

                # Read the full name
                name_end = pos + len(needle)
                name = ''
                p = name_start
                while p < name_end:
                    c = r16(p)
                    name += chr(c)
                    p += 2

                if len(name) > 2 and len(name) < 200:
                    results.append((name_start, name))

            pos += 2

    return results

filenames = find_all_filenames()
print(f"Found {len(filenames)} filename occurrences")

# Deduplicate by name
unique_names = {}
for fpos, name in filenames:
    name_lower = name.lower()
    if name_lower not in unique_names:
        unique_names[name_lower] = (fpos, name)

print(f"Unique filenames: {len(unique_names)}")

# Sort and print
sorted_names = sorted(unique_names.items())
for name_lower, (fpos, name) in sorted_names[:20]:
    print(f"  0x{fpos:06X}: {name}")
if len(sorted_names) > 20:
    print(f"  ... and {len(sorted_names)-20} more")

# ============================================================
# Now let's try to match filenames to their TRomImageHeader
# ============================================================
# In the ROM directory structure, each file entry has a name AND
# an address pointing to the TRomImageHeader. We need to find
# the connection.

# Strategy: For each filename location, look nearby for a
# TRomEntry-like structure (the name is part of the entry).
# Then extract the iAddressLin from the entry.

print("\n=== Matching filenames to image headers ===")

file_catalog = []

for name_lower, (name_fpos, name) in sorted(unique_names.items()):
    # The TRomEntry structure has the name at the end.
    # Before the name, there's:
    #   iSize (4 bytes or 2 bytes)
    #   iAddressLin (4 bytes)
    #   iAtt (1 byte)
    #   iNameLength (1 byte)
    # So the name is at entry_start + 10 (for 4-byte iSize) or entry_start + 8 (for 2-byte iSize)

    name_len_chars = len(name)

    # Try: name_length byte right before the name
    # Layout might be: ... iAtt(1) iNameLength(1) iName[]
    # So 2 bytes before name start should be the name length
    if name_fpos >= 2:
        stored_len = r8(name_fpos - 1)
        att_byte = r8(name_fpos - 2)

        if stored_len == name_len_chars and att_byte in [0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x21, 0x22]:
            # Looks valid! Now get iAddressLin (4 bytes before att+namelen)
            if name_fpos >= 6:
                addr_lin = r32(name_fpos - 6)
                entry_size = r32(name_fpos - 10) if name_fpos >= 10 else 0

                entry = {
                    'name': name,
                    'name_offset': name_fpos,
                    'addr_lin': addr_lin,
                    'att': att_byte,
                    'entry_size_field': entry_size,
                }

                # Check if addr_lin points to a valid TRomImageHeader
                if rom_base <= addr_lin < rom_base + rom_size:
                    img_foff = virt_to_file(addr_lin)
                    if img_foff + 0x60 < rom_size:
                        uid1 = r32(img_foff)
                        uid2 = r32(img_foff + 4)
                        uid3 = r32(img_foff + 8)

                        entry['uid1'] = uid1
                        entry['uid2'] = uid2
                        entry['uid3'] = uid3

                        if uid1 in uid_types:
                            entry['exe_type'] = uid_types[uid1]
                            entry['entry_point'] = r32(img_foff + 0x10)
                            entry['code_addr'] = r32(img_foff + 0x14)
                            entry['code_size'] = r32(img_foff + 0x1C)
                            entry['text_size'] = r32(img_foff + 0x20)
                            entry['data_size'] = r32(img_foff + 0x24)
                            entry['bss_size'] = r32(img_foff + 0x28)
                            entry['heap_min'] = r32(img_foff + 0x2C)
                            entry['heap_max'] = r32(img_foff + 0x30)
                            entry['stack_size'] = r32(img_foff + 0x34)
                            entry['dll_ref_table'] = r32(img_foff + 0x38)
                            entry['export_dir_count'] = r32(img_foff + 0x3C)
                            entry['export_dir'] = r32(img_foff + 0x40)
                            entry['secure_id'] = r32(img_foff + 0x44)
                            entry['vendor_id'] = r32(img_foff + 0x48)
                            entry['caps_lo'] = r32(img_foff + 0x4C)
                            entry['caps_hi'] = r32(img_foff + 0x50)
                        else:
                            entry['exe_type'] = 'DATA/OTHER'

                file_catalog.append(entry)

print(f"Cataloged {len(file_catalog)} files with directory entries")

# Count by type
type_counts = {}
for f in file_catalog:
    t = f.get('exe_type', 'UNKNOWN')
    type_counts[t] = type_counts.get(t, 0) + 1
print("By type:", type_counts)

# ============================================================
# TASK 3 & 4: Extract details for LDDs and specific files
# ============================================================

print("\n=== LDD Files (Kernel-mode drivers) ===")
ldds = [f for f in file_catalog if f.get('exe_type') == 'LDD']
for f in sorted(ldds, key=lambda x: x['name'].lower()):
    caps = (f.get('caps_hi', 0) << 32) | f.get('caps_lo', 0)
    print(f"  {f['name']:40s} UID3=0x{f.get('uid3',0):08X} SID=0x{f.get('secure_id',0):08X} Caps=0x{caps:010X} Code={f.get('code_size',0):6d} Exports={f.get('export_dir_count',0)}")

print("\n=== PDD Files (Physical device drivers) ===")
pdds = [f for f in file_catalog if f.get('exe_type') == 'PDD']
for f in sorted(pdds, key=lambda x: x['name'].lower()):
    caps = (f.get('caps_hi', 0) << 32) | f.get('caps_lo', 0)
    print(f"  {f['name']:40s} UID3=0x{f.get('uid3',0):08X} SID=0x{f.get('secure_id',0):08X} Caps=0x{caps:010X} Code={f.get('code_size',0):6d} Exports={f.get('export_dir_count',0)}")

print("\n=== Specific files of interest ===")
interest_patterns = ['ekern', 'efile', 'estart', 'variant', 'assp', 'bcm', 'broadcom', '2727', 'patcher', 'memoryaccess', 'rompatch', 'capss']
for f in sorted(file_catalog, key=lambda x: x['name'].lower()):
    name_l = f['name'].lower()
    for pat in interest_patterns:
        if pat in name_l:
            caps = (f.get('caps_hi', 0) << 32) | f.get('caps_lo', 0)
            print(f"  {f['name']:40s} Type={f.get('exe_type','?'):4s} UID3=0x{f.get('uid3',0):08X} SID=0x{f.get('secure_id',0):08X} Caps=0x{caps:010X} Code={f.get('code_size',0):6d}")
            break

# ============================================================
# EXE files
# ============================================================
print("\n=== EXE Files ===")
exes = [f for f in file_catalog if f.get('exe_type') == 'EXE']
for f in sorted(exes, key=lambda x: x['name'].lower()):
    caps = (f.get('caps_hi', 0) << 32) | f.get('caps_lo', 0)
    print(f"  {f['name']:40s} UID3=0x{f.get('uid3',0):08X} SID=0x{f.get('secure_id',0):08X} Caps=0x{caps:010X} Code={f.get('code_size',0):6d} Stack={f.get('stack_size',0):6d}")

# ============================================================
# Generate output markdown
# ============================================================

out_lines = []
out_lines.append("# Nokia E7 ROM File Inventory")
out_lines.append("")
out_lines.append(f"**Source**: `romdumpplus.dmp` ({rom_size:,} bytes, {rom_size/1024/1024:.1f} MB)")
out_lines.append(f"**Date parsed**: 2026-03-24")
out_lines.append(f"**ROM base address**: 0x{rom_base:08X}")
rom_size_field = r32(HDR + 0x10)
out_lines.append(f"**ROM size (header)**: 0x{rom_size_field:08X} ({rom_size_field/1024/1024:.1f} MB)")
out_lines.append("")

# TRomHeader
out_lines.append("## TRomHeader")
out_lines.append("")
out_lines.append("| Offset | Field | Value |")
out_lines.append("|--------|-------|-------|")
for off, name, display, raw in hdr_result:
    out_lines.append(f"| +0x{off:02X} | {name} | {display} |")
out_lines.append("")

# Summary stats
out_lines.append("## Summary")
out_lines.append("")
out_lines.append(f"- **Total files cataloged**: {len(file_catalog)}")
for t in sorted(type_counts.keys()):
    out_lines.append(f"  - {t}: {type_counts[t]}")
out_lines.append("")

# EXE files
out_lines.append("## EXE Files")
out_lines.append("")
out_lines.append("| Name | UID3 | SecureID | Capabilities | CodeSize | Stack | Exports |")
out_lines.append("|------|------|----------|-------------|----------|-------|---------|")
for f in sorted(exes, key=lambda x: x['name'].lower()):
    caps = (f.get('caps_hi', 0) << 32) | f.get('caps_lo', 0)
    out_lines.append(f"| {f['name']} | 0x{f.get('uid3',0):08X} | 0x{f.get('secure_id',0):08X} | 0x{caps:010X} | {f.get('code_size',0)} | {f.get('stack_size',0)} | {f.get('export_dir_count',0)} |")
out_lines.append("")

# DLL files
dlls = [f for f in file_catalog if f.get('exe_type') == 'DLL']
out_lines.append("## DLL Files")
out_lines.append("")
out_lines.append("| Name | UID3 | SecureID | Capabilities | CodeSize | Exports |")
out_lines.append("|------|------|----------|-------------|----------|---------|")
for f in sorted(dlls, key=lambda x: x['name'].lower()):
    caps = (f.get('caps_hi', 0) << 32) | f.get('caps_lo', 0)
    out_lines.append(f"| {f['name']} | 0x{f.get('uid3',0):08X} | 0x{f.get('secure_id',0):08X} | 0x{caps:010X} | {f.get('code_size',0)} | {f.get('export_dir_count',0)} |")
out_lines.append("")

# LDD files (detailed)
out_lines.append("## LDD Files (Logical Device Drivers)")
out_lines.append("")
out_lines.append("| Name | UID2 | UID3 | SecureID | VendorID | Capabilities | CodeSize | DataSize | Exports |")
out_lines.append("|------|------|------|----------|----------|-------------|----------|----------|---------|")
for f in sorted(ldds, key=lambda x: x['name'].lower()):
    caps = (f.get('caps_hi', 0) << 32) | f.get('caps_lo', 0)
    out_lines.append(f"| {f['name']} | 0x{f.get('uid2',0):08X} | 0x{f.get('uid3',0):08X} | 0x{f.get('secure_id',0):08X} | 0x{f.get('vendor_id',0):08X} | 0x{caps:010X} | {f.get('code_size',0)} | {f.get('data_size',0)} | {f.get('export_dir_count',0)} |")
out_lines.append("")

# PDD files
out_lines.append("## PDD Files (Physical Device Drivers)")
out_lines.append("")
out_lines.append("| Name | UID2 | UID3 | SecureID | VendorID | Capabilities | CodeSize | DataSize | Exports |")
out_lines.append("|------|------|------|----------|----------|-------------|----------|----------|---------|")
for f in sorted(pdds, key=lambda x: x['name'].lower()):
    caps = (f.get('caps_hi', 0) << 32) | f.get('caps_lo', 0)
    out_lines.append(f"| {f['name']} | 0x{f.get('uid2',0):08X} | 0x{f.get('uid3',0):08X} | 0x{f.get('secure_id',0):08X} | 0x{f.get('vendor_id',0):08X} | 0x{caps:010X} | {f.get('code_size',0)} | {f.get('data_size',0)} | {f.get('export_dir_count',0)} |")
out_lines.append("")

# Files of interest
out_lines.append("## Files of Interest")
out_lines.append("")
out_lines.append("### Kernel and Boot")
out_lines.append("")
for f in sorted(file_catalog, key=lambda x: x['name'].lower()):
    name_l = f['name'].lower()
    if any(p in name_l for p in ['ekern', 'efile', 'estart', 'euser', 'elocd']):
        caps = (f.get('caps_hi', 0) << 32) | f.get('caps_lo', 0)
        out_lines.append(f"- **{f['name']}**: Type={f.get('exe_type','?')}, UID3=0x{f.get('uid3',0):08X}, SID=0x{f.get('secure_id',0):08X}, Caps=0x{caps:010X}, Code={f.get('code_size',0)}")
out_lines.append("")

out_lines.append("### Variant/ASSP (SoC-specific)")
out_lines.append("")
found_variant = False
for f in sorted(file_catalog, key=lambda x: x['name'].lower()):
    name_l = f['name'].lower()
    if any(p in name_l for p in ['variant', 'assp', 'bcm', 'broadcom', '2727', '2763']):
        caps = (f.get('caps_hi', 0) << 32) | f.get('caps_lo', 0)
        out_lines.append(f"- **{f['name']}**: Type={f.get('exe_type','?')}, UID3=0x{f.get('uid3',0):08X}, SID=0x{f.get('secure_id',0):08X}, Caps=0x{caps:010X}, Code={f.get('code_size',0)}")
        found_variant = True
if not found_variant:
    out_lines.append("- None found with these name patterns")
out_lines.append("")

out_lines.append("### Patcher/MemoryAccess/Security")
out_lines.append("")
found_sec = False
for f in sorted(file_catalog, key=lambda x: x['name'].lower()):
    name_l = f['name'].lower()
    if any(p in name_l for p in ['patcher', 'memoryaccess', 'rompatch', 'capss', 'capsw']):
        caps = (f.get('caps_hi', 0) << 32) | f.get('caps_lo', 0)
        out_lines.append(f"- **{f['name']}**: Type={f.get('exe_type','?')}, UID3=0x{f.get('uid3',0):08X}, SID=0x{f.get('secure_id',0):08X}, Caps=0x{caps:010X}, Code={f.get('code_size',0)}")
        found_sec = True
if not found_sec:
    out_lines.append("- None found in ROM (these may only exist as SIS-installed files)")
out_lines.append("")

# DATA/OTHER files
others = [f for f in file_catalog if f.get('exe_type') in ('DATA/OTHER', 'UNKNOWN') or 'exe_type' not in f]
if others:
    out_lines.append("## Other Files (non-executable)")
    out_lines.append("")
    out_lines.append("| Name | Address | Attribute |")
    out_lines.append("|------|---------|-----------|")
    for f in sorted(others, key=lambda x: x['name'].lower()):
        out_lines.append(f"| {f['name']} | 0x{f.get('addr_lin',0):08X} | 0x{f.get('att',0):02X} |")
    out_lines.append("")

# Write output
with open(OUTPUT_PATH, 'w') as out:
    out.write('\n'.join(out_lines))

print(f"\n=== Output written to {OUTPUT_PATH} ===")
print(f"Total files: {len(file_catalog)}")
