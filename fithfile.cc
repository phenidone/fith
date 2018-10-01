
#include "fithfile.h"
#include <stdexcept>
#include <cassert>

using namespace std;

namespace fith {

FithOutFile::FithOutFile(ostream &_os, unsigned segs, unsigned binver, unsigned iover)
    : os(_os), segs(0)
{
    hdr.magic=MAGIC;
    hdr.fileversion=1;
    hdr.binversion=binver;
    hdr.ioversion=iover;
    hdr.segcount=segs;

    assert(sizeof(hdr) == 20);
    
    crc.insert((const unsigned *) &hdr, sizeof(hdr)/4);
    os.write((const char *) &hdr, sizeof(hdr));
}

void FithOutFile::writeText(fith_cell *pcell)
{
    writeSegment(SEG_TEXT, pcell+1, pcell[0]);
}

void FithOutFile::writeData(fith_cell *pcell)
{
    writeSegment(SEG_DATA, pcell+1, pcell[0]);
}

void FithOutFile::writeConfig(fith_cell *pcell)
{
    writeSegment(SEG_CONFIG, pcell+1, pcell[0]);
}

void FithOutFile::writeMap(const string &str)
{
    unsigned len=str.length();

    // pad out with between 1 and 4 NULs
    unsigned words=(len >> 2) + 1;
    fith_cell *pstr=new fith_cell[words];
    memset(pstr, 0, words*4);
    memcpy(pstr, str.c_str(), len);

    writeSegment(SEG_MAP, pstr, words+1);
    delete[] pstr;
}

void FithOutFile::writeEntry(fith_cell root)
{
    writeSegment(SEG_ENTRY, &root, 2);
}

void FithOutFile::writeCrc()
{
    unsigned checksum=crc.remainder();
    writeSegment(SEG_CRC, (fith_cell *) &checksum, 2);
}

void FithOutFile::writeSegment(unsigned kind, fith_cell *pcell, unsigned count)
{
    if(++segs > hdr.segcount){
        throw range_error("too many segments in FITH file");
    }

    crc.insert(kind);
    crc.insert(count);
    crc.insert((const unsigned *) pcell, count-1);

    os.write((const char *) &kind, 4);
    os.write((const char *) &count, 4);
    os.write((const char *) pcell, (count-1)*4);
}

void FithInFile::readFile(istream &is, FithInFile::SegmentHandler &sh)
{
    FithOutFile::header hdr;
    CRC32STM crc;

    // read and validate file header
    checkRead(is, crc, (unsigned *) &hdr, sizeof(hdr)/4);
    if(hdr.magic != FithOutFile::MAGIC){
        throw runtime_error("FithInFile bad magic");
    }
    // callback
    sh.onHeader(hdr.binversion, hdr.ioversion);

    // for each segment...
    for(unsigned i=0;i<hdr.segcount;++i){
        // CRC doesn't include its own segment header, so keep old value
        unsigned precheck=crc.remainder();
        unsigned kind, count;
        // read segment header
        checkRead(is, crc, &kind, 1);
        checkRead(is, crc, &count, 1);
        // attempt to allocate segment data
        unsigned *data=new unsigned[count-1];
        if(!data){
            throw runtime_error("FithInFile allocation failed");
        }

        try{
            // read segment data
            checkRead(is, crc, data, count-1);

            if(kind == FithOutFile::SEG_CRC){
                // CRC check
                if(precheck != data[0]){
                    throw runtime_error("FithInFile CRC check fails");
                }
                else{
                    // cerr << "FithInFile CRC OK" << endl;
                }
            }
            else{
                // any other segment is passed to handler
                sh.onSegment(FithOutFile::SEGTYPES(kind), (fith_cell *) data, count);
            }
            delete[] data;
        }
        catch(...){
            delete[] data;
            throw;
        }
    }
}

void FithInFile::checkRead(std::istream &is, CRC32STM &crc, unsigned *data, unsigned wordcount)
{
    is.read((char *) data, wordcount<<2);
    if(!is || is.gcount() < wordcount<<2){
        throw runtime_error("FithInFile read failed");
    }
    crc.insert(data, wordcount);
}

} // namespace fith
