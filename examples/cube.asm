; Companion Cube Sample program. Disregard its advice.

; Assemble the program to a CHIP-8 binary like so:
;    $ ./c8asm -o GAMES/CUBE8.ch8 examples/cube.asm
; Once you've assembled it, you can run it in the interpreter like so:
;    $ ./chip8 -f FFB6F8 -b B2B2B2 GAMES/CUBE.ch8

; Define some more human-friendly names for the registers
define boxx V0 ; Sprite X,Y position
define boxy V1
define oldx V2 ; Previous sprite X,Y position
define oldy V3
define dirx V4 ; Sprite direction
define diry V5
define tmp VE

; Clear the screen
CLS

SYS 1 ; SYS instructions are treated as no-ops

; Load the variables' initial values
LD  boxx, 1
LD  dirx, 1
LD  boxy, 10
LD  I, sprite1
DRW boxx, boxy, 8
LD  tmp, 1

; The main loop
loop:
	; Store the current position
LD oldx, boxx
LD oldy, boxy

	; If the X direction is 0, go to sub1...
SE dirx, 0
JP sub1

	; ...otherwise add 1 to the box' position
ADD boxx, 1

	; If you reached the right edge of the screen, change direction
	SNE boxx, 56
LD  dirx, 1
jp draw1
sub1:
	; subtract 1 from the box' position
SUB boxx, tmp

	; If you reached the left edge of the screen, change direction
SNE boxx, 0
LD  dirx, 0

; Draw the box
draw1:
	; Load the address of the sprite's graphics into register I
LD  I, sprite1
	; Erase the sprite at the old position
DRW oldx, oldy, 8
	; Draw the sprite at the new position
DRW boxx, boxy, 8

	; Return to the start of the loop
	JP  loop

; Binary data of the sprite.
; 1s represent pixels to be drawn, 0s are blank pixels.
sprite1:
db  %01111110,
    %10000001,
    %10100101,
    %10111101,
    %10111101,
    %10011001,
    %10000001,
    %01111110,
