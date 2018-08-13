/** -*- C++ -*- */

#include "fithi.h"

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
    &Interpreter::Context::mw_and,
    &Interpreter::Context::mw_or,
    &Interpreter::Context::mw_xor,
    &Interpreter::Context::mw_invert,
    &Interpreter::Context::mw_sl,
    &Interpreter::Context::mw_sra,
    &Interpreter::Context::mw_srl,
    &Interpreter::Context::mw_store,
    &Interpreter::Context::mw_read
};

Interpreter::Context::Context(size_t _ip, fith_cell *_dstk, fith_cell *_cstk, size_t _dsp, size_t _csp, size_t _dsz, size_t _csz, Interpreter &_interp)
    : ip(_ip), dsp(_dsp), csp(_csp), state(EX_RUNNING), dstk(_dstk), cstk(_cstk), dsz(_dsz), csz(_csz), interp(_interp)
{
    // consider clearing the stacks?
}

Interpreter::EXEC_RESULT Interpreter::Context::execute()
{
    while(state == EX_RUNNING){
        if(ip >= interp.binsz){
            state=EX_SEGV_CODE;
            break;
        }
        fith_cell ins=interp.bin[ip];

        if(ins <= 0){
            // builtin
            ins=-ins;
            if(ins >= MW_INTERP_COUNT){
                state=EX_BAD_OPCODE;
                break;
            }
            // do it.
            ++ip;
            machineword_t bi=builtin[ins];
            (this->*bi)();
        }
        else{
            // word to call

            // push return address and jump
            if(csp >= csz){
                state=EX_CSTK_OVER;
                break;
            }
            
            cstk[csp++]=ip+1;
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
    if(csp == 0){
        // returned from top-level function
        state=Interpreter::EX_SUCCESS;
    }
    else{
        // jump back
        ip=cstk[--csp];
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

}

void Interpreter::Context::mw_mulmod()
{

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

void Interpreter::Context::mw_lt()
{

}

void Interpreter::Context::mw_gt()
{

}

void Interpreter::Context::mw_le()
{

}

void Interpreter::Context::mw_ge()
{

}

void Interpreter::Context::mw_eq()
{

}

void Interpreter::Context::mw_max()
{

}

void Interpreter::Context::mw_min()
{

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

void Interpreter::Context::mw_and()
{

}

void Interpreter::Context::mw_or()
{

}

void Interpreter::Context::mw_xor()
{

}

void Interpreter::Context::mw_invert()
{

}

void Interpreter::Context::mw_sl()
{

}

void Interpreter::Context::mw_sra()
{

}

void Interpreter::Context::mw_srl()
{

}

void Interpreter::Context::mw_store()
{

}

void Interpreter::Context::mw_read()
{

}


};
