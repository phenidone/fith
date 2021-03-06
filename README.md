# FITH

It's about 80% of Forth.  Also an acronym.

## What

This is a simple Forth-like system, inspired by Jones-forth: 
http://annexia.org/forth

... but with some odd differences due to being designed for use
in an embedded system where reliability is key.

## Why

I wanted a simple embeddable, interpreted language with bounds checking to put on PLCs and implement
really simple logic for things like building automation: lights, HVAC, etc.

PLCopen Structured Text would be better but due to its complexity, that will have to wait.

# How

The system has two build modes: the full system including compiler, and a cut-down mode which
has a number of risky opcodes disabled, designed to run pre-compiled code in an embedded system.

## Interpreter

The interpreter has four address spaces:
- the code space, which is executable and can be accessed with the opcodes C! C@ and ,
- the data space, which is accessed with the opcodes ! @ !C and @C
- the data stack, which most instructions manipulate, and
- the return stack, which is used when calling into and returning from subroutines, and via the opcodes >R R> R@ RDROP and RPICK

The machine's cell-size and address granularity is 32-bits, though the !C and @C
opcodes have a byte addressing granularity, e.g. for string-handling.

The execution of builtin opcodes is implemented using a jump table, i.e. an array of member-function pointers,
indexed by (bounds-checked) opcodes.

In contrast to classic Forth where word-names are stored in the header of each word, Fith keeps
a dictionary entirely outside the code space; it is accessible to the user code only via the CREATE, FIND,
LATEST, IMMEDIATE and HIDDEN opcodes.  The code-space therefore contains only byte-code.

Unlike classic Forths, there is no native (x86, ARM, etc) machine code in the code space,
nor is any machine code accessible
to or callable from the Fith user-layer code.  The fith-machine should be entirely portable and purely
virtual, i.e. it should be impossible to determine the nature of or interact with the
underlying physical architecture from within this interpreted language.

Yes, this is pretty slow.  That doesn't matter when we have a 32-bit microcontroller to turn a light or fan on/off
once every few minutes.

### Code Space

The code space is an array of 32-bit cells.  The first cell in the code space indicates the total number of valid cells in the code space.

Most cells contain an opcode:
- builtin opcodes have the top bit (FLAG_MACHINE) set
- two more upper bits denote hidden (FLAG_HIDE) and immediate (FLAG_IMMED) subroutines
- cells without FLAG_MACHINE set are generally interpreted as an address (cell offset
  into the code space) to which a function call should be made.

The cells directly following LIT, JMP and JZ are all taken to be 32-bit signed scalar values, not
offsets into the code space and therefore not subject to relocation.

The DUMP opcode will dump out a disassembly of the whole code-space to bindump.txt, which is particularly
useful when attempting to debug code-generation.

### Data Space

The data space is an array of 32-bit cells.  The first cell in the data space indicates the total number of valid cells in the data space.

The data space can be freely used by the program.  At startup, some room is allocated for the use of the
WORD opcode, as a static place that text can be read into.

Strings are represented in the data space as a 32-bit length (1 cell) followed by NUL-terminated ASCII
data.  The space allocation for a string, including its NUL-terminator is rounded up to a whole number
of cells.  Strings are represented on the stack as a pointer (offset into data space) to the length prefix.

### Stacks

The stacks are arrays of 32-bit cells, for use while executing a thread.  Each thread gets its own
Context, which contains a pair of stacks and stack-pointers.

They can contain any scalar, data-offet or code-offset values. Stacks are therefore never persistent and
not relocatable because it is not possible to tell the data type of each cell in a stack.

## Compiler

In the full system (-DFULLFITH when compiling: fithi), all opcodes are enabled.  There is a bootstrap
function which creates the beginning of the REPL: colon, semicolon and QUIT.  On initialisation
of a full system, the interpreter first reads and executes bootstrap.5th which contains everything
needed to bootstrap a working compiler from the builtin opcodes.

The compiler is quite Forth-like but not identical to Forth.  Some of the words have
arguments in a different order and the syntax differs slightly for some looping and branching constructs.

Closures (CREATE DOES>) are supported.

Locals are not currently supported by the compiler.

## Garbage Collection

