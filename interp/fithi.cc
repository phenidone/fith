/** -*- C++ -*- */

#include "fithi.h"
#include <iostream>

using namespace std;

namespace fith {

Interpreter::Interpreter(fith_cell *_bin, size_t _binsz, fith_cell *_heap, size_t _heapsz)
    : bin(_bin), heap(_heap), binsz(_binsz), heapsz(_heapsz)
{
    // clear heap
    for(size_t i=0;i<heapsz;++i){
        heap[i]=0;
    }
}


const Interpreter::Context::machineword_t Interpreter::Context::builtin[Interpreter::MW_INTERP_COUNT]={
    &Interpreter::Context::mw_exit,
    &Interpreter::Context::mw_lit,
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
};

Interpreter::Context::Context(size_t _ip, fith_cell *_dstk, fith_cell *_rstk, size_t _dsp, size_t _rsp, size_t _dsz, size_t _rsz, Interpreter &_interp)
    : ip(_ip), dsp(_dsp), rsp(_rsp), state(EX_RUNNING), dstk(_dstk), rstk(_rstk), dsz(_dsz), rsz(_rsz), interp(_interp)
{
}

Interpreter::EXEC_RESULT Interpreter::Context::execute()
{
    while(state == EX_RUNNING){
        if(ip >= interp.binsz){
            state=EX_SEGV_CODE;
            break;
        }
        fith_cell ins=interp.bin[ip];

#ifndef NDEBUG
        cerr << ip << ":" << ins << endl;
#endif
        ++ip;
        
        if(ins <= 0){
            // builtin
            ins=-ins;
            if(ins >= MW_INTERP_COUNT){
                state=EX_BAD_OPCODE;
                break;
            }
            // do it.
            (this->*builtin[ins])();
        }
        else{
            // word to call

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
    
    if(tgt < 0){
        // builtin word, just run it
        // gonna be fun if it's MW_CALL :)
        tgt=-tgt;
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


};
