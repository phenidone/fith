/** -*- C++ -*- */

#include "fithi.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>

using namespace fith;
using namespace std;

const size_t BINSZ=1024;
const size_t HEAPSZ=128;
const size_t STKSZ=128;
fith_cell bin[BINSZ];
fith_cell heap[HEAPSZ];
fith_cell dstk[STKSZ], cstk[STKSZ];
size_t dsp=0, csp=0;

#ifdef FULLFITH

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
        if(res == Interpreter::EX_SUCCESS){
        }
        else{
            cerr << "bootstrap failed" << endl;
            ctx.printdump(cerr);
            exit(1);
        }
    }
    else{
        cerr << "could not load " << BOOTSTRAP_5TH << endl;
    }
}

#endif

/**
 * read a saved binary image, for which the first word is the
 * total number of words in the image.
 */
void readblob(const string &fn, fith_cell *to, fith_cell alloc)
{
    ifstream ifs;
    try{
        ifs.open(fn.c_str(), ios::in | ios::binary);
        if(!ifs){
            throw runtime_error(string("can't open ")+fn);
        }
        ifs.read((char *) to, sizeof(fith_cell));
        if(!ifs){
            throw runtime_error(string("can't read ")+fn);
        }
        if(to[0] > alloc || to[0] < 0){
            throw runtime_error(string("invalid length prefix in ")+fn);
        }
        ifs.read((char *) (to+1), (to[0]-1)*sizeof(fith_cell));
        if(!ifs){
            throw runtime_error(string("can't read ")+fn);
        }
        ifs.close();
    }
    catch(runtime_error &e){
        if(ifs.is_open()){
            ifs.close();
        }
    }
}

/**
 * Dummy implementation which replaces stream IO!
 */
class IOSC : public SysCalls {
public:
    virtual fith_cell syscall1(fith_cell a)
    {
        char x;
        
        switch(a){
        case 0:     // does KEY functionality
            cin.get(x);
            return (!cin) ? -1 : x;
        default:
            return -1;
        }
        return 0;
    }
    
    virtual fith_cell syscall2(fith_cell a, fith_cell b)
    {
        switch(b){
        case 0:      // does EMIT functionalitu
            cout << char(a & 0xFF);
            break;
        default:
            cerr << "SYSCALL2(" << a << ", " << b << ")" << endl;
            return -1;
        }
        return 0;
    }
    
    virtual fith_cell syscall3(fith_cell a, fith_cell b, fith_cell c)
    {
        return 0;
    }

};

int main(int argc, char *argv[])
{
    fith_cell entptr=-1;
    bool bs=true;
    
    if(argc > 3 && strcmp(argv[1], "-r") == 0){
        string load=argv[2];
        string entry=argv[3];

        try{
            readblob(load+".bin", &bin[0], BINSZ);
            readblob(load+".dat", &heap[0], HEAPSZ);            
            ifstream ifs((load+".map").c_str(), ios::in);

            // load the map
            while(!ifs.eof()){
                unsigned long addr;
                string word;
                ifs >> hex >> addr >> word;

                // found the entry-point in the map
                if(word == entry){
                    entptr=fith_cell(addr);
                }                
            }
            ifs.close();

            if(entptr == -1){
                throw runtime_error(string("Could not find entry point ")+entry);
            }

            bs=false;
        }
        catch(runtime_error &e){
            cerr << e.what() << endl;
            return 1;
        }
    }
#ifndef FULLFITH
    else{
        // no command-line args, can't bootstrap
        cerr << "embedded interpreter cannot bootstrap, use:" << endl
             << "\t" << argv[0] << " -r file ENTRYPOINT" << endl
             << "to load and run stored FITH code" << endl;
        return 1;
    }
#endif
    
    // create bootstrapped interpreter
    Interpreter interp(bin, BINSZ, heap, HEAPSZ, bs);
    IOSC iosc;
    Interpreter::EXEC_RESULT res;

    interp.setSyscalls(&iosc);
    
#ifdef FULLFITH
    if(bs){
        // load+run boostrap.5th
        bootstrap(interp);
        entptr=interp.find("QUIT");        
    }
#endif
    
    // create new thread to run chosen code
    Interpreter::Context ctx(entptr, &dstk[0], &cstk[0], dsp, csp,
                             STKSZ, STKSZ, interp
#ifdef FULLFITH
                             , cin, cout
#endif
        );
    res=ctx.execute();        

    if(res != Interpreter::EX_SUCCESS){
#ifdef FULLFITH
        ctx.printdump(cerr);
#else
        cerr << endl << "Failed, status=" << res << endl;
#endif        
    }
    return 0;
}
