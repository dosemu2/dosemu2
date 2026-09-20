# run286 — an open loader for Phar Lap 286|DOS-Extender programs

The games Origin shipped on 286|DOS-Extender (Ultima VIII, BioForge,
Crusader) are not flat `.EXP` images. Each `.EXE` is a real-mode RUN286B
stub, followed by the extender's own `P2` image, a `DLLD` directory of the
DLLs bound into the file, and finally the program itself as a **16bit
segmented NE** marked `ne_exetyp = 0x81`. The program talks to the extender
through two DLLs in the OS/2 1.x style, PHAPI and DOSCALLS, and through
plain `int 21h`.

The real extender reads `SLDT` and writes descriptors straight into the
tables, which is why it needs a pile of workarounds in dosemu2. This is a
replacement loader that does the same job as an ordinary **DPMI client in
ring 3**: every descriptor comes from DPMI calls, nothing touches the LDT
by hand.

## What is here so far

* `neexe.[ch]` — the bound-file and NE parsers. Finds the stub, the
  extender image, the DLL directory and the program's NE; reads the segment
  table, the module reference table and the entry table. Plain NE files
  (the games' own `AILXMI.DLL`, `ASYLUM.DLL`) are handled too.
* `neload.[ch]` — applying NE relocations: internal references including
  moveable segments through the entry table, imports by ordinal and by
  name, additive fixups and fixup chains.
* `nedump.c` — a host-side harness. It lays the image out in malloc'ed
  memory, runs every relocation against a dummy backend and reports what it
  found. Build it with `make` and run `./nedump [-v] <game.exe>`.

The DPMI backend, PHAPI and DOSCALLS are next; the plan is to build the
loader itself with dj64, the way comcom64 is built.

## Status

```
$ ./nedump BIOFORGE.EXE
  real-mode stub  0..0xf71e (63262 bytes)
  extender image  0xf71e..0x339c0 (148130 bytes, 'P2')
  DLL directory   0x339c0, 3 entries: doscalls@0x33a90 phapi@0x36090 int33@0x36290
  program image   0x36f70
NE: exetyp 0x81 flags 0x9 segments 84 modules 3 align 512
relocations: 12557 records, 12557 locations patched
    unresolved 0, out of range 0
```

Same for `u8.exe` (149 segments, 12113 records) and `CRUSADER.EXE`
(145 segments, 11898 records).
