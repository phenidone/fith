/** -*- C++ -*- */

#include "fithi.h"
#include "fithfile.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdexcept>

using namespace fith;
using namespace std;

const size_t BINSZ=65536;
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
                                 &ifs, &cout);

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
 * Dummy implementation which replaces stream IO, and does some things
 * which are required in a PLC simulator
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

/**
 * Callback-handler for loading a file
 */
class Loader : public FithInFile::SegmentHandler {
public:

    /// @param entname, optional name of the entry-point (obtain from map)
    Loader(const string &entname)
        : state(0), entry(0), entryname(entname)
    {
    }

    /// file is opened, validate versions
    virtual void onHeader(unsigned binver, unsigned iover)
    {
        state=0;
        if(binver != Interpreter::BINVERSION){
            throw runtime_error("Invalid BINVERSION");
        }
        if(iover != Interpreter::IOVERSION){
            throw runtime_error("Invalid IOVERSION");
        }
    }

    /// got a segmentx
    virtual void onSegment(FithOutFile::SEGTYPES kind, fith_cell *pcell, unsigned count)
    {
        switch(kind){
        case FithOutFile::SEG_TEXT:
            loadText(pcell, count);
            break;
        case FithOutFile::SEG_BSS:
            loadBss(pcell, count);
            break;
        case FithOutFile::SEG_ENTRY:
            if(entryname.length() == 0 && count == 2){
                // use the ENTRY segment only if user has not specified name of entry point
                entry=pcell[0];
                state |= GOT_ENTRY;
            }
            break;
        case FithOutFile::SEG_MAP:
            parseMap(pcell, count);
            break;
        default:
            cerr << "Ignoring segment-type " << hex << kind << endl;
        }
    }

    /// where do we run from?
    fith_cell getEntry() const { return entry; }

    /// load complete and valid?
    bool success() const { return state == GOT_ALL; }
    
private:

    void loadText(fith_cell *pcell, unsigned count)
    {
        if(count+1 > BINSZ){
            throw runtime_error("loaded binary too large (TEXT)");
        }
        bin[0]=count;
        memcpy(bin+1, pcell, (count-1)*4);

        state |= GOT_TEXT;
    }

    void loadBss(fith_cell *pcell, unsigned count)
    {
        if(count+1 > HEAPSZ){
            throw runtime_error("loaded binary too large (BSS)");
        }
        heap[0]=count;
        memcpy(heap+1, pcell, (count-1)*4);

        state |= GOT_BSS;
    }

    void parseMap(fith_cell *pcell, unsigned count)
    {
        // don't bother with map unless we need it
        if(entryname.length() == 0){
            return;
        }

        // don't want to include the header-size
        --count;
        
        char *pstr=(char *) pcell;
        if(pstr[count*4-1] != '\0'){
            throw runtime_error("bad string termination in MAP segment");
        }

        // parse the map
        istringstream iss(pstr);
        while(!iss.eof()){
            unsigned long addr;
            string word;
            iss >> hex >> addr >> word;

            // cerr << word << "=" << addr << endl;
            
            // found the entry-point in the map
            if(word == entryname){
                entry=fith_cell(addr);
                state |= GOT_ENTRY;
                break;
            }       
        }
    }

    // bits in state, tracking load-progress
    static const unsigned GOT_TEXT=1;
    static const unsigned GOT_BSS=2;
    static const unsigned GOT_ENTRY=4;
    static const unsigned GOT_ALL=7;
    
    unsigned state;
    fith_cell entry;
    string entryname;
};

int main(int argc, char *argv[])
{
    fith_cell entptr=-1;
    bool bs=true;
    
    if(argc > 2 && strcmp(argv[1], "-r") == 0){
        string load=argv[2];
        string entname;
        if(argc > 3){
            entname=argv[3];
        }

        ifstream ifs;
        Loader loader(entname);
        try{
            ifs.open(load.c_str(), ios::in);
            if(!ifs){
                throw runtime_error(string("Can't open ")+load);
            }
            FithInFile::readFile(ifs, loader);
        }
        catch(runtime_error &e){
            cerr << e.what() << endl;
            return 1;
        }
        ifs.close();

        if(loader.success()){
            entptr=loader.getEntry();
            bs=false;  // bootstrap not required as we have an entry point into valid binary
        }
        else{
            cerr << "Loader(" << load << ") failed" << endl;
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
    
    // create interpreter
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
                             , &cin, &cout
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
