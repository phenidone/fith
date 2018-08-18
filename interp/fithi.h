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
 * portable to microcontrollers, except for the part inside #ifdef FULLFITH
 *
 * Strings are represented in the heap as a 1-cell length (number of bytes),
 * followed by the string data, NUL-terminated.  On the stack, they are passed
 * as the offset into the heap of the length field.
 *
 * @author william@brodie-tyrrell.org
 */



#include <cstring>
#ifdef FULLFITH
#include <string>
#include <iostream>
#include <map>
#endif

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
     * Create baseline, non-interactive, non-compiling interpreter.
     * Requires that we pass in a ready-to-run binary of fixed size.
     *
     * @param bin pointer to loaded executable binary
     * @param binsz number of fith_cells in the binary
     * @param heap pointer to a heap space
     * @param heapsz number of fith_cells in the heap
     */
    Interpreter(fith_cell *_bin, std::size_t _binsz, fith_cell *_heap, std::size_t _heapsz);

#ifdef FULLFITH
    /**
     * Create an interactive interpreter with IO streams and compilation capabilities
     */
    Interpreter(fith_cell *_bin, std::size_t _binsz, fith_cell *_heap, std::size_t _heapsz,
                std::istream &_is, std::ostream &_os);
#endif
    
    enum {
        MW_EXIT = 0,    ///< exit/ret
        MW_LIT,         ///< literal
        MW_TICK,        ///< code-literal.  Subject to relocation whereas literals are not.
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
        MW_HERE,        ///< get ptr (0) to cell that contains offset of first free cell in either space
        MW_SYSCALL1,    ///< 1-param syscall
        MW_SYSCALL2,    ///< 2-param syscall
        MW_SYSCALL3,    ///< 3-param syscall
#ifdef FULLFITH
        MW_STORECODE,   ///< write to code area
        MW_READCODE,    ///< read from code area
        MW_COMMA,       ///< append to binary

        MW_KEY,         ///< wait and retrieve next keystroke
        MW_EMIT,        ///< emit one char
        MW_WORD,        ///< read a word/identifier from input stream
        MW_NUMBER,      ///< parse a string as a number ( ptr -- results unconverted )
        MW_DOT,         ///< print TOS as int
        MW_TELL,        ///< print TOS, assuming string ptr

        MW_CREATE,      ///< create a word definition
        MW_FIND,        ///< lookup a word in code space, by name
        MW_IMMEDIATE,   ///< toggle the immediate flag of the latest word definition
        MW_HIDDEN,      ///< toggle the hidden flag a word definition
        MW_LBRAC,       ///< left-bracket, enter immediate mode
        MW_RBRAC,       ///< right-bracket, enter compile mode
        MW_STATE,       ///< get compiling-state (compiling=true)

        MW_INTERPRET,   ///< interactive shell
#endif            
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
        Context(std::size_t _ip, fith_cell *_dstk, fith_cell *_rstk, std::size_t &_dsp, std::size_t &_rsp, std::size_t _dsz, std::size_t _rsz, Interpreter &_interp);

        /**
         * Run the interpreter until the called word returns or something breaks.
         */
        EXEC_RESULT execute();

        /**
         * Choose a new entry-point.
         */
        void set_ip(std::size_t _ip);
        
    private:
        
        void mw_exit();
        void mw_lit();
        void mw_tick();
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
        void mw_here();
        void mw_syscall1();
        void mw_syscall2();
        void mw_syscall3();
        
#ifdef FULLFITH
        void mw_storecode();
        void mw_readcode();
        void mw_comma();
        void mw_key();
        void mw_emit();
        void mw_word();
        void mw_number();
        void mw_dot();
        void mw_tell();
        void mw_create();
        void mw_find();
        void mw_immediate();
        void mw_hidden();
        void mw_lbrac();
        void mw_rbrac();
        void mw_state();

        void mw_interpret();
#endif
        
        /// machine words are kept here as member-function pointers
        typedef void (Context::*machineword_t)();

        /// function table 
        static const machineword_t builtin[Interpreter::MW_INTERP_COUNT];
        
        std::size_t ip;     ///< instruction pointer
        std::size_t &dsp;   ///< data stack pointer
        std::size_t &rsp;   ///< return stack pointer
        EXEC_RESULT state;  ///< what we're doing
        
        fith_cell *dstk;
        fith_cell *rstk;
        const std::size_t dsz;
        const std::size_t rsz;
        Interpreter &interp;
    };

#ifdef FULLFITH
    static const fith_cell WORDSZ=8;  // max 8 cells = 31 chars in the WORD buffer plus NUL
    /// allocation of heap space
    enum {
        HEREAT=0,
        WORDLENAT,
        WORDBUFAT,
        HEAPUSED=WORDBUFAT+WORDSZ
    };

    /// allocation of binary space/variables
    enum {
        HEREATB=0,
        STATE,
        BINUSED
    };

    fith_cell here() const { return bin[HEREAT]; }
    
    /**
     * append a word to the binary
     */
    void compile(fith_cell c);

    /**
     * create a dictionary entry
     */
    void create(const std::string &name, fith_cell value);

    /**
     * lookup a dictionary entry
     * @return -1 on failure, else the stored word
     */
    fith_cell find(const std::string &name) const;

    /**
     * get name of latest-created word
     */
    const std::string &latest() const;
    
#endif
private:
    
    /// get a C-string from the TOS ptr; NULL if invalid
    const char *get_string(fith_cell ptr);

    fith_cell *bin;
    fith_cell *heap;
    std::size_t binsz, heapsz;

    // first cell of both bin and heap specify how much space is
    // allocated in each, therefore starts at 1.
    
#ifdef FULLFITH

    /**
     * Manually assemble a bunch of functions that are used to
     * bootstrap the system: things that are not native code, 
     * but which need to exist to support the self-hosting compiler.
     */
    void bootstrap();
    
    typedef std::map<std::string, fith_cell> dict_t;
    typedef dict_t::iterator di;
    typedef dict_t::const_iterator dci;
    
    std::istream &is;
    std::ostream &os;

    // we encode flags in the top three bits,
    // which means we have only 29-bit (*4 byte) = 2GB usable address space.
    static const fith_cell FLAG_MACHINE=  0x80000000;     // is a machine opcode, via negation
    static const fith_cell FLAG_IMMED=    0x40000000;     ///< indicates an immediate word
    static const fith_cell FLAG_HIDE=     0x20000000;     ///< indicates a hidden word
    static const fith_cell FLAG_ADDR=     0x1FFFFFFF;     ///< mask to obtain address from dict
    
    // machine-word (opcode) names
    static const std::string opcodes[MW_INTERP_COUNT];
    
    std::string latestword;
    dict_t dictionary;
    bool compilestate;
#endif

};


}; // namespace fith