Once a program has been compiled, a GC is provided which:
- accepts the address of a single entry-point function
- determines its static call-graph
- discards all functions which are not reached from the entry point
- relocates all reachable functions into the minimum space
- saves the code and data spaces to files, along with a map-file showing the result of the relocation

Where a program is written for embedded use, it will not make any reference to the compiler and
therefore the GC will discard the majority of the code present in bootstrap.5th.

Execution cannot continue after the GC has run because there is no guarantee that any functions on
the return-stack will remain in code space, and the interpreter-loop itself (QUIT) is likely to have also
been purged unless it was referenced by the entry-point.

Once GC is completed, SAVE is called automatically, which saves the relocated program into a binary file
along with its map and the chosen entry-point (the GC root).  The interpreter then exits.

## Embedded Runtime

In embedded mode (without -DFULLFITH: fithe), no bootstrap is performed.  Instead, code and data spaces
are directly loaded into memory, the map-file is consulted to find the entry-point and execution begins there.

In embedded mode, "risky" opcodes are disabled:
- the code space cannot be accessed except via execution (no self-modifying code!)
- the compiler is obviously disabled
- the external dictionary (CREATE, FIND) upon which the compiler depends is not present
- stream IO (KEY, EMIT, WORD, DOT, EOF) is disabled
and a number of large compilation dependencies (STL and C++ streams) are avoided.

Any interesting structures such as closures that were generated before the GC was executed and
the cut-down output was saved, will continue to work properly, though it will not be possible to instantiate
new closures after that point due to the prohibition on modifying code.

IO in embedded mode is to be performed using the SYSCALL opcodes.  An embedding of the interpreter must
supply appropriate implementations that get/set the necessary state.

The main function (entry-point / root of GC) should install handlers (function pointers) for
various events relevant to that system.  In turn, the system will invoke execution at those
entry points when the events occur.  Because the main function references all of the various
event-handler functions, those functions and their call graphs will be preserved through GC.  See
5th/plctest.5th for an example where MAIN installs a GPIO-change handler and a timer handler.

## Saved-Binary Format

When saved to file, binaries have the following structure:
- uint32 magic=0x48544946;   ("FITH")
- uint32 fileversion=1;
- uint32 binversion=1;
- uint32 ioversion=1;
- uint32 segcount=3+;
- struct segment[segcount]
  - uint32 segtype
  - uint32 seglength
  - uint32 data[seglength-1]

The following values of segtype are admissible:
- 0x101: TEXT (compiled program)
- 0x102: DATA (memory initial content)
- 0x103: CONFIG (persistent but mutable data)
- 0x104: ENTRY (program entry point)
- 0x105: MAP (textual listing of symbols)
- 0x110: CRC (CRC32-MPEG2 big-endian)
- 0x111: signature (TBD)

A saved binary must have exactly one segment of type TEXT, one segment of type DATA.  It may have one
segment of type ENTRY, which contains the primary entry-point to the program, and/or one segment
of type MAP which conains a textual map of the program's symbols.  A saved program should also have one
segment of type CRC.

CRC and Signature segments cover all file content (including the header) prior to the beginning
of that segment, but no part of that segment.

The various version flags in the header are for compatibility-checking in future, allowing changes
to the binary format (e.g. new instructions) or the IO subsystem (SYSCALLS supported).  The fileversion
flag specifies the format, currently it must be 1.

## PLC Engine

The PLC simulator (fithp) is a unix-based simulation of a trivial PLC with two GPIO ports (32 bits in, 32 bits out)
and a single periodic timer.  It runs the embedded FITH runtime and makes the PLC functionality (GPIO read/write
and timer manipulations) available through the SYSCALL instructions.

See plcsim.cc for the driver program, 5th/plc.5th for interface definitions and 5th/plctest.5th for
a trivial demonstration that shows GPIO manipulations and the use of timer events.  While running it, press
digit keys on the keyboard to toggle input pins.

# Example Code

For example code, see bootstrap.5th.  Some examples are pasted in below.

Simple recursive function:
```
( recursive factorial on stack )
( n -- n! )
: FACTORIAL
  DUP IF
    DUP 1 - RECURSE *
  ELSE
    DROP 1
  ENDIF
;
```

