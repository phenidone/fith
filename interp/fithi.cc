/** -*- C++ -*- */

#include "fithi.h"
#ifdef FULLFITH
#include <iostream>
#include <cstdlib>
#endif

using namespace std;

namespace fith {

const Interpreter::Context::machineword_t Interpreter::Context::builtin[Interpreter::MW_INTERP_COUNT]={
    &Interpreter::Context::mw_exit,
    &Interpreter::Context::mw_lit,
    &Interpreter::Context::mw_tick,
    &Interpreter::Context::mw_plus,    
    &Interpreter::Context::mw_minus,   
    &Interpreter::Context::mw_neg,
    &Interpreter::Context::mw_mul,
    &Interpreter::Context::mw_div,
    &Interpreter::Context::mw_mod,
    &Interpreter::Context::mw_muldiv,
    &Interpreter::Context::mw_divmod,
    &Interpreter::Context::mw_mulmod,
    &Interpreter::Context::mw_jmp,
    &Interpreter::Context::mw_jz,
    &Interpreter::Context::mw_call,
    &Interpreter::Context::mw_lt,
    &Interpreter::Context::mw_gt,
    &Interpreter::Context::mw_le,
    &Interpreter::Context::mw_ge,
    &Interpreter::Context::mw_eq,
    &Interpreter::Context::mw_max,
    &Interpreter::Context::mw_min,
    &Interpreter::Context::mw_dup,
    &Interpreter::Context::mw_dupnz,
    &Interpreter::Context::mw_drop,
    &Interpreter::Context::mw_swap,
    &Interpreter::Context::mw_rot,
    &Interpreter::Context::mw_nrot,
    &Interpreter::Context::mw_pick,
    &Interpreter::Context::mw_roll,
    &Interpreter::Context::mw_and,
    &Interpreter::Context::mw_or,
    &Interpreter::Context::mw_xor,
    &Interpreter::Context::mw_invert,
    &Interpreter::Context::mw_sl,
    &Interpreter::Context::mw_sra,
    &Interpreter::Context::mw_srl,
    &Interpreter::Context::mw_store,
    &Interpreter::Context::mw_read,
    &Interpreter::Context::mw_tors,
    &Interpreter::Context::mw_fromrs,
    &Interpreter::Context::mw_cpfromrs,
    &Interpreter::Context::mw_rdrop,
    &Interpreter::Context::mw_here,
    &Interpreter::Context::mw_syscall1,
    &Interpreter::Context::mw_syscall2,
    &Interpreter::Context::mw_syscall3,
#ifdef FULLFITH
    &Interpreter::Context::mw_storecode,
    &Interpreter::Context::mw_readcode,
    &Interpreter::Context::mw_comma,    
    &Interpreter::Context::mw_key,
    &Interpreter::Context::mw_emit,
    &Interpreter::Context::mw_word,
    &Interpreter::Context::mw_number,
    &Interpreter::Context::mw_dot,
    &Interpreter::Context::mw_tell,
    &Interpreter::Context::mw_create,
    &Interpreter::Context::mw_find,
    &Interpreter::Context::mw_immediate,
    &Interpreter::Context::mw_hidden,
    &Interpreter::Context::mw_lbrac,
    &Interpreter::Context::mw_rbrac,
    &Interpreter::Context::mw_state,
    &Interpreter::Context::mw_interpret    
#endif
};

#ifdef FULLFITH
const string Interpreter::opcodes[MW_INTERP_COUNT]={
    "EXIT",
    "LIT",
    "'",
    "+",
    "-",
    "NEGATE",
    "*",
    "/",
    "MOD",
    "*/",
    "/MOD",
    "*/MOD",
    "JMP",
    "JZ",
    "EXECUTE",
    "<",
    ">",
    "<=",
    ">=",
    "=",
    "MAX",
    "MIN",
    "DUP",
    "?DUP",
    "DROP",
    "SWAP",
    "ROT",
    "-ROT",
    "PICK",
    "ROLL",
    "&",
    "|",
    "^",
    "~",
    "<<",
    "SRA",
    ">>",
    "!",
    "@",
    ">R",
    "R>",
    "R@",
    "RDROP",
    "HERE",
    "SYSCALL1",
    "SYSCALL2",
    "SYSCALL3",

    "!C",
    "@C",
    ",",
    "KEY",
    "EMIT",
    "WORD",
    "NUMBER",
    ".",
    "TELL",
    "CREATE",
    "FIND",
    "IMMEDIATE",
    "HIDDEN",
    "[",
    "]",
    "STATE",
    "INTERPRET"
};
#endif

Interpreter::Interpreter(fith_cell *_bin, size_t _binsz, fith_cell *_heap, size_t _heapsz)
    : bin(_bin), heap(_heap), binsz(_binsz), heapsz(_heapsz)
#ifdef FULLFITH
    , is(cin), os(cout)
#endif
{
    // clear heap
    for(size_t i=0;i<heapsz;++i){
        heap[i]=0;
    }
    compilestate=false;
}

Interpreter::Context::Context(size_t _ip, fith_cell *_dstk, fith_cell *_rstk, size_t &_dsp, size_t &_rsp, size_t _dsz, size_t _rsz, Interpreter &_interp)
    : ip(_ip), dsp(_dsp), rsp(_rsp), state(EX_RUNNING), dstk(_dstk), rstk(_rstk), dsz(_dsz), rsz(_rsz), interp(_interp)
{
}

Interpreter::EXEC_RESULT Interpreter::Context::execute()
{
    state=EX_RUNNING;
    
    while(state == EX_RUNNING){
        if(ip >= interp.binsz){
            state=EX_SEGV_CODE;
            break;
        }
        fith_cell ins=interp.bin[ip];

        ++ip;
        
        if((ins & FLAG_MACHINE) != 0){
            // builtin
            ins &= FLAG_ADDR;
            if(ins >= MW_INTERP_COUNT){
                state=EX_BAD_OPCODE;
                break;
            }
#ifndef NDEBUG
            cerr << ip << ":" << opcodes[ins] << endl;
#endif
            // do it.
            (this->*builtin[ins])();
        }
        else{
            // word to call
            ins &= FLAG_ADDR;
#ifndef NDEBUG
            cerr << ip << ":" << ins << endl;
#endif

            // push return address and jump
            if(rsp >= rsz){
                state=EX_RSTK_OVER;
                break;
            }

            rstk[rsp++]=ip;
            ip=ins;
        }
    }

    return state;
}

void Interpreter::Context::set_ip(size_t _ip)
{
    ip=_ip;
}


/*
 * Here be the implementations of the builtin words
 */

void Interpreter::Context::mw_exit()
{
    if(rsp == 0){
        // returned from top-level function
        state=Interpreter::EX_SUCCESS;
    }
    else{
        // jump back
        ip=rstk[--rsp];
    }
}

void Interpreter::Context::mw_lit()
{
    if(dsp >= dsz){
        // stack overflow
        state=Interpreter::EX_DSTK_OVER;
    }
    else if(ip >= interp.binsz){
        // opcode trails off the end of the binary
        state=Interpreter::EX_SEGV_CODE;
    }
    else{
        // push literal
        dstk[dsp++]=interp.bin[ip++];
    }
}

void Interpreter::Context::mw_tick()
{
    if(dsp >= dsz){
        // stack overflow
        state=Interpreter::EX_DSTK_OVER;
    }
    else if(ip >= interp.binsz){
        // opcode trails off the end of the binary
        state=Interpreter::EX_SEGV_CODE;
    }
    else{
        // push literal
        dstk[dsp++]=interp.bin[ip++];
    }
}

void Interpreter::Context::mw_plus()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else{
        --dsp;
        dstk[dsp-1]=dstk[dsp-1] + dstk[dsp];
    }
}

