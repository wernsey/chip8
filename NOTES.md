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

I saw this weird thing happen once where my sprite just disappeared. It turns out that
the number of cycles the program ran in each frame was the same as the number of iterations
in my game's loop, so that the sprite was erased and by the time the next frame was drawn the
game was in a state again just after the sprite had been erased.