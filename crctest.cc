
#include "crc.h"
#include <iostream>
#include <iomanip>

using namespace std;

int main()
{
    CRC32STM crc;
#if 1
    unsigned buf[1024];
    unsigned long count=0;

    while(cin.good()){
        cin.read((char *) &buf[0], sizeof(buf));
        unsigned got=cin.gcount();
       
        crc.insert(&buf[0], got>>2);
        count+=got & ~3;
        if(got & 3){
            cerr << "Warn: discarded " << (got & 3) << " bytes" << endl;
        }
    }

    cout << count << ": 0x" << setw(8) << setfill('0') << hex << crc.remainder() << endl;

    crc.insert(crc.remainder());
    cout << "check=" << crc.remainder() << endl;
    
#else
    const char *test="1\0\0\0\0\0\0\0\0";
    crc.insert((const unsigned *) test, 1);
    cout << "0x" << setw(8) << setfill('0') << hex << crc.remainder() << endl;
#endif
    return 0;
}
