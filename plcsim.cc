/** -*- C++ -*- */

#include "fithi.h"
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
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
 * Dummy implementation which does PLC stuff.
 *
 * Allows a single on-change handler, single periodic-timer
 * and single oneshot-timer.
 */
template<int inp, int outp>
class PLCSC : public SysCalls {
public:

    
    PLCSC(Interpreter &in)
        : interp(in)
    {
        gpio_handler=0;
        periodic_handler=0;
        oneshot_handler=0;
        
        for(size_t i=0;i<inp;++i)
            inputs[i]=0;
        for(size_t i=0;i<outp;++i)
            outputs[i]=0;
    }

    virtual fith_cell syscall1(fith_cell a)
    {
        return -1;
    }
    
    virtual fith_cell syscall2(fith_cell a, fith_cell b)
    {
        switch(b){
        case SC2_GPIO_READ:
            if(a >= 0 && a < inp){
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
            if(b >= 0 && b < outp){
                outputs[b]=a;
                refreshView();
                return 0;
            }
            break;
        case SC3_GPIO_HANDLER:
            gpio_handler=b;
            return 0;
        case SC3_TIMER_PERIODIC:
        case SC3_TIMER_ONESHOT:
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
        if(which < 0 || which >= inp)
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
        if(which < 0 || which >= inp)
            return -1;
        
        return inputs[which];
    }

    fith_cell getOutput(int which)
    {
        if(which < 0 || which >= outp)
            return -1;
        
        return outputs[which];
    }

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
    
    static const fith_cell SC2_GPIO_READ=0x1000;
    static const fith_cell SC3_GPIO_WRITE=0x1001;
    static const fith_cell SC3_GPIO_HANDLER=0x1010;

    static const fith_cell SC3_TIMER_PERIODIC=0x2010;
    static const fith_cell SC3_TIMER_ONESHOT=0x2011;

    Interpreter &interp;
    
    fith_cell gpio_handler, periodic_handler, oneshot_handler;
    fith_cell inputs[inp];
    fith_cell outputs[outp];
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
    
    // create bootstrapped interpreter
    Interpreter interp(bin, BINSZ, heap, HEAPSZ, bs);
    PLCSC<1,1> plcsc(interp);
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
