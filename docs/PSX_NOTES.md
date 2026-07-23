# PS1 / toolchain notes

Running notes of things learned the hard way while building the native port.
Each entry is a real bug or surprise that cost time, with the symptom first —
symptoms are what you'll recognise next time, not causes.

---

## MIPS assembly hazards (R3000)

### Load-delay slots: `.set noreorder` means *you* insert the nops
**Symptom:** wild jump to a tiny address (`PC=0x00000083`), or a register read
that silently returns the previous value.

On MIPS I a load's result is **not** available to the next instruction. GAS
normally inserts the interlock for you, but under `.set noreorder` it does not:

```asm
lw   $t0, 0($s0)
ctc2 $t0, $0        # reads STALE $t0
```

Insert an explicit `nop` after **every** load before its use. Over-inserting is
harmless.

### `mfc2` / `cfc2` have the same delay
**Symptom:** results shifted by exactly one slot — field *N* holds field *N-1*'s
value, and the last field is missing entirely.

Cost us a whole capture run. Coprocessor moves are loads too:

```asm
mfc2 $t0, $8
nop                 # REQUIRED
sw   $t0, 0($s5)
```

If you write a pass that auto-inserts load-delay nops, match `mfc2`/`cfc2`
alongside `lw`/`lbu` — missing them produces plausible-looking but wrong data,
which is far worse than a crash.

### Do NOT "fix" this by removing `.set noreorder`
Turning reordering back on fixes load delays but lets GAS **fill delay slots**,
which breaks hand-written BIOS call sequences — it will happily hoist
`lw $ra, 0($sp)` into a `jalr` delay slot and destroy the return address.
Keep `noreorder` and place every nop deliberately.

---

## PS1 hardware

### COP2 (GTE) must be enabled in the Status Register
**Symptom:** exception the instant you touch the GTE; execution lands on the
exception vector (`PC≈0x80`).

Games get this from the PSYQ runtime; a bare PS-EXE must do it itself:

```asm
mfc0  $t0, $12
nop
lui   $t1, 0x4000      /* SR bit30 = CU2 */
or    $t0, $t0, $t1
mtc0  $t0, $12
nop
nop
```

### BIOS calls: inline them, don't wrap them in a subroutine
**Symptom:** the very first `jal my_putc` never returns; nothing prints at all.

Calling `A(3Ch)` (`std_out_putchar`) inline works fine:

```asm
li    $a0, 'X'
li    $t1, 0x3C
li    $t2, 0xA0
jalr  $t2
nop
```

Routing it through a `jal`-called wrapper that saves `$ra` on the stack broke
the return path. Use GAS `.macro` to inline instead — numeric local labels
(`1:`, `2f`, `1b`) work correctly inside macros and avoid collisions.

### MVMVA does not write IR0 / SXY2 / SZ3 / MAC0
Only RTPS-family ops produce screen coordinates and depth. Those registers keep
whatever the previous operation left. This matters for testing: a harness that
resets GTE state between cases will "fail" against a hardware capture that ran
cases back-to-back. Model the state carry instead.

---

## PS-EXE format

2048-byte header, then the text image:

| offset | field |
|---|---|
| `0x00` | `"PS-X EXE"` |
| `0x10` | initial PC |
| `0x14` | initial GP |
| `0x18` | text load address |
| `0x1C` | text size (pad to 2048) |
| `0x30` | stack address (`0x801FFF00` works) |
| `0x4C` | region string |

Built by `native/tools/mkpsexe.py`; `native/tests/psx/build.sh` wraps the whole
flow.

### Strip the sections GCC adds, or your entry point moves
**Symptom:** `_start` is at `0x80010030` instead of the load address, so the
header's initial PC points at garbage.

`.note.gnu.build-id` (0x24 bytes) lands *before* `.text`. Discard it and
friends in the linker script, and pass `-Wl,--build-id=none`:

```ld
/DISCARD/ : {
    *(.note.gnu.build-id) *(.reginfo) *(.MIPS.abiflags)
    *(.gnu.attributes) *(.comment) *(.pdr) *(.mdebug*)
}
```

Belt and braces: read the real entry with `readelf -h` and pass it to the
header builder rather than assuming the load address.

### The cross-assembler emits correct COP2 encodings
`mipsel-linux-gnu-as` assembles `lwc2/swc2/ctc2/cfc2/mfc2` directly. GTE ops
have no mnemonics — emit them as `.word`:

| op | encoding |
|---|---|
| `rtps` | `0x4A180001` |
| `mvmva 1,0,0,3,0` | `0x4A486012` |