void Interpreter::Context::mw_minus()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else{
        --dsp;
        dstk[dsp-1]=dstk[dsp-1] - dstk[dsp];
    }
}

void Interpreter::Context::mw_neg()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else{
        dstk[dsp-1]= -dstk[dsp-1];
    }
}

void Interpreter::Context::mw_mul()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else{
        --dsp;
        dstk[dsp-1]=dstk[dsp-1] * dstk[dsp];
    }
}

void Interpreter::Context::mw_div()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else if(dstk[dsp-1] == 0){
        state=Interpreter::EX_DIV_ZERO;
    }
    else{        
        --dsp;
        dstk[dsp-1]=dstk[dsp-1] / dstk[dsp];
    }
}

void Interpreter::Context::mw_mod()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else if(dstk[dsp-1] == 0){
        state=Interpreter::EX_DIV_ZERO;
    }
    else{        
        --dsp;
        dstk[dsp-1]=dstk[dsp-1] % dstk[dsp];
    }
}

void Interpreter::Context::mw_muldiv()
{
    if(dsp < 3){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else{
        // 64-bit mul (keep all the bits)
        long long prod=((long long) dstk[dsp-3]) * ((long long) dstk[dsp-2]);
        // then divide; discard upper bits
        fith_cell div=(fith_cell) ((prod/dstk[dsp-1]) & 0xFFFFFFFF);
        dsp-=2;
        dstk[dsp-1]=div;
    }
}

void Interpreter::Context::mw_divmod()
{
    state=Interpreter::EX_BAD_OPCODE;
}

void Interpreter::Context::mw_mulmod()
{
    state=Interpreter::EX_BAD_OPCODE;
}

void Interpreter::Context::mw_jmp()
{
    if(ip >= interp.binsz){
        // opcode trails off the end of the binary
        state=Interpreter::EX_SEGV_CODE;
    }
    else{
        // move the IP by the specified offset wrt the start of the JMP instruction
        ip+=interp.bin[ip]-1;
    }
}

void Interpreter::Context::mw_jz()
{
    if(dsp < 1){
        // do we have a test-value
        state=Interpreter::EX_DSTK_UNDER;
    }
    if(ip >= interp.binsz){
        // opcode trails off the end of the binary
        state=Interpreter::EX_SEGV_CODE;
    }
    else{
        // pop test-value
        fith_cell tv=dstk[--dsp];

        if(tv == 0){
            // move the IP by the specified offset wrt the start of the JMP instruction
            ip+=interp.bin[ip]-1;
        }
        else{
            // no branch, skip over the offset to next instruction
            ++ip;
        }
    }
}

void Interpreter::Context::mw_call()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
    fith_cell tgt=dstk[--dsp];
    
    if((tgt & FLAG_MACHINE) != 0){
        // builtin word, just run it
        // gonna be fun if it's MW_CALL :)
        tgt &= FLAG_ADDR;
        if(tgt >= Interpreter::MW_INTERP_COUNT){
            state=Interpreter::EX_BAD_OPCODE;
            return;
        }
        // exec it
        (this->*builtin[tgt])();
    }
    else{
        if(rsp >= rsz){
            state=Interpreter::EX_RSTK_OVER;
            return;
        }

        // clear any flag bits there might be
        tgt &= FLAG_ADDR;

        // call = push return, branch
        rstk[rsp++]=ip;  // IP was inc'd before we were called, so this is where to return to
        ip=tgt;

        // we don't validate the target address here, it will get
        // checked before fetch in the next cycle of execute()
    }
}
    
