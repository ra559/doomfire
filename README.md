# DoomFire (C)

A small, dependency-free reimplementation in C of the classic **DOOM fire effect**, based on
[marcinruszkiewicz/doomfire](https://github.com/marcinruszkiewicz/doomfire) — a Ruby gem that puts your
terminal on fire using 24-bit color.

Here the effect is rewritten in C, and the full version can burn **text** into the fire: your message is
drawn with a built-in bitmap font while flames rise behind and around it.

For comparison, [r-marques/doomfire](https://github.com/r-marques/doomfire) implements the same effect in
Rust, following [Fabien Sanglard's DOOM fire write-up](https://fabiensanglard.net/doom_fire_psx/).

> No dependencies, no build system — one `.c` file and a truecolor terminal.

## Quick start

```sh
make
./doomfire -t "DOOM"
```

No `make` on hand? Compile directly:

```sh
cc -std=c11 -O2 -o doomfire doomfire.c
./doomfire -t "DOOM"
```

Press `q` or `Esc` to quit. Platform-specific details, including a Windows path, are in
[Compile and run](#compile-and-run) below.

---

## What it is

Two programs live in this repository:

| File | What it does |
| --- | --- |
| `doomfire.c` | **Full version.** The fire simulation plus centered text typography, four color palettes and font scaling. Text comes from `-t` or `-f`. |
| `complete_doomfire_base.c` | **Base version.** The first milestone: the fire effect only. No text, no font options — just flames and palette switching. |

The repository also contains a prebuilt Linux x86-64 binary named `doomfire`. It is architecture-specific,
so on macOS, Windows, or ARM Linux you should rebuild from source. If the committed binary lacks the
executable bit in your checkout, run `chmod +x doomfire` or build your own.

### How the effect works

* The screen is a grid of heat values (`0`–`36`) the size of your terminal window.
* The bottom row is re-seeded with maximum heat every frame, acting as the ember bed.
* Each cell copies its heat one row **upward**, losing a random amount (`0`–`2`) and drifting randomly
  left, right, or straight (`-1`, `0`, `+1`), clamped at the edges. Heat never goes below zero.
  This two-line rule is all it takes to produce the flickering, rising flame shape.
* Heat is mapped to RGB through a palette: the classic DOOM ramp (black → deep red → orange → yellow →
  white), or blue, green, and purple gradients.
* Every terminal cell is painted with a 24-bit **background** color; if the cell happens to fall on a
  pixel of a rendered glyph, a bright foreground block character (`█`) is printed instead of a space.

### Features

* **Text in fire** — a 5x7 bitmap font, centered horizontally and vertically, drawn over the flames.
* **Four palettes** — `-classic`, `-blue`, `-green`, `-purple`.
* **Adjustable text size** — `-s` / `--size`, from 7 to 140 (default `14`).
* **Live controls** — quit, speed up, slow down, and restart the fire from the keyboard.
* **Terminal resize aware** — the heat buffer is reallocated when the window size changes.
* **No dependencies** — C11 plus the POSIX C library; no build system, no libraries to install.
* **Cleans up after itself** — restores the cursor and leaves the alternate screen on any exit,
  including `Ctrl-C`, `SIGTERM`, and window close.

### Requirements

* A **truecolor (24-bit) terminal**: Windows Terminal, iTerm2, kitty, WezTerm, Alacritty, foot,
  GNOME Terminal/VTE, Konsole, Ghostty, and similar. Terminals limited to 256 colors will render the
  fire as flat or wrongly colored blocks.
* An **interactive TTY**. The program refuses to start when stdin or stdout is a pipe or a file,
  so redirecting its output (`./doomfire -t HI > out.txt`) will not work — by design.
* A C compiler. The source is written against POSIX (`termios.h`, `unistd.h`, `sys/ioctl.h`).

---

## Command-line options

```
doomfire [OPTIONS]
```

| Option | Description |
| --- | --- |
| `-t, --text STRING` | Text to display. A literal `\n` starts a new line. |
| `-f, --file FILE` | Read the text from a file, one screen line per file line. |
| `-classic` | Classic DOOM fire palette (default). |
| `-blue` | Blue fire palette. |
| `-green` | Green fire palette. |
| `-purple` | Purple fire palette. |
| `-s, --size SIZE` | Text size, `7`–`140` (default `14`). |
| `-h, --help` | Show usage and exit. |

Exactly one input source is required: `-t` or `-f`. Supplying neither, or both, is an error.
Note that the palette flags use a **single dash** (`-blue`), not `--blue`.

### Keyboard controls

| Key | Action |
| --- | --- |
| `q`, `Q`, `Esc` | Quit |
| `+`, `=` | Speed up (frame delay −5 ms per press) |
| `-` | Slow down (frame delay +5 ms per press) |
| `r`, `R` | Restart the fire from a blank screen |

The animation starts at one frame per 40 ms (about 25 FPS) and can be slowed to 200 ms
(5 FPS) or sped up to 5 ms (200 FPS).

### Text notes

* The bitmap font covers **A–Z, 0–9, and space**. Lowercase letters are folded to uppercase.
* Any other character prints as a blank cell, so punctuation acts as spacing.
* `-s` is quantized: the effective scale is `ceil(SIZE / 14)` cell-multiples. `-s 14` is 1x,
  `-s 28` is 2x, `-s 42` is 3x, and so on. Sizes below `14` also render at 1x; the range check
  simply keeps the value sane.
* Long text is centered, not wrapped. Text taller or wider than the terminal is clipped, so give
  the window some room or lower `-s`.

### Exit status

`0` on normal exit, `1` on a usage error (missing/invalid input, bad size, unknown option) or when no
interactive terminal is available.

---

## Compile and run

Only the C compiler is needed. Pick your platform below; the command is the same shape everywhere.

### Linux

Install a compiler if you don't have one:

```sh
# Debian / Ubuntu
sudo apt update && sudo apt install -y build-essential

# Fedora
sudo dnf install -y gcc

# Arch
sudo pacman -S --needed gcc
```

Build the full version:

```sh
cc -std=c11 -O2 -Wall -Wextra -o doomfire doomfire.c
```

Build the fire-only base version instead:

```sh
cc -std=c11 -O2 -Wall -Wextra -o doomfire-base complete_doomfire_base.c
```

The included `Makefile` wraps both commands:

```sh
make                # build doomfire and doomfire-base
make doomfire       # full version only
make doomfire-base  # fire-only version
make clean          # delete the built binaries
```

Run it:

```sh
./doomfire -t "DOOM"
./doomfire -t "STAY\nSHARP" -blue -s 28
./doomfire -f message.txt -purple
./doomfire --help
```

`gcc` and `clang` are interchangeable here; substitute whichever your distribution provides.
`-Wall -Wextra` is optional but should produce no warnings.

### macOS

Install the command-line tools once (this provides `clang` and the macOS SDK headers):

```sh
xcode-select --install
```

Build and run — the same source and flags, on both Intel and Apple Silicon:

```sh
cc -std=c11 -O2 -Wall -Wextra -o doomfire doomfire.c
./doomfire -t "HELLO" -green
```

Build the base version with:

```sh
cc -std=c11 -O2 -Wall -Wextra -o doomfire-base complete_doomfire_base.c
```

The command-line tools also install `make`, so `make` builds both programs with these flags.

**Use a truecolor terminal.** The default `Terminal.app` does not support 24-bit color, so the fire
will look wrong there. iTerm2, WezTerm, kitty, Alacritty, and Ghostty all work; run the binary from
within one of them.

> If you move the binary between machines, note that the compiled output is architecture-specific.
> Rebuild on the target machine (or build a universal binary with
> `cc -arch arm64 -arch x86_64 -std=c11 -O2 -o doomfire doomfire.c`).

### Windows

The source targets POSIX (`termios.h`, `unistd.h`, `sys/ioctl.h`), which the native Windows toolchains
do not provide. **MSVC and MinGW-w64 cannot build this program as-is.** Use one of the POSIX-capable
environments below.

#### Option 1 — WSL (recommended)

WSL gives you a real Linux userspace and terminal, so nothing needs changing.

1. Install WSL and a distribution from an elevated PowerShell or Command Prompt:

   ```powershell
   wsl --install -d Ubuntu
   ```

2. Open **Ubuntu** (or your distribution) from the Start menu and install the compiler:

   ```sh
   sudo apt update && sudo apt install -y build-essential
   ```

3. Build and run from inside WSL, exactly as on Linux:

   ```sh
   cd /mnt/c/path/to/this/project
   make
   ./doomfire -t "DOOM" -blue -s 28
   ```

   (`make` is part of `build-essential`, or run the `cc` command from the Linux section instead.)

Run it from **Windows Terminal** (the default for WSL) so you get 24-bit color and correct block
glyphs. The older `conhost.exe` window handles the effect poorly.

#### Option 2 — MSYS2 or Cygwin

Both provide the POSIX layer this source expects. With MSYS2, install the **MSYS** toolchain — the
`gcc` package from the `msys` repository, *not* the `mingw-w64` one — and build from the
"MSYS2 MSYS" shell:

```sh
pacman -S --needed gcc make
make
./doomfire -t "DOOM"
```

With Cygwin, install the `gcc-core` and `make` packages through `setup-x86_64.exe`, then run `make`
from the Cygwin terminal. In both cases run the binary from the MSYS2/Cygwin terminal (mintty), which
supports truecolor; launching it from `cmd.exe` or PowerShell will not give the program a suitable
terminal.

#### Native Windows build

A native port is possible but requires replacing the POSIX layer, specifically:

* `termios` raw-mode setup → `SetConsoleMode` (and clearing `ENABLE_LINE_INPUT`/`ENABLE_ECHO_INPUT`),
* `ioctl(TIOCGWINSZ)` → `GetConsoleScreenBufferInfo`,
* `nanosleep` → `Sleep`,
* `getline` → `fgets` or a manual reader,
* enabling ANSI escape processing with `ENABLE_VIRTUAL_TERMINAL_PROCESSING` so the color and
  cursor sequences are interpreted.

Until that port exists, use WSL or MSYS2/Cygwin.

---

## Troubleshooting

**"doomfire requires an interactive terminal."**
The program was started with its input or output redirected (a pipe, a file, or a non-interactive
runner). Run it directly in a terminal emulator.

**Flat, black, or oddly colored fire.**
The terminal is not rendering 24-bit color. Check that `COLORTERM` reports `truecolor` or `24bit`
(`echo $COLORTERM`) and that you are using a truecolor-capable terminal — not `Terminal.app`,
`conhost.exe`, or a 256-color configuration.

**The text is cut off.**
The message is centered but not wrapped. Use shorter lines, a smaller `-s`, or a larger window.

**Text characters are missing.**
Only `A–Z`, `0–9`, and space exist in the bitmap font. Punctuation renders as blank spacing, and
lowercase is uppercased automatically.

**The screen looks wrong after quitting.**
The program restores the terminal from an `atexit` handler and on `SIGINT`/`SIGTERM`/`SIGHUP`. If it
was killed with `SIGKILL`, run `reset` (or `stty sane`) to recover.

---

## Credits

* Original concept and the DOOM fire algorithm in Ruby:
  [marcinruszkiewicz/doomfire](https://github.com/marcinruszkiewicz/doomfire) (MIT licensed).
* Reference implementation in Rust:
  [r-marques/doomfire](https://github.com/r-marques/doomfire) (MIT licensed), based on
  [Fabien Sanglard's blog post](https://fabiensanglard.net/doom_fire_psx/) and rendered through minifb,
  pixels, SDL2, or WebAssembly.
* The fire effect itself originates from id Software's *DOOM* (1993) and the well-known
  "Doom fire algorithm" write-up.
* This C reimplementation and its text typography: see the source files in this repository.