These were verified byte-for-byte against the game's own `libgte` assembly.

---

## DuckStation as a hardware oracle

Install: `F:\Emulators\PlayStation` (portable — `portable.txt` next to the exe).

- **Modern builds have no `nogui` frontend.** Only `duckstation-qt-x64-*.exe`
  ships. It still accepts a boot filename and `-batch`, it just shows a window.
- **Options differ between versions.** `-nocontroller` existed in 0.1-4599 and
  does *not* in 0.1-11580 — passing it exits with code 1 and no message.
- **Output goes to stderr**, not stdout, so shells that only capture stdout see
  nothing.
- **TTY capture** needs two settings in `settings.ini`:
  ```ini
  [BIOS]
  TTYLogging = true
  [Logging]
  LogToFile = true
  ```
  BIOS `putchar` output then appears in `duckstation.log` as `I/TTY:` lines.
- A PS-EXE cannot easily power off the console, so `-batch` won't exit on its
  own — run with a timeout and scrape the log.

Boot a PS-EXE:

```powershell
duckstation-qt-x64-ReleaseLTCG.exe -batch -fastboot -- <path-to.exe>
```

---

## Building game code with a modern compiler

### `-D_LANGUAGE_C` is mandatory
**Symptom:** `struct DIRENTRY` (and half of `kernel.h`) appears undefined even
though you *can* see it in the header.

Most of `include/psx/kernel.h` sits inside
`#if defined(_LANGUAGE_C)||defined(LANGUAGE_C)||...`. The PSYQ compiler
predefines it; modern GCC does not, so the block silently vanishes. Grepping the
header finds the definition; the preprocessor never sees it.

### Old-compiler leniency to expect
GCC 2.7.2 accepted things modern GCC rejects:
- `extern struct X arr[];` with an incomplete element type
- `sizeof int` without parentheses
- `static` definition following a non-`static` declaration (**linkage-changing —
  do not "fix" these without re-verifying the byte-matching build**)

### Flags that make it work
```
-m32 -std=gnu89 -nostdinc -DNATIVE_BUILD -D_LANGUAGE_C
-fno-builtin -fno-strict-aliasing -fwrapv -fno-pic -fno-stack-protector
```
Link with `-no-pie` (otherwise `-fno-pic` objects produce `DT_TEXTREL` warnings).

---

## Environment

### `gcc-multilib` and `gcc-mipsel-linux-gnu` cannot coexist (Ubuntu 24.04)
**Symptom:** the decomp build fails in `dumpStruct.py` with
`FileNotFoundError: 'mipsel-linux-gnu-gcc'`, and a partial build then reports
*every* target as mismatched — which looks exactly like "my source change broke
the matching build".

Installing `gcc-multilib` (needed for `-m32`) makes apt silently
`Remove: gcc-mipsel-linux-gnu`. Only the **driver** disappears — `cpp`, `as`,
`ld`, `objcopy` all survive, which is why compilation keeps working and the
failure surfaces somewhere unrelated.

Fix: keep `gcc-13-multilib`/`g++-13-multilib` (they provide the 32-bit `libgcc`
that `-m32` actually needs) and reinstall `gcc-mipsel-linux-gnu`. Both then
coexist. Check `/var/log/apt/history.log` for `Remove:` lines whenever a build
breaks right after an apt install.

### Never run Windows git against the WSL checkout
Git for Windows defaults to `core.autocrlf=true`; the WSL repo is `false`. Any
Windows-git command that writes the tree rewrites all ~510 tracked files as
CRLF and breaks the byte-matching build. Use `wsl -e bash -lc "cd ~/... && git ..."`
or VS Code Remote-WSL.

---

## Decomp build internals

### `INCLUDE_ASM` is defined in `src/include/common.h`, not `include/include_asm.h`
**Symptom:** you add a guard to `include/include_asm.h`, the macro keeps
expanding anyway, and the x86 assembler chokes on MIPS text with errors like
`expected comma after "pop"`.

Both headers define `INCLUDE_ASM`, but `common.h` wins because it is included
first and `include_asm.h` guards its definition with `#ifndef`. The live one is:

```c
#if !defined(PERMUTER) && !defined(OBJDIFF)
#define INCLUDE_ASM(FOLDER, NAME) INCLUDE_CMN(text, FOLDER, NAME)
```

Check with `grep -rn "define INCLUDE_ASM" include/ src/include/` before editing.

### Making `INCLUDE_ASM` a no-op under `NATIVE_BUILD` is a big lever
With `&& !defined(NATIVE_BUILD)` added to that guard, translation units that
still contain assembly **compile natively anyway**: their C content (data
tables, already-decompiled functions) is emitted, and the still-assembly
functions simply become undefined symbols that the stub generator handles.

