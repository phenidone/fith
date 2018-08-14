/** -*- C++ -*- */

/**
 * This is a Forth-like interpreter, very loosely based on Jones-FORTH
 * but with the difference that it aims at memory safety for use on
 * embedded control systems.  It is also relatively portable (any 32-bit
 * architecture) and a lot slower because of the memory safety.
 *
 * In trying to make it safe, we lose:
 * - unrestricted memory read/write instructions
 * - self-modifying code, including callable compiler
 * - STDIN and therefore the ability to read WORDs, therefore
 * - there is no concept of IMMEDIATE/COMPILE mode
 * and we need to add:
 * - special read/write-within-heap instructions, separate from write-to-code
 * - an explicit Harvard-like data/code separation
 * - an offline compiler, which adds back in most of the missing features
 *   in order to bootstrap the system.
 *
 * That's an ugly workflow compared to normal Forth, but should result
 * in safer high-level programs that cannot so easily corrupt themselves,
 * and which do not actually need to perform compilation at runtime.
 *
 * The binary that this interpreter executes is distilled from a
 * less-constrained, more-complete Forth-like system.  Or it should
 * be possible to compile to this target machine from other imperative
 * languages if necessary.
 *
 * The cell-size is 32 bits, and the address granularity is also 32 bits.
 *
 * The structure of each Word, in the executable binary, is just a list
 * of target Words to execute.  Because we don't allow the mixing of
 * machine and forth code (all the machine-code is in the interpreter,
 * not the loaded binary!):
 * - positive words are offsets into the binary
 * - negative words are the index of built-in machine-code words
 * - zero is EXIT(-from-word)
 * Because of this coding, there is no need for a DOCOL at the front
 * of each non-machine definition, instead we use a jump-table to branch
 * to the builtins.
 *
 * The program has a persistent heap, but a new pair of data/call stacks
 * are initialised for each call/entry into the interpreter.  In typical
 * Forth fashion, the heap is a linear allocation with no credible means
 * of GC.  For long-running programs, the heap content should be initialised
 * at startup and no allocation should be performed during ongoing execution.
 *
 * The coding lacks STL etc because it is intended to be
 * portable to microcontrollers.
 *
 * @author william@brodie-tyrrell.org
 */

/*
Notes from jonesforth
! store also +! -!
@ fetch
STATE immediate or compiling.  Part of the INTERPRET function, i.e REPL
LATEST ptr to ptr to newest word
HERE ptr to ptr to next free byte
S0 ptr to data stack
BASE io radix
R0 ptr to return stack
DOCOL ptr to interp
F_IMMED (run even in compile mode), F_HIDDEN (find will fail), F_LENMASK (not the flags) various flags of a word
>R and R> transfer data between data/return stacks
RSP@ RSP! get/set the return-stack pointer via data stack
RDROP drop from return stack
DSP@ DSP! get/set data stack pointer to/from data stack!

WORD read a word from STDIN into static buffer, leaves char* and len on stack
NUMBER parse an int in a buffer, e.g. as returned by WORD
FIND locate word-header given a word (char* / len) on stack
>CFA convert word-pointer into code-pointer (skip word header)
DFA skip the DOCOL header

CREATE makes a word-header
[ ] enter/leave compilation mode
, COMMA appends to HERE, i.e. appends to a compilation

COLON == WORD CREATE (LIT DOCOL) , LATEST @ HIDDEN ] EXIT
  gets the word, creates header, appends DOCOL, sets hidden flag, sets compile mode

SEMICOLON == (LIT EXIT) COMMA LATEST @ HIDDEN [ EXIT
  appends the exit instruction, clears hidden-flag, clears compile mode

IMMEDIATE toggle the immediate bit of the latest word
HIDDEN toggle the hidden bit of a word pointed to on the stack
HIDE == WORD FIND HIDDEN EXIT

' TICK push the codeword address of the following word.  Get word address, i.e. obtain function pointer.

BRANCH uses offset following the opcode
0BRANCH is conditional, also relative.

." LITSTRING pushes char* / len of literal string
TELL prints a string

QUIT the entry-point.  while(true) { clear return-stack; INTERPRET; }
INTERPRET the REPL.  Read a word, look it up.  if immed, run it.  check if number (push or LIT ,) etc.

CHAR like WORD but just gets a single-char literal onto the stack
EXECUTE jmp to address popped from data stack!
SYSCALL calls to OS

KEY read char
EMIT print char
\ comment to EOL

compilation written in FORTH.  Uses ! to write computed offsets into compiled code.

PICK uses stack-pointer manipulation
: PICK 1+ 4 * DSP@ + @ ;

add:
: OVER (x y -- x y x) SWAP DUP NROT ; ?

: TUCK (x y -- y x y) SWAP OVER ;
: NIP (x y -- y) SWAP DROP ;
unsigned comparisons

 */

#include <cstring>

namespace fith {

typedef int fith_cell;
    
class Interpreter {
public: 

    enum EXEC_RESULT {
        EX_SUCCESS,         ///< execution completed, i.e. returned
        EX_DSTK_OVER,       ///< data stack overflowed
        EX_DSTK_UNDER,      ///< data stack underflowed
        EX_RSTK_OVER,       ///< return stack overflowed
        EX_RSTK_UNDER,      ///< return stack underflowed
        EX_SEGV_DATA,       ///< segmentation violation, e.g. ! or @ outside heap bounds
        EX_SEGV_CODE,       ///< execution outside the binary
        EX_BAD_OPCODE,      ///< opcode/instruction not recognised
        EX_DIV_ZERO,        ///< attempted to divide by zero
        EX_HALTED,          ///< execution halted, e.g. by timeout callback
        EX_RUNNING,         ///< still going

