
#ifndef _CRC32_H_
#define _CRC32_H_

#include <cstdlib>

/**
 * Compute CRC32 on a stream of data, in the same
 * configuration that the STM32F1xx does it.  The purpose is to
 * generate CRCs that can be verified by an STM32F1.
 *
 * Because of the endianness-fussiness and the requirement to
 * process only whole 32-bit words on STM32, this code also
 * processes only whole words.
 *
 * This is a variant of CRC32-MPEG2 in big-endian mode:
 * poly = 0x04C11DB7
 * initial = 0xFFFFFFF
 * final XOR = 0
 * input/output reflection: none
 * byte order = swapped (big endian)
 */
class CRC32STM {
public:

    /// initialize a new CRC computation
    CRC32STM();

    /// insert a word, as big-endian
    void insert(unsigned i);
    
    /// insert an array of words, including byte-swaps
    /// @param p pointer to data
    /// @param count number of 32-bit words
    void insert(const unsigned *p, size_t count);

    /// get current remainder (checksum) after inserting trailing zeroes
    unsigned remainder();
    
private:

    /// insert a single byte
    inline void insert8(unsigned char d)
    {
        state=(state << 8) ^ TABLE[(state>>24) ^ d];
    }

    unsigned state;

    static const unsigned TABLE[256];
};

#endif  // _CRC32_H_
