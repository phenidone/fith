/** -*- C++ -*- */

#include "fithi.h"
#include <iostream>
#include <fstream>

using namespace fith;
using namespace std;

const size_t BINSZ=1024;
const size_t HEAPSZ=128;
const size_t STKSZ=128;
fith_cell bin[BINSZ];
fith_cell heap[HEAPSZ];
fith_cell dstk[STKSZ], cstk[STKSZ];
size_t dsp=0, csp=0;

const string BOOTSTRAP_5TH="bootstrap.5th";

void bootstrap(Interpreter &interp)
{
    ifstream ifs(BOOTSTRAP_5TH.c_str(), ios::in);

    if(ifs){
        // create thread
        Interpreter::Context ctx(interp.find("QUIT"), &dstk[0], &cstk[0], dsp, csp,
                                 STKSZ, STKSZ, interp,
                                 ifs, cout);

        Interpreter::EXEC_RESULT res=ctx.execute();
        ifs.close();
        
        cerr << "bootstrap complete" << endl;
    }
    else{
        cerr << "could not load " << BOOTSTRAP_5TH << endl;
    }
}

int main()
{
    // create bootstrapped interpreter
    Interpreter interp(bin, BINSZ, heap, HEAPSZ, true);

    // load+run boostrap.5th
    bootstrap(interp);

    // create new thread to run interp-loop
    Interpreter::Context ctx(interp.find("QUIT"), &dstk[0], &cstk[0], dsp, csp,
                             STKSZ, STKSZ, interp,
                             cin, cout);
    Interpreter::EXEC_RESULT res=ctx.execute();

    if(res == Interpreter::EX_SUCCESS){
        cout << "exited?" << endl;
    }
    else{
        cout << "fail " << res << endl;
        return 0;
    }

   
    return 0;
}