void Interpreter::Context::mw_lt()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1]=(dstk[dsp-1] < dstk[dsp]) ? 1 : 0;
}

void Interpreter::Context::mw_gt()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1]=(dstk[dsp-1] > dstk[dsp]) ? 1 : 0;
}

void Interpreter::Context::mw_le()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1]=(dstk[dsp-1] <= dstk[dsp]) ? 1 : 0;
}

void Interpreter::Context::mw_ge()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1]=(dstk[dsp-1] >= dstk[dsp]) ? 1 : 0;
}

void Interpreter::Context::mw_eq()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1]=(dstk[dsp-1] == dstk[dsp]) ? 1 : 0;
}

void Interpreter::Context::mw_max()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1]=(dstk[dsp-1] > dstk[dsp]) ? dstk[dsp-1] : dstk[dsp];
}

void Interpreter::Context::mw_min()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1]=(dstk[dsp-1] < dstk[dsp]) ? dstk[dsp-1] : dstk[dsp];
}

void Interpreter::Context::mw_dup()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else if(dsp >= dsz){
        state=Interpreter::EX_DSTK_OVER;
    }
    else{
        fith_cell c=dstk[dsp-1];
        dstk[dsp++]=c;
    }
}

void Interpreter::Context::mw_dupnz()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else if(dsp >= dsz){
        state=Interpreter::EX_DSTK_OVER;
    }
    else{
        fith_cell c=dstk[dsp-1];
        if(c != 0){
            dstk[dsp++]=c;
        }
    }
}

