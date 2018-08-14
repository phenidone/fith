/** -*- C++ -*- */

#include "fithi.h"
#include <iostream>

using namespace fith;
using namespace std;

const size_t BINSZ=128;
const size_t HEAPSZ=128;
const size_t STKSZ=128;
fith_cell bin[BINSZ];
size_t binptr=0;
fith_cell heap[HEAPSZ];
fith_cell dstk[STKSZ], cstk[STKSZ];
size_t dsp=0, csp=0;

void compile(fith_cell i)
{
    bin[binptr++]=i;
}

void dpush(fith_cell i)
{
    dstk[dsp++]=i;
}

size_t compile_tests()
{
    // dummy entry so first func is not at zero and looks like EXIT when called
    compile(0);
    
    // : double 2 * ;
    size_t dubble=binptr;
    compile(-Interpreter::MW_LIT);
    compile(2);
    compile(-Interpreter::MW_MUL);
    compile(-Interpreter::MW_EXIT);

    // : quad double double ;
    size_t quad=binptr;
    compile(dubble);
    compile(dubble);
    compile(-Interpreter::MW_EXIT);

    // : PYTHAG DUP * SWAP DUP * + QUAD ;
    size_t pythag=binptr;
    compile(-Interpreter::MW_DUP);
    compile(-Interpreter::MW_MUL);
    compile(-Interpreter::MW_SWAP);
    compile(-Interpreter::MW_DUP);
    compile(-Interpreter::MW_MUL);
    compile(-Interpreter::MW_PLUS);
    compile(quad);
    compile(-Interpreter::MW_EXIT);

    return pythag;
}

int main()
{
    // manually write some code
    size_t test=compile_tests();
    
    Interpreter interp(bin, binptr, heap, HEAPSZ);

    // put some numbers onna stack
    dpush(3);
    dpush(4);

    // create thread
    Interpreter::Context ctx(test, &dstk[0], &cstk[0], dsp, csp, STKSZ, STKSZ, interp);

    // run
    Interpreter::EXEC_RESULT res=ctx.execute();

    cout << "exec " << res << endl;
    if(res == Interpreter::EX_SUCCESS){
        cout << "result = " << dstk[0] << endl;
    }
    
    return 0;
}
