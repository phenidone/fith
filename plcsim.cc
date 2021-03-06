/** -*- C++ -*- */

#include "fithi.h"
#include "fithfile.h"
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <sys/time.h>
#include <sys/signal.h>

using namespace fith;
using namespace std;

const size_t BINSZ=65536;
const size_t HEAPSZ=128;
const size_t STKSZ=128;
fith_cell bin[BINSZ];
fith_cell heap[HEAPSZ];
fith_cell dstk[STKSZ], cstk[STKSZ];
size_t dsp=0, csp=0;

/**
 * Syscalls implementation that does PLC stuff.
 *
 * Allows a single on-change handler and single periodic-timer.
 * 
 * Has a finite set of both input and output ports; 
 * really intended for use with just 1 each way.
 */
class PLCSC : public SysCalls {
public:

    
    PLCSC(Interpreter &in)
        : interp(in)
    {
        gpio_handler=0;
        periodic_handler=0;
        
        for(size_t i=0;i<INPORTS;++i)
            inputs[i]=0;
        for(size_t i=0;i<OUTPORTS;++i)
            outputs[i]=0;

        gettimeofday(&t_boot, NULL);
        me=this;
    }

    ~PLCSC()
    {
        if(me == this)
            me=NULL;
    }
    
    virtual fith_cell syscall1(fith_cell a)
    {
        struct timeval now, dt;

        gettimeofday(&now, NULL);
        
        switch(a){
        case SC1_TIME_UNIX:
            return fith_cell(now.tv_sec);
        case SC1_TIME_EPOCH:
            return fith_cell(now.tv_sec-EPOCH);
        case SC1_TIME_MSBOOT:            
            timersub(&now, &t_boot, &dt);
            return 1000*dt.tv_sec+dt.tv_usec/1000;
        default:
            break;
        }
        return -1;
    }
    
    virtual fith_cell syscall2(fith_cell a, fith_cell b)
    {
        switch(b){
        case SC2_GPIO_READ:
            if(a >= 0 && size_t(a) < INPORTS){
                return inputs[a];
            }
            break;
        default:
            break;
        }
        return -1;
    }
    
    virtual fith_cell syscall3(fith_cell a, fith_cell b, fith_cell c)
    {
        switch(c){
        case SC3_GPIO_WRITE:
            if(b >= 0 && size_t(b) < OUTPORTS){
                outputs[b]=a;
                refreshView();
                return 0;
            }
            break;
        case SC3_GPIO_HANDLER:
            gpio_handler=b;
            return 0;
        case SC3_TIMER_PERIODIC:
        {
            periodic_handler=b;

            struct itimerval tv;
            tv.it_value.tv_sec=0;
            tv.it_value.tv_usec=0;
            tv.it_interval=tv.it_value;

            if(setitimer(ITIMER_REAL, &tv, NULL) < 0){
                return -1;
            }
            tv.it_value.tv_sec=a/1000;
            tv.it_value.tv_usec=(a % 1000)*1000;
            tv.it_interval=tv.it_value;

            if(setitimer(ITIMER_REAL, &tv, NULL) < 0){
                periodic_handler=0;
                return -1;
            }
            return 0;
        }
        default:
            break;
        }
        return -1;
    }

    /**
     * Input has changed! (external event, called from outside interpreter
     */
    void changeInput(int which, fith_cell value)
    {
        if(which < 0 || size_t(which) >= INPORTS)
            return;
        
        inputs[which]=value;
        refreshView();
        
        if(gpio_handler != 0){
            // run the on-change handler, passing port-number on stack
            call(gpio_handler, which);
        }
    }


    fith_cell getInput(int which)
    {
        if(which < 0 || size_t(which) >= INPORTS)
            return -1;
        
        return inputs[which];
    }

    fith_cell getOutput(int which)
    {
        if(which < 0 || size_t(which) >= OUTPORTS)
            return -1;
        
        return outputs[which];
    }

    /**
     * timer event, run a thread if not already doing so
     */
    void ontimer()
    {
        if(periodic_handler){
            call(periodic_handler, 0);
        }
    }

    static PLCSC *me;

private:

    /**
     * Print an update of the IO state to cout
     */
    void refreshView()
    {
        cout << "\rIN ";
        binary(cout, inputs[0]);
        cout << " OUT ";
        binary(cout, outputs[0]);
        cout << endl;
    }

    void binary(ostream &os, unsigned i)
    {
        for(int k=31;k>=0;--k){
            os << ((i&(1<<k))?"1":"0");
        }
    }
    
    /**
     * Run a thread in the interpreter.
     */
    void call(fith_cell entry, fith_cell param)
    {
        dsp=csp=0;
        dstk[dsp++]=param; 
        Interpreter::Context ctx(entry, &dstk[0], &cstk[0], dsp, csp,
                                 STKSZ, STKSZ, interp);
        Interpreter::EXEC_RESULT res=ctx.execute();        
        if(res != Interpreter::EX_SUCCESS){
            cerr << endl << "GPIO on-change callback failed, status=" << res << endl;
        }

    }

    static const size_t INPORTS=1, OUTPORTS=1;
    
    static const fith_cell SC2_GPIO_READ=0x1000;
    static const fith_cell SC3_GPIO_WRITE=0x1001;
    static const fith_cell SC3_GPIO_HANDLER=0x1010;

    static const fith_cell SC1_TIME_UNIX=0x2000;
    static const fith_cell SC1_TIME_EPOCH=0x2001;
    static const fith_cell SC1_TIME_MSBOOT=0x2002;
    static const fith_cell SC3_TIMER_PERIODIC=0x2010;

    Interpreter &interp;
    
    fith_cell gpio_handler, periodic_handler;
    fith_cell inputs[INPORTS];
    fith_cell outputs[OUTPORTS];

    struct timeval t_boot;
    static const time_t EPOCH=946684800;        ///< year 2000, 30 year offset from Unix
};

PLCSC *PLCSC::me=NULL;

void ontimer(int sig)
{
    signal(sig, ontimer);       // reinstall

    if(PLCSC::me){
        PLCSC::me->ontimer();
    }
}


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
        case FithOutFile::SEG_DATA:
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
            throw runtime_error("loaded binary too large (DATA)");
        }
        heap[0]=count;
        memcpy(heap+1, pcell, (count-1)*4);

        state |= GOT_DATA;
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
    static const unsigned GOT_DATA=2;
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

    signal(SIGALRM, ontimer);
    
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
    else{
        cerr << "No binary loaded" << endl;
    }
    
    // create bootstrapped interpreter
    Interpreter interp(bin, BINSZ, heap, HEAPSZ, bs);
    PLCSC plcsc(interp);    
    Interpreter::EXEC_RESULT res;

    interp.setSyscalls(&plcsc);
    
    
    // create new thread to run boot code
    Interpreter::Context ctx(entptr, &dstk[0], &cstk[0], dsp, csp,
                             STKSZ, STKSZ, interp);
    res=ctx.execute();        
    if(res != Interpreter::EX_SUCCESS){
        cerr << endl << "Failed, status=" << res << endl;
        return 1;
    }

    // wait for char IO, for state changes
    while(true){
        char c;
        cin.get(c);

        if(tolower(c) == 'q')
            break;
        
        if(isdigit(c)){
            int bit=c-'0';

            // toggle an input GPIO bit
            plcsc.changeInput(0, plcsc.getInput(0) ^ (1<<bit));
        }
    }
    
    return 0;
}