void Interpreter::Context::mw_drop()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else{
        --dsp;
    }
}

void Interpreter::Context::mw_swap()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else{
        fith_cell tmp=dstk[dsp-1];
        dstk[dsp-1]=dstk[dsp-2];
        dstk[dsp-2]=tmp;
    }
}

void Interpreter::Context::mw_rot()
{
    if(dsp < 3){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else{
        fith_cell tmp=dstk[dsp-3];
        dstk[dsp-3]=dstk[dsp-2];
        dstk[dsp-2]=dstk[dsp-1];
        dstk[dsp-1]=tmp;
    }
}
void Interpreter::Context::mw_nrot()
{
    if(dsp < 3){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else{
        fith_cell tmp=dstk[dsp-1];
        dstk[dsp-1]=dstk[dsp-2];
        dstk[dsp-2]=dstk[dsp-3];
        dstk[dsp-3]=tmp;
    }
}
void Interpreter::Context::mw_pick()
{
    if(dsp < 2 || dstk[dsp-1] < 0 || dsp < size_t(dstk[dsp-1]+2)){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else{
        dstk[dsp-1]=dstk[dsp-dstk[dsp-1]-2];
    }
}
void Interpreter::Context::mw_roll()
{
    if(dsp < 2 || dstk[dsp-1] < 0 || dsp < size_t(dstk[dsp-1]+2)){
        state=Interpreter::EX_DSTK_UNDER;
    }
    else{
        fith_cell n=dstk[--dsp];
        fith_cell tmp=dstk[dsp-n-1];
        for(fith_cell i=n;i>0;--i){
            dstk[dsp-i-1]=dstk[dsp-i];
        }
        dstk[dsp-1]=tmp;
    }
}

void Interpreter::Context::mw_and()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1] &= dstk[dsp];
}

void Interpreter::Context::mw_or()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1] |= dstk[dsp];
}

void Interpreter::Context::mw_xor()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1] ^= dstk[dsp];
}

void Interpreter::Context::mw_invert()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
    }
    dstk[dsp-1] = ~dstk[dsp-1];
}

void Interpreter::Context::mw_sl()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1] <<= dstk[dsp];
}

void Interpreter::Context::mw_sra()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    dstk[dsp-1] >>= dstk[dsp];
}

void Interpreter::Context::mw_srl()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
    }
    --dsp;
    unsigned long c=(unsigned long) dstk[dsp-1];
    c >>= dstk[dsp];
    dstk[dsp-1]=(fith_cell) c;
}

// ( val addr -- )
void Interpreter::Context::mw_store()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
    size_t ptr=(size_t) dstk[dsp-1];
    if(ptr >= interp.heapsz){
        state=Interpreter::EX_SEGV_DATA;
        return;
    }
    interp.heap[ptr]=dstk[dsp-2];
    dsp-=2;
}

void Interpreter::Context::mw_read()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
    size_t ptr=(size_t) dstk[dsp-1];
    if(ptr >= interp.heapsz){
        state=Interpreter::EX_SEGV_DATA;
    }
    dstk[dsp-1]=interp.heap[ptr];
}


void Interpreter::Context::mw_tors()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
    if(rsp >= rsz){
        state=Interpreter::EX_RSTK_OVER;
        return;
    }
    
    rstk[rsp++]=dstk[--dsp];
}

void Interpreter::Context::mw_fromrs()
{
    if(rsp < 1){
        state=Interpreter::EX_RSTK_UNDER;
        return;
    }
    if(dsp >= dsz){
        state=Interpreter::EX_DSTK_OVER;
        return;
    }
    
    dstk[dsp++]=rstk[--rsp];
}