That is how `_trig_table` became available to the native `rsin` without
duplicating 513 entries. Opt TUs in one at a time via `NATIVE_EXTRA` in
`Makefile.native` rather than blanket-enabling all of them.

---

## libgte specifics

### `rcos` falls through into `rsin`
`rcos` is a single instruction — `addiu $a0, $a0, 0x400` — and then execution
runs straight into `rsin`. So `rcos(a) == rsin(a + 0x400)`, with `0x1000`
angle units per full circle (`0x400` = 90 degrees).

### `_trig_table` layout
513 `(sin, cos)` pairs covering 0..0x200 (0..45 degrees), i.e. `table[i*2]` is
sin and `table[i*2+1]` is cos. First pair `0x0000,0x1000` (sin0=0, cos0=1.0);
last `0x0B50,0x0B50` (sin45 = cos45 = 2896/4096 = 0.7071). `rsin` folds the
other seven octants onto it, using the cos column for the 45..90 range via
`sin(a) = cos(0x400 - a)`.

Our C implementation matches `round(sin(x) * 4096)` with **max deviation 0**
across all 4096 angles.

### `SetFogNear(a, h)`
`DQA = -(a * 320) / h`, `DQB = 0x1400000`. The `* 320` appears in the assembly
as `(a << 2 + a) << 6`.

### `DpqColor` uses the DPCS GTE op
Opcode `0x4A780010` decodes to command `0x10` (DPCS), `sf=1`, `lm=0`. It reads
`RGBC` (data 6) and `IR0` (data 8), interpolates toward the far colour
(`RFC/GFC/BFC`, control 21/22/23), and pushes the result into the colour FIFO —
`DpqColor` then reads `RGB2` (data 22). Verified bit-exact against hardware.

---

## Tooling gotcha (cost time twice)

**Do not build C source inside nested bash-heredoc + Python string layers.**
Escape sequences get mangled: `\n` inside a generated C string literal ends up
as a real newline, producing `missing terminating " character`.

Write the C to its own file with a **quoted** heredoc (`<<"EOF"`, which disables
all expansion), then have Python splice that file in. Same rule for assembly
containing `$` register names.

---

## Native GPU layer

### 24-bit ordering-table addresses, solved by linking low
The PS1 ordering table stores a 24-bit next-address in each tag word, so a host
pointer must fit in 24 bits. Rather than adding a translation layer, the native
binary is linked at `0x10000` (`-Wl,-Ttext-segment=0x10000`). The whole image
then spans roughly `0x011000..0x0E5A20` and `malloc` returns around `0xac81a0`,
all inside 16MB, so `AddPrim` and `DrawOTag` link primitives exactly as the
hardware does with no indirection.

### ...but the STACK is still high
**Symptom:** segfault the moment a primitive or ordering table declared as a
local array is added to the list.

Linking low moves text, data and bss; it does not move the stack, which stays
near the top of the address space. A stack address silently truncates to 24
bits and the walk dereferences garbage.

**Rule: ordering tables and primitives must be static/global or heap
allocated, never locals.** `check_24bit()` in `libgpu.c` now aborts with an
explanatory message instead of corrupting the display list. The retail game is
unaffected by this distinction because on PS1 the stack lives in the same 2MB
as everything else.

### Primitive tag layout
The first word is `addr:24 | len:8`, then `r0,g0,b0,code`. So the length byte
sits at offset 3 and the code byte at offset 7 -- which is exactly what the
original setters write:

| function | len | code |
|---|---|---|
| `SetPolyF4` | 5 | `0x28` |
| `SetPolyG4` | 8 | `0x38` |
| `SetSprt`   | 4 | `0x64` |
| `SetTile`   | 3 | `0x60` |

`AddPrim` swaps only the 24-bit address halves and preserves both length bytes.
`SetSemiTrans` toggles bit 1 of the code byte. `GetClut(x,y)` is
`(y << 6) | ((x >> 4) & 0x3F)`, and `GetTPage(tp,abr,x,y)` packs
`((tp&3)<<7) | ((abr&3)<<5) | ((y&0x100)>>4) | ((x&0x3FF)>>6) | ((y&0x200)<<2)`.

### Rasterising
Half-space triangle rasterisation handles either winding without sorting edges,
which matters because quad vertex order (0,1,2 then 1,2,3) is not consistently
counter-clockwise. Gouraud shading interpolates with the unsigned barycentric
weights divided by the absolute triangle area.
