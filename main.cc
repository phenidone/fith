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

    // create bootstrapped interpreter
    Interpreter interp(bin, BINSZ, heap, HEAPSZ, bs);
    Interpreter::EXEC_RESULT res;
    
    if(bs){
        // load+run boostrap.5th
        bootstrap(interp);
        entptr=interp.find("QUIT");        
    }


    // create new thread to run chosen code
    Interpreter::Context ctx(entptr, &dstk[0], &cstk[0], dsp, csp,
                             STKSZ, STKSZ, interp,
                             cin, cout);
    res=ctx.execute();        

    if(res != Interpreter::EX_SUCCESS){
        ctx.printdump(cerr);
        return 0;
    }
   
    return 0;
}
