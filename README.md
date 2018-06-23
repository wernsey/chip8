# CHIP-8 Interpreter, Assembler and Disassembler

This package contains an interpreter for
[CHIP-8](https://en.wikipedia.org/wiki/CHIP-8) as well as a command-line
assembler and disassembler.

It also supports the SuperChip instructions.

The syntax of the assembler and disassembler is based on the syntax described
in [Cowgod's Chip-8 Technical Reference
v1.0](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM), by Thomas P. Greene

Frédéric Devernay's [SVision-8 website](http://devernay.free.fr/hacks/chip8/)
has a wealth of information. He also has a collection of CHIP-8 games and
programs in his [GAMES.zip](http://devernay.free.fr/hacks/chip8/GAMES.zip).

## Compilation and Usage

* Linux: Type `make` from the shell.
* Windows: The system was built and tested with the
  [MinGW](http://www.mingw.org/) tools. To compile it type `make` from the MSYS
  shell.

To use the emulator:

* Under Linux: Type `chip8 game.ch8` where game.ch8 is the binary CHIP-8 file.
* Under Windows: Type `chip8 game.ch8` or `chip8-gdi game.ch8` depending on
  which of the implementations (see below) you want to use.

The assembler and disassemblers are simple command line applications and
platform independent.

To use the assembler, type

    $ c8asm -o file.c8h file.asm

This will assemble `file.asm` into a binary `file.c8h`. If the `-o` is not
specified it will default to `a.c8h`.

To use the disassembler, run the command
	
    $ c8dasm a.ch8 > outfile.asm

where `a.ch8` is the file you want to disassemble.

## Interpreter Implementations

The core of the emulator is in `chip8.c`. The idea is that this core be
platform independent and then hooks are provided for platform specific
implementations.

The API is described in `chip8.h`. The `docs` target in the Makefile generates
HTML documentation from it.

Two implementations are provided in this repository:

1. A SDL-based implentation (<https://www.libsdl.org/>) which is intended for
  portability, and
2. a native Windows implementation which is intended for small size and
  requires no third party dependencies.

In both versions

* `bmp.h` and `bmp.c` (together with the `fonts/` directory) is used to draw
  and manipulate the bitmap graphics. See also
  https://github.com/wernsey/bitmap
* `render.c` implements the `init_game()`, `deinit_game()` and `render()`
  functions that forms the core of both implementations and demonstrates how
  the interpreter's API works.

The `render()` function checks the keyboard and executes the interpreter a
couple of times by calling `c8_step()` and redraws the screen if it changed.
The SDL and Win32 frameworks were written in such a way that the `render()`
function works with both with only a couple of minor modifications.

The implementations feature a rudimentary debugger: Press F5 to pause a running
game. The program counter and the current instruction will be displayed at the
bottom of the screen, along with the values of the 16 Vx registers. Press F6 to
step through the program to the next instruction and F8 to resume the program.
  
The `Makefile` will build the SDL version by default, and build the GDI version
under Windows.
  
### SDL Implementation

The SDL-based implementation is intended for portability. The files `pocadv.c`
and `pocadv.h` implement a wrapper around the SDL that contains the `main()`
function, the SDL event loops and so on.

The included `emscripten.mak` file is used to compile the SDL implementation to
JavaScript with [Emscripten](http://emscripten.org/) for running the
interpreter in a web browser. The `chip8.html` is a wrapper around the
Emscripten-generated JavaScript. If you want to use this implementation:

1. You need to put your CHIP-8 binary file in a `./GAMES/` directory
2. Run `make -f emscripten.mak`
3. Change the `Module.arguments` variable in the JavaScript in `chip8.html`
4. Serve `chip8.html` in a web server.

I built the emscripten version through the emscripten SDK installed
according to the [installation instructions][emscripten-install]. I had
some linker errors with Ubuntu's `emscripten` package that I couldn't
resolve.

[emscripten-install]: http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html#sdk-download-and-install

### Win32/GDI Implementation

The native Windows version uses a simple hook around the Win32 GDI and requires
no third party dependencies.

`gdi.h` and `gdi.c` implements the native Windows code. It implements a
`WinMain` function with the main Win32 events processing loop. It binds the
window's GDI context to a `Bitmap` object so that a render function can draw
onto it and fires off periodic `WM_PAINT` messages which calls the `render()`
function to draw the screen.

## Implementation Notes

I've consulted several sources for my implementation (see references below),
and there were some discrepancies. This is how I handled them:

* Regarding `2nnn`, [2] says the stack pointer is incremented first (i.e.
  `stack[++SP]`), but that skips `stack[0]`. My implementation does it the
  other way round.
* The `Fx55` and `Fx65` instructions doesn't change `I` in my implementation:
  * Wikipedia [1] says that modern emulators leave `I` unchanged (in the
    footnote under the instruction table).
  * [5] says that the `Fx55` instruction changes the value stored in `I`
  * [9] says that `Fx65` also changes `I`
  * All documentation I've found says that `I` changes to `I = I + x + 1`
  * The `+ 1` above actually feels wrong
* I've read David Winter's emulator's documentation [4] when I started, but I
  implemented things differently:
  * His emulator scrolls only 2 pixels if it is in low-res mode, but 4 pixels
    is consistent with [10].
  * His emulator's `Dxy0` instruction apparently also works differently in
    lo-res mode.
* [8] says that images aren't generally wrapped, but [3] and [10] seems to
  think differently.
* According to [7], the upper 256 bytes of RAM is used for the display, but it
  seems that modern interpreters don't do that. Besides, you'd need 1024 bytes
  to store the SCHIP's hi-res mode.
* `hp48_flags` is not cleared between runs (See [11]); I don't make any effort
  to persist them, though.
* Apparently there are CHIP-8 interpreters out there that don't use the
  standard 64x32 and 128x64 resolutions, but I don't support those.
* As far as I can tell, there is not much in terms of standard timings on
  CHIP-8 implementations. My implementation allows you to specify the speed as
  the number of instructions to execute per second (through the global variable
  `speed` in `render.c`). The value of 1200 instructions per second seems like
  a good value to start with.

## References

* [1] <https://en.wikipedia.org/wiki/CHIP-8>
* [2] Cowgod's Chip-8 Technical Reference v1.0, by Thomas P. Greene,
  <http://devernay.free.fr/hacks/chip8/C8TECH10.HTM>
* [3]
  <http://www.multigesture.net/articles/how-to-write-an-emulator-chip-8-interpreter/>
* [4] CHIP8 A CHIP8/SCHIP emulator Version 2.2.0, by David WINTER
  (<http://devernay.free.fr/hacks/chip8/CHIP8.DOC>)
* [5] Chip 8 instruction set, author unknown(?),
  <http://devernay.free.fr/hacks/chip8/chip8def.htm>
* [6] Byte Magazine Volume 03 Number 12 - Life pp. 108-122. "An Easy
  Programming System," by Joseph Weisbecker,
  <https://archive.org/details/byte-magazine-1978-12>
* [7] <http://chip8.wikia.com/wiki/Chip8>
* [8] <http://chip8.wikia.com/wiki/Instruction_Draw>
* [9] Mastering CHIP-8 by Matthew Mikolay, <http://mattmik.com/chip8.html>
* [10] Octo, John Earnest, <https://github.com/JohnEarnest/Octo>
* [11] Octo SuperChip document, John Earnest,
  <https://github.com/JohnEarnest/Octo/blob/gh-pages/docs/SuperChip.md>
* [12] <http://www.codeslinger.co.uk/pages/projects/chip8/primitive.html>

## License

This code is licensed under the [Apache license version
2](http://www.apache.org/licenses/LICENSE-2.0):

    Copyright 2015-2016 Werner Stoop
    
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
    
        http://www.apache.org/licenses/LICENSE-2.0
    
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