Factorial, converted for tail-recursion optimisation (?TAIL does a conditional tail-recurse):
```
( accum n -- accum*n n-1 )
: _TAILFACTINNER
  DUP ROT *     ( n accum*n )
  SWAP 1 -      ( accum n-1 )
  DUP ?TAIL     ( tail recursion when n != 0 )
;


( tail-recursed factorial as a loop )
( n -- n! )
: TAILFACT
  1 SWAP         ( 1 n )
  _TAILFACTINNER ( n! 0 )
  DROP

  HIDE _TAILFACTINNER
;
```

ROT13, performing string access, iteration and selection:

```
( CHAR BASE -- CHAR )
: _ROT13C
  SWAP OVER -        ( b c-b )
  13 + 26 MOD + ( b+(c-b+13)%26 )
;

( rot13 in-place, case-preserving )
( str -- str )
: ROT13
  DUP STRLEN        ( str len )
  0 FOR             ( str )
    DUP I [@]       ( str char )

    DUP 'A' >=      ( str char char>=A )
    OVER 'Z' <= &   ( str char A<=c<=Z )
    IF
        'A' _ROT13C
    ENDIF

    DUP 'a' >=      ( str char char>=A )
    OVER 'z' <= &   ( str char A<=c<=Z )
    IF
        'a' _ROT13C
    ENDIF

    OVER I [!]      ( str; str[i]=char )
  ROF

  HIDE _ROT13C
;

" Hello World" ROT13 TELL     ( prints Uryyb Jbeyq )


```

Use of closures to create functions for unit-conversion:

```
( test generation of closures with DOES, by creating a
  div/mul unit-conversion functor )
( mul div -- )
: UNITS

  ( create named constructor for closure )
  WORD HERE C@ CREATE

  ( transfer 2 words from stack into closure )
  2 PRESERVE DOES>

  ( this is what happens inside the closure )
  */
;

( function for converting inches to mm )
254 10 UNITS INCH
12 INCH .   ( prints 304 )
( another function for converting miles to metres )
160934 100 UNITS MILE
5 MILE .    ( prints 8046 )

```

## Example Sessions & Invocation

Here's what it looks like to compile a top-level function, call
the GC on it and then load that saved binary into a new interpreter
and run it:
```
william@gytha:~/git/code/fith$ ./fithi
FITH Bootstrap Complete
INCLUDE 5th/example.5th
: TEST 8 1 FOR I FACTORIAL . ROF CR ;
[FUNCPTR] TEST GC
warn: GC retains extended instruction EMIT in CR
warn: GC retains extended instruction . in TEST
SAVE success
************************
context state = Halted
IP = 17
D:
R:
************************
william@gytha:~/git/code/fith$ ./fithi -r save.fith
1 2 6 24 120 720 5040 
```

Again without the GC, saving the whole interpreter state.  This requires that you
specify the name of the entry-point on the command line; the first attempt to load and run
the saved program fails because the entry-point is unknown.

```
william@gytha:~/git/code/fith$ ./fithi 
FITH Bootstrap Complete
: HELLO ." Hello World" CR ;
SAVE
SAVE success
^D
william@gytha:~/git/code/fith$ ./fithi -r save.fith
Loader(save.fith) failed
william@gytha:~/git/code/fith$ ./fithi -r save.fith HELLO
Hello World
```

To compile and run the plctest demo, noting that the last line in plctest.5th is a call to GC:

```
william@gytha:~/git/code/fith$ ./fithi < 5th/plctest.5th
FITH Bootstrap Complete
SAVE success
************************
context state = Halted
IP = 17
D:
R:
************************
william@gytha:~/git/code/fith$ ./fithp -r save.fith 
IN 00000000000000000000000000000000 OUT 00000000000000000000000000000000
IN 00000000000000000000000000000000 OUT 00000000000000000000000100000000
IN 00000000000000000000000000000000 OUT 00000000000000000000001000000000
IN 00000000000000000000000000000000 OUT 00000000000000000000001100000000
IN 00000000000000000000000000000000 OUT 00000000000000000000010000000000
^C
```

# License

(C) 2018 W. Brodie-Tyrrell
Licensed under GPL v3