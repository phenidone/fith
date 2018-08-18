/** -*- C++ -*- */

#include "fithi.h"
#include <iostream>

using namespace fith;
using namespace std;

const size_t BINSZ=128;
const size_t HEAPSZ=128;
const size_t STKSZ=128;
fith_cell bin[BINSZ];
fith_cell heap[HEAPSZ];
fith_cell dstk[STKSZ], cstk[STKSZ];
size_t dsp=0, csp=0;

void dpush(fith_cell i)
{
    dstk[dsp++]=i;
}

size_t dubble, quad, pythag, quit;

void compile_tests(Interpreter &interp)
{
    // : double 2 * ;
    dubble=interp.here();
    interp.create("DOUBLE", dubble);
    interp.compile(-Interpreter::MW_LIT);
    interp.compile(2);
    interp.compile(-Interpreter::MW_MUL);
    interp.compile(-Interpreter::MW_EXIT);

    // : quad double double ;
    quad=interp.here();
    interp.create("QUAD", quad);
    interp.compile(dubble);
    interp.compile(dubble);
    interp.compile(-Interpreter::MW_EXIT);

    // : PYTHAG DUP * SWAP DUP * + ;
    pythag=interp.here();
    interp.create("PYTHAG", pythag);
    interp.compile(-Interpreter::MW_DUP);
    interp.compile(-Interpreter::MW_MUL);
    interp.compile(-Interpreter::MW_SWAP);
    interp.compile(-Interpreter::MW_DUP);
    interp.compile(-Interpreter::MW_MUL);
    interp.compile(-Interpreter::MW_PLUS);
    interp.compile(-Interpreter::MW_EXIT);

    // : QUIT INTERPRET JMP(-4) ;
    quit=interp.here();
    interp.create("QUIT", quit);
    interp.compile(interp.find("INTERPRET"));
    interp.compile(-Interpreter::MW_JMP);
    interp.compile(-1);  // inf loop
    interp.compile(-Interpreter::MW_EXIT);  // waste
}

int main()
{
    // create bootstrapped interpreter
    Interpreter interp(bin, BINSZ, heap, HEAPSZ, cin, cout);

    // manually write some code
    compile_tests(interp);

    // put some numbers on the stack
    dpush(3);
    dpush(4);

    // create thread
    Interpreter::Context ctx(pythag, &dstk[0], &cstk[0], dsp, csp, STKSZ, STKSZ, interp);

    // run!
    Interpreter::EXEC_RESULT res=ctx.execute();

    if(res == Interpreter::EX_SUCCESS){
        cout << "> " << dstk[0] << endl;
    }
    else{
        cout << "fail " << res << endl;
        return 0;
    }

    // go again
    ctx.set_ip(quad);
    res=ctx.execute();

    if(res == Interpreter::EX_SUCCESS){
        cout << "> " << dstk[0] << endl;
    }
    else{
        cout << "fail " << res << endl;
        return 0;
    }

    // start interpreter
    ctx.set_ip(quit);
    res=ctx.execute();

    if(res == Interpreter::EX_SUCCESS){
        cout << "exited?" << endl;
    }
    else{
        cout << "fail " << res << endl;
        return 0;
    }

   
    return 0;
}
