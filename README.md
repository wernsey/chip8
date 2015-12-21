# CHIP-8 Interpreter, Assembler and Disassembler

This package contains an interpreter for [CHIP-8](https://en.wikipedia.org/wiki/CHIP-8) as
well as a command-line assembler and disassembler.

It also supports the SuperChip instructions.

The syntax of the assembler is based on the syntax described in
[Cowgod's Chip-8 Technical Reference v1.0](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM), by Thomas P. Greene

Frédéric Devernay's [CHIP-8 website](http://devernay.free.fr/hacks/chip8/) has a wealth of information.
The [GAMES.zip](http://devernay.free.fr/hacks/chip8/GAMES.zip) file contains several CHIP-8 games.

## Implementation Notes

* Regarding 2nnn, [2] says the stack pointer is incremented first (i.e. `stack[++SP]`), but that skips `stack[0]`. My implementation does it the other way round.
* The `Fx55` and `Fx65` instructions doesn't change `I` in my implementation:
** Wikipedia [1] says that modern emulators leave `I` unchanged (in the footnote under the instruction table).
** [5] says that the `Fx55` instruction changes the value stored in `I`
** [9] says that `Fx65` also changes `I`
** All documentation I've found says that `I` changes to `I = I + x + 1`
** The `+ 1` above actually feels wrong
* I've read David Winter's emulator's documentation [4] when I started, but I implemented things differently:
** His emulator scrolls only 2 pixels if it is in low-res mode, but 4 pixels is consistent with [10].
** His emulator's Dxy0 instruction apparently also works differently in lo-res mode.
* [8] says that images aren't generally wrapped, but [3] and [10] seems to think differently.
* According to [7], the upper 256 bytes of RAM is used for the display, but it seems that modern interpreters don't do that. Besides, you'd need 1024 bytes to store the SCHIP's hi-res mode.
* `hp48_flags` is not cleared between runs (See [11]); I don't make any effort to persist them, though.
* Apparently there are CHIP-8 interpreters out there that don't use the standard 64x32 and 128x64 resolutions, but I don't support those.
* As far as I can tell, there is not much in terms of standard timings on CHIP-8 implementations. My implementation follows the notes in [12]: The interpreter 
	refreshes the screen at 60 frames per second, and 10 instructions are executed during each frame.

### Portability

The assembler and disassemblers are simple command line applications and platform independent.

The core of the emulator is in `chip8.c`. The idea is that the core of the implementation be platform independent and then hooks are provided for platform specific implementations.

At the moment, only a Windows version that uses a simple hook around the Win32 GDI is available. A cross-platform version that uses SDL is on my *TODO* list.

## References

* [1] https://en.wikipedia.org/wiki/CHIP-8
* [2] Cowgod's Chip-8 Technical Reference v1.0, by Thomas P. Greene, http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
* [3] http://www.multigesture.net/articles/how-to-write-an-emulator-chip-8-interpreter/
* [4] CHIP8 A CHIP8/SCHIP emulator Version 2.2.0, by David WINTER (http://devernay.free.fr/hacks/chip8/CHIP8.DOC)
* [5] Chip 8 instruction set, author unknown(?), http://devernay.free.fr/hacks/chip8/chip8def.htm
* [6] Byte Magazine Volume 03 Number 12 - Life pp. 108-122. "An Easy Programming System," by Joseph Weisbecker, https://archive.org/details/byte-magazine-1978-12
* [7] http://chip8.wikia.com/wiki/Chip8
* [8] http://chip8.wikia.com/wiki/Instruction_Draw
* [9] Mastering CHIP-8 by Matthew Mikolay, http://mattmik.com/chip8.html
* [10] Octo, John Earnest, https://github.com/JohnEarnest/Octo
* [11] Octo SuperChip document, John Earnest, https://github.com/JohnEarnest/Octo/blob/gh-pages/docs/SuperChip.md
* [12] http://www.codeslinger.co.uk/pages/projects/chip8/primitive.html

## License

These sources are provided under the terms of the unlicense:

	This is free and unencumbered software released into the public domain.

	Anyone is free to copy, modify, publish, use, compile, sell, or
	distribute this software, either in source code form or as a compiled
	binary, for any purpose, commercial or non-commercial, and by any
	means.

	In jurisdictions that recognize copyright laws, the author or authors
	of this software dedicate any and all copyright interest in the
	software to the public domain. We make this dedication for the benefit
	of the public at large and to the detriment of our heirs and
	successors. We intend this dedication to be an overt act of
	relinquishment in perpetuity of all present and future rights to this
	software under copyright law.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
	OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
	ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.

	For more information, please refer to <http://unlicense.org/>
