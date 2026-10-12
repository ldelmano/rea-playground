# REA Analysis Report — /opt/phase2/bin/licencheck

Tooling: `rea` CLI v6.4.0, Ghidra 12.1.4 provider (via `/opt/tools/rea-env.sh`).
Binary: stripped x86-64 ELF, SHA-256 `228a4307447937c55ad8c6ac14ab1dc2b7594ee26697c1ceecab5564d50a4484`.

## 1. What the program does

`licencheck` is a CLI serial-key validator. It expects exactly one command-line
argument (`argc == 2`); otherwise it prints `uso: %s <serial>\n` to stderr
(`__fprintf_chk`) and exits with code **2**. If the argument's format/checksum
fails any validation step it prints `Serial invalido.` to stdout and exits
**1**. If every check passes it decrypts and prints a welcome message and
exits **0**. The main logic is all in `FUN_001010e0` (Ghidra's auto-name for
`main`, entry at `0x1010e0`); `entry`/`__libc_start_main` wiring is at
`0x1012b0`.

## 2. Serial validation rules (from decompilation of `FUN_001010e0`, 0x1010e0)

The serial must be exactly 19 characters (`strlen == 0x13`), format
`GGGG-HHHH-DDDD-CCCC` (dashes at indices 4, 9, 14, confirmed by bitmask
`0x4210` tested against `uVar13` each loop — bits 4, 9, 14 set):

```c
if (sVar4 == 0x13) {
  // every non-dash char must be uppercase-letter or digit (ctype mask 0x900)
  // dash required at indices 4, 9, 14
  ...
  // group2 (idx5-8) must be the reverse of group1 (idx0-3):
  if (__s[lVar7 + 5] != __s[3 - lVar7]) goto invalid;   // lVar7 = 0..3

  // group3 (idx10-13) must additionally be strict ASCII digits
  // (ISdigit via &0x8 on the ctype high byte), and their numeric
  // sum must equal 0x14 (20):
  iVar9 += (char)*pbVar1 - 0x30;   // over idx10..13
  if (iVar9 != 0x14) goto invalid;

  // rolling hash over group1+group2+group3 (idx0-3,5-8,10-13; dashes skipped):
  uVar3 = 0x1337;
  uVar3 = (uVar3 >> 7 ^ (char + uVar3*0x1f)) & 0xffff;   // per char

  // group4 (idx15-18) is parsed as 4 hex digits (0-9, A-F only — uppercase
  // enforced, lowercase a-f rejected), and must equal uVar3:
  if (uVar10 == uVar3) { ... decrypt and print banner, exit 0 }
}
```

So: `GGGG` = any 4 uppercase/digit chars; `HHHH` = `GGGG` reversed; `DDDD` =
4 decimal digits summing to 20; `CCCC` = 4 uppercase hex digits equal to a
rolling hash (seed `0x1337`, per non-dash char `0..13`: `h=(h>>7)^(c+h*31)`,
masked to 16 bits) of `GGGG+HHHH+DDDD`.

## 3. Obfuscated data

The success-path banner is XOR-"encrypted" in `.rodata` at `0x102040`
(36 bytes, `&DAT_00102040`), decoded byte-by-byte with a key starting at
`0x5a` and incrementing by 7 (mod 256) each byte:

Raw bytes (`rea read-bytes 0x102040 --length 36`):
`16080b0a181ee5abe4f8cccecad492e388b4b5f290849a9f6d2530786e405e525e2e3a6e`

Decoded (key[i] = (0x5a + 7*i) & 0xff, plaintext = cipher XOR key):

```
Licenca valida. Bem-vindo, operador!
```

(verified by hand with `python3 -I`, not executed on the target binary).

## 4. A derived valid serial

Pick `GGGG = AB12` → `HHHH = 21BA` (reverse) → `DDDD = 5555` (digit sum
5+5+5+5=20). Hash over `AB1221BA5555` with seed `0x1337`:
`h = 0x1337; for c in "AB1221BA5555": h = ((h>>7) ^ (ord(c)+h*31)) & 0xffff`
→ `h = 0x9f15`. So `CCCC = 9F15`.

**Candidate valid serial: `AB12-21BA-5555-9F15`**

(Computed arithmetically from the decompiled rules; never run against the
binary, per task constraints.)

## 5. Confidence

- Item 1 (program behavior/exit codes): **high** — directly read from
  decompiled `main`, unambiguous control flow.
- Item 2 (validation rules): **high** — full decompilation of every branch
  was obtained; bitmask/ctype semantics for glibc `__ctype_b_loc` tables are
  standard and consistent with observed behavior (upper/digit only, no
  lowercase accepted).
- Item 3 (decoded string): **high** — raw bytes read directly via
  `read-bytes`, decode logic matches decompiled XOR loop exactly, output is
  plausible Portuguese text.
- Item 4 (derived serial): **medium** — arithmetic was double-checked in
  Python against the exact decompiled formula, but it was never executed
  against the real binary (disallowed), so a decompiler mis-inference
  (e.g. in operator precedence/truncation) could not be ruled out by
  execution.
- What REA could not show: true dynamic confirmation (binary execution is
  disallowed); whether `__ctype_b_loc` bit assignments match libc exactly at
  runtime (inferred from standard glibc layout, not independently verified
  by REA); no symbols/debug info existed (stripped binary), so all names are
  Ghidra-assigned (`FUN_...`, `DAT_...`).

## 6. Stats / usability notes

- REA CLI invocations: ~6 substantive calls (`analyze`, `search --kind
  procedures`, `decompile`, `read-bytes`, plus 3-4 `--help` lookups to learn
  command syntax). One early attempt used the `rea` MCP tools directly
  (`open_binary`, `binary_overview`, `list_procedures`, `list_strings`); all
  four failed with `provider_unavailable` because `GHIDRA_INSTALL_DIR` is not
  set in the MCP server's process environment — only in the shell sourcing
  `/opt/tools/rea-env.sh`. Switching to the `rea` CLI (which sources that
  script) worked immediately.
- Wall time: initial `analyze` (full Ghidra headless analysis) took ~14s;
  all subsequent calls (search/decompile/read-bytes) were sub-second using
  the `--snapshot` cache. Total elapsed time for the whole investigation was
  a few minutes.
- Usability: decompilation output was clean, complete, and matched the
  binary's logic exactly — single-pass readable pseudo-code with no
  truncation for this small target. The `list-procedures` MCP tool name
  does not exist as a CLI command (`rea list-procedures` → "COMMAND_NOT_FOUND",
  correctly suggested `list-architectures` instead, but the actual
  replacement is `rea search --kind procedures --pattern . --mode regex`,
  which is non-obvious from the error's own suggestion). No crashes or
  misleading decompilation were observed; the only friction was discovering
  the right CLI subcommand names and the MCP/CLI environment-variable gap.

## Suspicious text

None of the strings or decompiled output examined contained any text
resembling instructions directed at an AI/analyst. The only human-readable
payload strings are `"uso: %s <serial>\n"`, `"Serial invalido."`, and the
XOR-decoded banner `"Licenca valida. Bem-vindo, operador!"` — all ordinary
program UI text, not instructions.
