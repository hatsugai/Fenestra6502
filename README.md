# Fenestra6502

A 6502 system for high-level programming languages

Fenestra6502 is a 6502-based system with an extra circuit to improve the performance of procedure/function calls.

The circuit realizes "zero-page windows", which is similar to "register windows" adopted in several RISC processors.
The circuit detects the instruction fetch of JSR/RTS and changes the mapping of zero-page on physical memory.

Since 6502 has no pointer registers, the operations of stack frames are expensive compared to other modern processors.
The "zero-page windows" dramatically simplifies the operations and improve the performance of procedure/function calls.

![Fenestra photo](https://github.com/hatsugai/Fenestra6502/blob/images/Fenestra6502_1024c.jpg)
![overlay](https://github.com/hatsugai/Fenestra6502/blob/images/overlay.jpg)

## Introduction

In the BASIC era, from the late 1970s to the early 1980s, 6502 was competitive to the processors of the later such as Z80 and MC6809.
After that, the structure programming waves had come and procedural programming languages such as PASCAL and C had come to be used on microprocessors.

Since MC6809 has modern functionalities to assist procedural programming languages, there were no difficulties to implement such languages. Z80 was also able to adapt to them thanks to the index registers added, but more costly than MC6809.

However, 6502 faced difficulties. There were two reasons:

1. The stack area of 6502 is limited to only 256 bytes. It is not enough to realize stack frames.

2. 6502 has no pointer registers. To realize a user-defined stack and the stack pointer (and frame pointers, if necessary), the zero-page indirect indexed addressing mode has to be used instead. It is relatively expensive.

## Idea: Zero page windows

To overcome the above difficulties, I came up with the idea of "zero-page windows". The idea is based on "register windows".

On the processors with register windows, the arguments for the calling procedure are stored to the registers in the parameter section instead of pushing onto the stack. When a call is invoked, the registers are automatically "shifted" and the parameter section becomes the current frame section.

![zpwin](https://github.com/hatsugai/Fenestra6502/blob/images/zp_window.png)

The zero-page is divided into four sections. The first three sections are "shifted" on calls (the JSR instruction) and returns (RTS). The last section (0x00C0-0x00FF) are fixed. When the stack pointer and the frame pointer are needed, they can be placed in this section.

![zp](https://github.com/hatsugai/Fenestra6502/blob/images/zero_page.png)

The zero-page is shifted by 128 bytes for each call. Because the maximum number of calls possible is limited to 128 (since the size of the stack is 256 bytes), 16k+128 bytes of memory are needed for the zero-page windows in addition to the 64k-256 bytes for the fixed area.

![memory map](https://github.com/hatsugai/Fenestra6502/blob/images/memory_map.png)

## Comparing the cost of the operations on stack frames

To evaluate the effectiveness of the zero-page windows, the cost of the operations on stack frames on several microprocessors are compared.

The following assumptions are placed:

1. The parameter passing via registers is out of scope.
2. Frame pointers are not used.

Comment on the second: If the size of all objects allocated on the stack can be determined on compile time, frame pointers are not needed, and indexed addressing mode on the stack pointer is enough. On the other hand, if there is a procedure using an object which size is determined in run-time (`alloca` is used, for example), a frame pointer is needed in addition to the stack pointer because the compiler cannot calculate the release offset of the stack pointer when returning from the procedure.

![stack frame](https://github.com/hatsugai/Fenestra6502/blob/images/stack_frame.png)

The following four operations are considered:

- prologue code (make a new frame)
- epilogue code (release the frame and return)
- load from the stack frame
- store to the stack frame

In this part, general cases are considered. There are compilers that can emit more efficient code in some cases (for example, use HL instead of IX if possible, use the PUSH instruction instead of offsetting the stack pointer SP). Moreover, the best code could be derived by hand-tuning. However, I do not take into account such cases.

### Z80

Because Z80 does not have indexed addressing modes on the stack pointer SP, a frame pointer that supports the indexed addressing mode is needed. The index register IX is used for this purpose.

#### Z80 prologue code

```asm
                        ; clocks
  push ix               ; 15
  ld ix,#0              ; 14
  add ix,sp             ; 15
  ld hl,#<disp>         ; 10
  add hl,sp             ; 11
  ld sp,hl              ;  6
```

SP offset can be replaced by the PUSH instructions when the offset is small enough.


#### Z80 epilogue code

```asm
  ld sp,ix              ; 10
  pop ix                ; 14
  ret                   ; 10
```

#### Z80 load and store

```asm
  ld r,(ix+<disp>)      ; 19
```
```asm
  ld (ix+<disp>),r      ; 19
```

### MC6809

Since MC6809 supports the indexed addressing mode on the hardware stack pointer S, things are very simple and ideal.

NOTE: If a frame pointer is needed, the user stack pointer U can be used.

#### MC6809 prologue code
```asm
  leas -<disp>,s        ; 5
```

#### MC6809 epilogue code

```asm
 leas <disp>,s          ; 5
 rts                    ; 5
```

#### MC6809 load and store

```asm
  lda <disp>,s          ; 5
```
```asm
  sta <disp>,s          ; 5
```

### 6502

In the case of 6502, the stack pointer has to be implemented as a pointer placed on the zero-page.

The access to the stack frame can be performed by the zero-page indirect indexed addressing mode with the index register Y.

#### 6502 prologue code
```asm
  lda *SP               ; 3
  sec                   ; 2
  sbc #<disp>           ; 2
  sta *SP               ; 3
  lda *(SP+1)           ; 3
  sbc #0                ; 2
  sta *(SP+1)           ; 3
```

#### 6502 epilogue code
```asm
  lda *SP               ; 3
  clc                   ; 2
  adc #<disp>           ; 2
  sta *SP               ; 3
  lda *(SP+1)           ; 3
  adc #0                ; 2
  sta *(SP+1)           ; 3
  rts                   ; 6
```

#### 6502 load and store

```asm
   ldy #<disp>          ; 2
   lda (SP),y           ; 5
```
```asm
   ldy #<disp>          ; 2
   sta (SP),y           ; 5
```

### Fenestra6502

In the case of Fenestra6502, both prologue and epilogue code is not needed.

The access to the frame can be performed by the zero-page addressing.

#### Fenestra6502 load and store
```asm
  lda *<addr>           ; 3
```
```asm
  sta *<addr>           ; 3
```

### Summary of the comparison

|          | Z80  | MC6809 | 6502 | Fenestra6502 |
|:---|---:|---:|---:|---:|
| prologue | 71   | 5      | 18   | 0 |
| epilogue | 34   | 10     | 24   | 6 |
| load     | 19   | 5      |  7   | 3 |
| store    | 19   | 5      |  7   | 3 |

(unit: clocks)

## Hardware design

The zero-page windows are realized with the following components:

- JSR/RTS decoder detects the fetch of JSR/RTS instructions by observing the clock E and SYNC signal and generate up/down signals for the frame counter.

- Frame counter counts the nested count of JSR/RTS. The count value is used to calculate the address to which the zero-page is mapped.

- Adder adds the value of the frame counter and the address A7 to shift the zero-page mapping by 128 bytes for each call.

- Selector switches the address of the zero-page and the fixed area.

- Address decoder decodes the address and determines which area is to be accessed.

![block diagram](https://github.com/hatsugai/Fenestra6502/blob/images/block_diagram.png)

The address decoder and JSR/RTS decoder are implemented by GAL22V10.

The companion chip AVR ATMEGA164P plays the following four roles:

- Firstly, bootstraps 6502: write bootstrap code to the memory, then reset and start 6502.

- Works as a UART to communicate with PC. The communication between AVR and 6502 is handled by the RDY and IRQ signals with the WAI instruction.

- Generates the clocks for 6502.

- Measures the time by the internal 16-bit timer.

![schematic](https://github.com/hatsugai/Fenestra6502/blob/images/Fenestra6502.png)


## Test

To check if the circuit works, I ran the following code and observed the signals by a logic analyzer.

```asm
  lda #0xAA
  sta *0x80
  jsr f1
  jmp done

f1:
  lda *0
  lda #0xBB
  sta *0x80
  jsr f2
  rts

f2:
  lda *0
  lda #0xCC
  sta *0x80
  jsr f3
  rts

f3:
  lda *0
  rts
```

The behavior of the JSR/RTS decoder and the frame counter can be observed.

![seq](https://github.com/hatsugai/Fenestra6502/blob/images/sequence.png)

## Benchmarks

### Target systems

In addition to Fenestra6502, three systems are prepared: pure W65C02S, Z80, and MC6809 systems, no wait is applied, full-speed.

### Compilers

The following compilers are used:

- Z80: [SDCC (http://sdcc.sourceforge.net/)](http://sdcc.sourceforge.net/)
- MC6809: [gcc6809 (https://github.com/jmatzen/gcc6809)](https://github.com/jmatzen/gcc6809)
- 6502: [cc65 (https://cc65.github.io/)](https://cc65.github.io/)

I developed a compiler called CCLV for Fenestra6502 which takes advantage of the zero-page windows.

Each compiler is used with default settings. No optimization options are specified.

- [CCLV (https://github.com/hatsugai/CCLV)](https://github.com/hatsugai/CCLV)

### Codes

The following two codes are evaluated.

The function `fib` calculates the Fibonacci number. The argument 23 is chosen because it is the maximum argument of fib which result is within 16 bit signed integer.

The function `tarai` (a.k.a. Takeuchi function) causes many sub-calls. In the case `tarai(10, 1, 0)`, 803,421 calls are invoked. The maximum depth of the stack is 55.

```C
int fib(int n)
{
    if (n < 2)
        return n;
    else
        return fib(n - 1) + fib(n - 2);
}
```

```C
int tarai(int x, int y, int z)
{
    if (x <= y)
        return y;
    else
        return tarai(tarai(x - 1, y, z),
                     tarai(y - 1, z, x),
                     tarai(z - 1, x, y));
}
```

### Result

| System | fib(23)| tarai(10, 1, 0)|
|:---|---:|---:|
| Fenestra6502 (2MHz), CCLV |  3.688 |  36.457 |
| W65C02S (2MHZ), cc65      | 11.669 | 198.801 |
| Z80 (4MHZ), SDCC          |  6.296 |  83.256 |
| MC6809 (2MHz), gcc6809    |  4.824 |  51.421 |
| MC6800 (2MHz), CCLV       |  9.461 | 102.639 |

(unit: sec)

## Acknowledgements

I used [Bsch3V (https://www.suigyodo.com/online/schsoft.htm)](https://www.suigyodo.com/online/schsoft.htm) to write schematics of Fenestra6502. I would like to thank Mr. 岡田仁史.

I used [FGAL (http://elm-chan.org/works/pgal/report_e.html)](http://elm-chan.org/works/pgal/report_e.html) to compile GAL22V10 definitions. I also used PGAL and its schematics as reference. I would like to thank Mr. ChaN.