void Interpreter::Context::mw_cpfromrs()
{
    if(rsp < 1){
        state=Interpreter::EX_RSTK_UNDER;
        return;
    }
    if(dsp >= dsz){
        state=Interpreter::EX_DSTK_OVER;
        return;
    }
    
    dstk[dsp++]=rstk[rsp-1];
}

void Interpreter::Context::mw_rdrop()
{
    if(rsp < 1){
        state=Interpreter::EX_RSTK_UNDER;
        return;
    }
    
    --rsp;
}

void Interpreter::Context::mw_here()
{
    if(dsp >= dsz){
        state=Interpreter::EX_DSTK_OVER;
        return;
    }
    dstk[dsp++]=0;
}

void Interpreter::Context::mw_syscall1()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
#ifndef NDEBUG
    cerr << "syscall1(" << dstk[dsp-1] << ")" << endl;
#endif
    dstk[dsp-1]=0;
}

void Interpreter::Context::mw_syscall2()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
#ifndef NDEBUG
    cerr << "syscall2(" << dstk[dsp-2] << ", " << dstk[dsp-1] << ")" << endl;
#endif
    dsp--;
    dstk[dsp-1]=0;
}

void Interpreter::Context::mw_syscall3()
{
    if(dsp < 3){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
#ifndef NDEBUG
    cerr << "syscall3(" << dstk[dsp-3] << ", " << dstk[dsp-2] << ", " << dstk[dsp-1] << ")" << endl;
#endif
    dsp-=2;
    dstk[dsp-1]=0;
}


#ifdef FULLFITH

Interpreter::Interpreter(fith_cell *_bin, size_t _binsz, fith_cell *_heap, size_t _heapsz,
                         istream &_is, ostream &_os)
    : bin(_bin), heap(_heap), binsz(_binsz), heapsz(_heapsz), is(_is), os(_os)
{
    // clear heap
    for(size_t i=0;i<heapsz;++i){
        heap[i]=0;
    }
    compilestate=false;

    // first words in each space denote the space consumed so far
    bin[HEREAT]=1;   // HERE    
    heap[HEREAT]=HEAPUSED;  // HEREDATA
    heap[WORDLENAT]=0;
    
    latestword="";

    // generate a bunch of builtin functions
    bootstrap();
}

void Interpreter::bootstrap()
{
    // put all the opcodes in the dict
    for(int i=0;i<MW_INTERP_COUNT;++i){
        create(opcodes[i], i | FLAG_MACHINE);
    }
    // make some immediate
    dictionary["IMMEDIATE"] |= FLAG_IMMED;
    dictionary["["] |= FLAG_IMMED;
    
    // : OVER ( x y -- x y x ) SWAP DUP NROT ;
    fith_cell over=here();
    create("OVER", over);
    compile(MW_SWAP);
    compile(MW_DUP);
    compile(MW_NROT);
    compile(MW_EXIT);

    // : TUCK ( x y -- y x y ) SWAP OVER ;
    fith_cell tuck=here();
    create("TUCK", tuck);
    compile(MW_SWAP);
    compile(over, false);
    compile(MW_EXIT);

    // : NIP ( x y -- y ) SWAP DROP ;
    fith_cell nip=here();
    create("NIP", nip);
    compile(MW_SWAP);
    compile(MW_DROP);
    compile(MW_EXIT);

    // : : WORD CREATE (LIT DOCOL , ) LATEST @ HIDDEN ] ;
    fith_cell colon=here();
    create(":", colon);
    compile(MW_WORD);
    compile(MW_LIT);
    compile(HEREAT, false);
    compile(MW_READCODE);  // HERE @
    compile(MW_CREATE);
    // compile(MW_COMMA);
    compile(MW_HIDDEN);
    compile(MW_RBRAC);
    compile(MW_EXIT);
    
    // : ; IMMEDIATE ' EXIT , LATEST @ HIDDEN [ ;
    fith_cell semicolon=here() | FLAG_IMMED;
    create(";", semicolon);
    compile(MW_TICK);      // compile EXIT
    compile(MW_EXIT);
    compile(MW_COMMA);
    compile(MW_HIDDEN);    // toggle hidden-bit
    compile(MW_LBRAC);     // back to immediate mode
    compile(MW_EXIT);
}

void Interpreter::compile(fith_cell c, bool machine)
{
    fith_cell &here=bin[HEREAT];
    if(size_t(here) >= binsz){
        // no room, ignore
        return;
    }

    bin[here++]=c | (machine ? FLAG_MACHINE : 0);
}

void Interpreter::create(const string &name, fith_cell value)
{
#ifndef NDEBUG
    cerr << name << " = " << value << endl;
#endif
    
    dictionary[name]=value;
    latestword=name;
}

fith_cell Interpreter::find(const string &name) const
{
    dci i=dictionary.find(name);
    if(i == dictionary.end()){
        return -1;
    }

    return i->second;
}

const string &Interpreter::latest() const
{
    return latestword;
}



void Interpreter::Context::mw_storecode()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
    size_t ptr=(size_t) dstk[dsp-1];
    if(ptr >= interp.binsz){
        state=Interpreter::EX_SEGV_CODE;
        return;
    }
    interp.bin[ptr]=dstk[dsp-2];
    dsp-=2;
}

