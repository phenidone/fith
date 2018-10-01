
#ifndef _FITHFILE_H_
#define _FITHFILE_H_

#include <iostream>
#include <string>
#include "crc.h"
#include "fithi.h"

namespace fith {

class FithInFile;
    
/**
 * A means of writing a saved program.  Handles the formatting as well
 * as generation of CRC or (in future) a signature.
 */
class FithOutFile {
public:

    FithOutFile(std::ostream &_os, unsigned segs, unsigned binver, unsigned iover);

    /// write a text-segment (length in first field)
    void writeText(fith_cell *pcell);
    /// write a data-segment (length in first field)
    void writeData(fith_cell *pcell);
    /// write a config-segment (length in first field)
    void writeConfig(fith_cell *pcell);
    /// write the program map
    void writeMap(const std::string &mapstr);
    /// write a program-entry tag
    void writeEntry(fith_cell root);
    /// append a CRC segment
    void writeCrc();

    enum SEGTYPES {
        SEG_TEXT=0x101,
        SEG_DATA=0x102,
        SEG_CONFIG=0x103,
        SEG_ENTRY=0x104,
        SEG_MAP=0x105,

        SEG_CRC=0x110,
    };
    
private:

    /// generic segment
    void writeSegment(unsigned kind, fith_cell *pcell, unsigned count);

    struct header {
        unsigned magic;
        unsigned fileversion;
        unsigned binversion;
        unsigned ioversion;
        unsigned segcount;
    };
    
    std::ostream &os;
    CRC32STM crc;
    unsigned segs;
    header hdr;

  
    static const unsigned MAGIC=0x48544946;

    // share header etc.
    friend class FithInFile;
};

/**
 * Means of reading a saved program.
 */
class FithInFile {
public:

    /// callback interface
    class SegmentHandler {
    public:
        /// file is opened.  Throw something if we don't like the versions
        virtual void onHeader(unsigned binver, unsigned iover) =0;
        
        /// receive a segment.
        /// the content of pcell will be freed after this returns!  so copy/retain it
        virtual void onSegment(FithOutFile::SEGTYPES kind, fith_cell *pcell, unsigned count) =0;
    };

    /// read a file, passing contents to the specified handler
    static void readFile(std::istream &, SegmentHandler &);
    
private:

    /// read a block of data, check that we got enough, and CRC it
    static void checkRead(std::istream &is, CRC32STM &crc, unsigned *data, unsigned wordcount);
};

} // namespace fith

#endif  // _FITHFILE_H_