        EX_INTERP_COUNT
    };
        
    /**
     * Create interpreter.
     * @param bin pointer to loaded executable binary
     * @param binsz number of fith_cells in the binary
     * @param heap pointer to a heap space
     * @param heapsz number of fith_cells in the heap
     */
    Interpreter(fith_cell *_bin, std::size_t _binsz, fith_cell *_heap, std::size_t _heapsz);

    
    enum {
        MW_EXIT = 0,    ///< exit/ret
        MW_LIT,         ///< literal
        MW_PLUS,        ///< integer addition
        MW_MINUS,       ///< integer subtract
        MW_NEG,         ///< integer negate
        MW_MUL,         ///< integer multiply
        MW_DIV,         ///< integer divide
        MW_MOD,         ///< integer mod
        MW_MULDIV,      ///< */ = floor(n1*n2/n3) where n1*n2 is kept at double-width
        MW_DIVMOD,      ///< /MOD = quotient and mod
        MW_MULMOD,      ///< */MOD = per */ and produce both quotient and mod
        MW_JMP,         ///< unconditional jump
        MW_JZ,          ///< conditional (TOS==0) jump
        MW_CALL,        ///< call the word at TOS, as if *ip == *dsp
        MW_LT,          ///< less than
        MW_GT,          ///< greater than
        MW_LE,          ///< less than or equal
        MW_GE,          ///< greater than or equal
        MW_EQ,          ///< equal
        MW_MAX,         ///< 2-arg maximum
        MW_MIN,         ///< 2-arg minimum
        MW_DUP,         ///< duplicate
        MW_DUPNZ,       ///< duplicate if non-zero
        MW_DROP,        ///< drop
        MW_SWAP,        ///< swap top items = 1 ROLL
        MW_ROT,         ///< (x y z -- y z x) = 2 ROLL
        MW_NROT,        ///< (x y z -- z x y)
        MW_PICK,        ///< (a0 .. an n -- a0 .. an a0)
        MW_ROLL,        ///< (a0 .. an n -- a1 .. an a0)
        MW_AND,         ///< bitwise and
        MW_OR,          ///< bitwise or
        MW_XOR,         ///< bitwise xor
        MW_INVERT,      ///< bitwise invert
        MW_SL,          ///< left shift
        MW_SRA,         ///< right shift arithmetic (treat as signed)
        MW_SRL,         ///< right shift logical
        MW_STORE,       ///< ! write to heap
        MW_READ,        ///< @ read from heap
        MW_TORS,        ///< >R, move value from data stack to return stack
        MW_FROMRS,      ///< R>, move value from return stack to data stack
        MW_CPFROMRS,    ///< R@, copy value from return stack to data stack
        MW_RDROP,       ///< RDROP, drop value from return stack
            
        MW_INTERP_COUNT        ///< number of machine-words defined
    };

    /**
     * Context of execution of one thread.
     */
    class Context {
    public:
        /**
         * Create thread
         * @param _ip offset (cells) of the entry-point (start of called Word) in the binary
         * @param _dstk pointer to the data stack for this thread
         * @param _rstk pointer to the return stack for this thread
         * @param _dsp data stack pointer (number of valid cells)
         * @param _rsp return stack pointer (number of valid cells)
         * @param _dsz size of the data stack (cells)
         * @param _rsz size of the return stack (cells)
         */
        Context(std::size_t _ip, fith_cell *_dstk, fith_cell *_rstk, std::size_t _dsp, std::size_t _rsp, std::size_t _dsz, std::size_t _rsz, Interpreter &_interp);

    /**
     * Run the interpreter until the called word returns or something breaks.
     *
     */
        EXEC_RESULT execute();

    private:
        
        void mw_exit();
        void mw_lit();
        void mw_plus();
        void mw_minus();
        void mw_neg();
        void mw_mul();
        void mw_div();
        void mw_mod();
        void mw_muldiv();
        void mw_divmod();
        void mw_mulmod();
        void mw_jmp();
        void mw_jz();
        void mw_call();
        void mw_lt();
        void mw_gt();
        void mw_le();
        void mw_ge();
        void mw_eq();
        void mw_max();
        void mw_min();
        void mw_dup();
        void mw_dupnz();
        void mw_drop();
        void mw_swap();
        void mw_rot();
        void mw_nrot();
        void mw_pick();
        void mw_roll();
        void mw_and();
        void mw_or();
        void mw_xor();
        void mw_invert();
        void mw_sl();
        void mw_sra();
        void mw_srl();
        void mw_store();
        void mw_read();
        void mw_tors();
        void mw_fromrs();
        void mw_cpfromrs();
        void mw_rdrop();

        /// machine words are kept here as member-function pointers
        typedef void (Context::*machineword_t)();

        /// function table 
        static const machineword_t builtin[Interpreter::MW_INTERP_COUNT];
        
        std::size_t ip;      ///< instruction pointer
        std::size_t dsp;     ///< data stack pointer
        std::size_t rsp;     ///< return stack pointer
        EXEC_RESULT state;  ///< what we're doing
        
        fith_cell *dstk;
        fith_cell *rstk;
        const std::size_t dsz;
        const std::size_t rsz;
        Interpreter &interp;
    };

private:
    
    fith_cell *bin;
    fith_cell *heap;
    std::size_t binsz, heapsz;

};


}; // namespace fith