void Interpreter::Context::mw_readcode()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
    size_t ptr=(size_t) dstk[dsp-1];
    if(ptr >= interp.binsz){
        state=Interpreter::EX_SEGV_CODE;
    }
    dstk[dsp-1]=interp.bin[ptr];
}

void Interpreter::Context::mw_comma()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
    fith_cell &here=interp.bin[HEREAT];
    if(size_t(here) >= interp.binsz){
        state=Interpreter::EX_SEGV_CODE;
        return;
    }

    // copy into the binary
    interp.bin[here++]=dstk[--dsp];
}

void Interpreter::Context::mw_key()
{
    if(dsp >= dsz){
        state=Interpreter::EX_DSTK_OVER;
        return;
    }

    char c;
    interp.is.get(c);  // blocking read 1 char
    if(!interp.is){
        // fail, eof, etc
        c=0;
    }
    // push result as int
    dstk[dsp++]=(fith_cell) c;
}

void Interpreter::Context::mw_emit()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
    interp.os.put((char)(dstk[--dsp] & 0xFF));
    
}

void Interpreter::Context::mw_word()
{
    if(dsp >= dsz){
        state=Interpreter::EX_DSTK_OVER;
        return;
    }
    
    string str;
    interp.is >> str; // it's nice to cheat...

    if(!interp.is){
        // EOF/fail
        interp.heap[WORDLENAT]=-1;
        ((char *) &interp.heap[WORDBUFAT])[0]='\0';
    }
    else{
        // truncate
        size_t len=str.length();
        if(len > WORDSZ*4-1){
            len=WORDSZ*4-1;
            str.resize(len);
        }
        // copy length into heap into fixed-size buffer
        interp.heap[WORDLENAT]=len;
        // copy string data plus NUL
        memcpy((char *) &interp.heap[WORDBUFAT], str.c_str(), len+1);
    }

#ifndef NDEBUG
    cerr << "word " << str << endl;
#endif
    
    // push ptr
    dstk[dsp++]=WORDLENAT;
}

void Interpreter::Context::mw_number()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }
    else if(dsp >= dsz){
        state=Interpreter::EX_DSTK_OVER;
        return;
    }

    // get pointer to C-string
    const char *str=interp.get_string(dstk[dsp-1]);
    // invalid or empty
    if(str == NULL || str[0] == '\0'){
        dstk[dsp-1]=0;  // result
        dstk[dsp++]=-1; // "number of unconverted chars"
        return;
    }

    char *endptr;
    long res=strtol(str, &endptr, 0);
    size_t len=strlen(str);

    dstk[dsp-1]=res;
    dstk[dsp++]=len-(endptr-str);
}

void Interpreter::Context::mw_dot()
{
    if(dsp < 1){
        state=EX_DSTK_UNDER;
        return;
    }
    interp.os << dstk[--dsp] << ' ';
}

void Interpreter::Context::mw_tell()
{
    if(dsp < 1){
        state=EX_DSTK_UNDER;
        return;
    }
    const char *str=interp.get_string(dstk[--dsp]);
    if(str != NULL){
        interp.os << str;
    }
}

void Interpreter::Context::mw_create()
{
    if(dsp < 2){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }

    fith_cell ptr=dstk[--dsp];
    const char *str=interp.get_string(dstk[--dsp]);
    
    if(str == NULL){
        state=Interpreter::EX_SEGV_DATA;
        return;
    }

    interp.create(str, ptr);
}

const char *Interpreter::get_string(fith_cell p)
{
    size_t ptr=size_t(p);
    
    if(ptr >= heapsz){
        // invalid ptr
        return NULL;
    }

    // retrieve string-len
    size_t len=size_t(heap[ptr]);
    if(ptr+1+((len+3)>>2) >= heapsz){
        // invalid length
        return NULL;
    }
    // ptr to data
    const char *sp=(const char *) &heap[ptr+1];
    if(sp[len] != '\0'){
        // no trailing NUL at expected location
        return NULL;
    }

    return sp;
}

void Interpreter::Context::mw_find()
{
    if(dsp < 1){
        state=Interpreter::EX_DSTK_UNDER;
        return;
    }

    // pointer to string object
    const char *p=interp.get_string(dstk[dsp-1]);
    // valid?
    if(p == NULL){
        state=Interpreter::EX_SEGV_DATA;
        return;
    }

    // lookup & overwrite TOS
    dstk[dsp-1]=interp.find(p);
}

void Interpreter::Context::mw_immediate()
{
    di i=interp.dictionary.find(interp.latestword);
    if(i != interp.dictionary.end()){
        i->second ^= FLAG_IMMED;
    }
}

void Interpreter::Context::mw_hidden()
{
    di i=interp.dictionary.find(interp.latestword);
    if(i != interp.dictionary.end()){
        i->second ^= FLAG_HIDE;
    }
}

void Interpreter::Context::mw_lbrac()
{
    interp.compilestate=false;
}

void Interpreter::Context::mw_rbrac()
{
    interp.compilestate=true;
}

void Interpreter::Context::mw_state()
{
    if(dsp >= dsz){
        state=Interpreter::EX_DSTK_OVER;
        return;
    }
    dstk[dsp++]=interp.compilestate ? 1 : 0;
}

void Interpreter::Context::mw_interpret()
{
    // cerr << "compilestate " << interp.compilestate << endl;
    
    // get word
    mw_word();
    if(interp.heap[WORDLENAT] < 1){
        --dsp;
        return; // nothing good
    }
    // word ptr is on stack

    // look it up
    mw_find();
    fith_cell wordptr=dstk[dsp-1];
    if(wordptr != -1){
        // got it; ptr to word is on stack
        if(!interp.compilestate || (wordptr & FLAG_IMMED) != 0){
            // is immediate or am in immediate mode, so call it
            // cerr << "interp immed word\n";
            mw_call();
            return;
        }
        else{
            // cerr << "interp compile word\n";
            // compile it.
            mw_comma();
        }
    }
    else{
        // drop the find-failure-flag, replace it with the word-pointer
        dstk[dsp-1]=WORDLENAT;
        
        // not found.  is it a number?
        mw_number();
        // cerr << "num: " << dstk[dsp-2] << ", " << dstk[dsp-1] << endl;
        if(dstk[dsp-1] == 0){
            // parse success. drop the success flag; have pushed number
            --dsp;

            // compile mode: LIT number
            if(interp.compilestate){
                // cerr << "interp compile lit\n";
                interp.compile(MW_LIT);
                mw_comma();
            }
            else{
                // cerr << "interp immed lit\n";
            }
            // else leave the number on the stack
        }
        else{
            // parse failure; whinge
            dsp-=2;            
            interp.os << "Unrecognised word " << ((char *) &interp.heap[WORDBUFAT]) << endl;
            return;
        }
    }
    
}

#endif


};
