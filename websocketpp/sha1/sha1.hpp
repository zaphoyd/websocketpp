/*
 * sha1.hpp
 *
 * Copyright (C) 1998, 2009
 * Paul E. Jones <paulej@packetizer.com>
 * All Rights Reserved.
 *
 * Modifications were done in 2012-13 by Peter Thorson (webmaster@zaphoyd.com)
 * to allow header only usage of the library and use C++ features to better
 * support C++ usage. These changes are distributed under the original freeware
 * license included below
 *
 * Freeware Public License (FPL)
 *
 * This software is licensed as "freeware."  Permission to distribute
 * this software in source and binary forms, including incorporation
 * into other products, is hereby granted without a fee.  THIS SOFTWARE
 * IS PROVIDED 'AS IS' AND WITHOUT ANY EXPRESSED OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE AUTHOR SHALL NOT BE HELD
 * LIABLE FOR ANY DAMAGES RESULTING FROM THE USE OF THIS SOFTWARE, EITHER
 * DIRECTLY OR INDIRECTLY, INCLUDING, BUT NOT LIMITED TO, LOSS OF DATA
 * OR DATA BEING RENDERED INACCURATE.
 *
 *****************************************************************************
 *
 *  Description:
 *      This class implements the Secure Hashing Standard as defined
 *      in FIPS PUB 180-1 published April 17, 1995.
 *
 *      Many of the variable names in this class, especially the single
 *      character names, were used because those were the names used
 *      in the publication.
 *
 *      The Secure Hashing Standard, which uses the Secure Hashing
 *      Algorithm (SHA), produces a 160-bit message digest for a
 *      given data stream.  In theory, it is highly improbable that
 *      two messages will produce the same message digest.  Therefore,
 *      this algorithm can serve as a means of providing a "fingerprint"
 *      for a message.
 *
 *  Portability Issues:
 *      SHA-1 is defined in terms of 32-bit "words".  This code was
 *      written with the expectation that the processor has at least
 *      a 32-bit machine word size.  If the machine word size is larger,
 *      the code should still function properly.  One caveat to that
 *      is that the input functions taking characters and character arrays
 *      assume that only 8 bits of information are stored in each character.
 *
 *  Caveats:
 *      SHA-1 is designed to work with messages less than 2^64 bits long.
 *      Although SHA-1 allows a message digest to be generated for
 *      messages of any number of bits less than 2^64, this implementation
 *      only works with messages with a length that is a multiple of 8
 *      bits.
 *
 *****************************************************************************
 */

#ifndef _SHA1_H_
#define _SHA1_H_

#include <websocketpp/common/stdint.hpp>

namespace websocketpp {

/// Provides SHA1 hashing functionality
class sha1 {
public:
    sha1() {
        reset();
    }

    virtual ~sha1() {}

    /// Re-initialize the class
    void reset() {
        length_low = 0;
        length_high = 0;
        message_block_index = 0;

        H[0] = 0x67452301;
        H[1] = 0xEFCDAB89;
        H[2] = 0x98BADCFE;
        H[3] = 0x10325476;
        H[4] = 0xC3D2E1F0;

        computed = false;
        corrupted = false;
    }

    /// Extract the message digest as a raw integer array
    /**
     * @param [out] message_digest_array Integer array to store the message in
     * @return Whether or not the extraction was sucessful
     */
    bool get_raw_digest(uint32_t * message_digest_array) {
        if (corrupted) {
            return false;
        }

        if (!computed) {
            pad_message();
            computed = true;
        }

        for (int i = 0; i < 5; i++) {
            message_digest_array[i] = H[i];
        }

        return true;
    }

    /// Provide input to SHA1
    /**
     * @param [in] message_array The input bytes
     * @param [in] length Length of the message array
     */
    void input(unsigned char const * message_array, uint32_t length) {
        if (!length) {
            return;
        }

        if (computed || corrupted) {
            corrupted = true;
            return;
        }

        while (length-- && !corrupted) {
// Suppresses Visual Studio code analysis for write overrun. It doesn't know the
// index into Message_Block is protected by checking length.
//
// TODO: is there a more compatible way to write the original code to avoid
//       this sort of warning?
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(suppress: 6386)
#endif
            message_block[message_block_index++] = (*message_array & 0xFF);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

            length_low += 8;
            length_low &= 0xFFFFFFFF;               // Force it to 32 bits
            if (length_low == 0) {
                length_high++;
                length_high &= 0xFFFFFFFF;          // Force it to 32 bits
                if (length_high == 0) {
                    corrupted = true;               // Message is too long
                }
            }

            if (message_block_index == 64) {
                process_message_block();
            }

            message_array++;
        }
    }

    /// Provide input to SHA1
    /**
     * Overload with signed message array
     *
     * @param [in] message_array The input bytes
     * @param [in] length Length of the message array
     */
    void input(char const * message_array, uint32_t length) {
        input(reinterpret_cast<const unsigned char *>(message_array), length);
    }

    /// Provide input to SHA1
    /**
     * Overload with a single unsigned char
     *
     * @param [in] message_element The character to input
     */
    void input(unsigned char message_element) {
        input(&message_element, 1);
    }

    /// Provide input to SHA1
    /**
     * Overload with a single signed char
     *
     * @param [in] message_element The character to input
     */
    void input(char message_element) {
        input(reinterpret_cast<unsigned char *>(&message_element), 1);
    }

    /// Provide input to SHA1
    /**
     * Overload via signed char array stream input
     *
     * @param [in] message_array The character array to input
     */
    sha1& operator<<(char const * message_array) {
        char const * p = message_array;

        while(*p) {
            input(*p);
            p++;
        }

        return *this;
    }

    /// Provide input to SHA1
    /**
     * Overload via unsigned char array stream input
     *
     * @param [in] message_array The character array to input
     */
    sha1& operator<<(unsigned char const * message_array) {
        unsigned char const * p = message_array;

        while(*p) {
            input(*p);
            p++;
        }

        return *this;
    }

    /// Provide input to SHA1
    /**
     * Overload via signed char stream input
     *
     * @param [in] message_element The character to input
     */
    sha1& operator<<(char message_element) {
        input((unsigned char *) &message_element, 1);

        return *this;
    }

    /// Provide input to SHA1
    /**
     * Overload via unsigned char stream input
     *
     * @param [in] message_element The character to input
     */
    sha1& operator<<(unsigned char message_element) {
        input(&message_element, 1);

        return *this;
    }
private:
    /// Process the next 512 bits of the message
    /**
     * Many of the variable names in this function, especially the single
     * character names, were used because those were the names used
     * in the publication.
     */
    void process_message_block() {
        // Constants defined for SHA-1
        uint32_t const K[] = { 0x5A827999,
                               0x6ED9EBA1,
                               0x8F1BBCDC,
                               0xCA62C1D6 };
        uint32_t temp;          // Temporary word value
        uint32_t W[80];         // Word sequence
        uint32_t A, B, C, D, E; // Word buffers

        // Initialize the first 16 words in the array W
        for (int t = 0; t < 16; t++) {
            W[t] = ((uint32_t) message_block[t * 4]) << 24;
            W[t] |= ((uint32_t) message_block[t * 4 + 1]) << 16;
            W[t] |= ((uint32_t) message_block[t * 4 + 2]) << 8;
            W[t] |= ((uint32_t) message_block[t * 4 + 3]);
        }

        for (int t = 16; t < 80; t++) {
           W[t] = CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
        }

        A = H[0];
        B = H[1];
        C = H[2];
        D = H[3];
        E = H[4];

        for (int t = 0; t < 20; t++) {
            temp = CircularShift(5,A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
            temp &= 0xFFFFFFFF;
            E = D;
            D = C;
            C = CircularShift(30,B);
            B = A;
            A = temp;
        }

        for (int t = 20; t < 40; t++) {
            temp = CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
            temp &= 0xFFFFFFFF;
            E = D;
            D = C;
            C = CircularShift(30,B);
            B = A;
            A = temp;
        }

        for (int t = 40; t < 60; t++) {
            temp = CircularShift(5,A) +
                   ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
            temp &= 0xFFFFFFFF;
            E = D;
            D = C;
            C = CircularShift(30,B);
            B = A;
            A = temp;
        }

        for (int t = 60; t < 80; t++) {
            temp = CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
            temp &= 0xFFFFFFFF;
            E = D;
            D = C;
            C = CircularShift(30,B);
            B = A;
            A = temp;
        }

        H[0] = (H[0] + A) & 0xFFFFFFFF;
        H[1] = (H[1] + B) & 0xFFFFFFFF;
        H[2] = (H[2] + C) & 0xFFFFFFFF;
        H[3] = (H[3] + D) & 0xFFFFFFFF;
        H[4] = (H[4] + E) & 0xFFFFFFFF;

        message_block_index = 0;
    }

    /// Pads the current message block to 512 bits
    /**
     * According to the standard, the message must be padded to an even
     * 512 bits.  The first padding bit must be a '1'.  The last 64 bits
     * represent the length of the original message.  All bits in between
     * should be 0.  This function will pad the message according to those
     * rules by filling the message_block array accordingly.  It will also
     * call ProcessMessageBlock() appropriately.  When it returns, it
     * can be assumed that the message digest has been computed.
     */
    void pad_message() {
        // Check to see if the current message block is too small to hold
        // the initial padding bits and length.  If so, we will pad the
        // block, process it, and then continue padding into a second block.
        if (message_block_index > 55) {
            message_block[message_block_index++] = 0x80;
            while(message_block_index < 64) {
                message_block[message_block_index++] = 0;
            }

            process_message_block();

            while(message_block_index < 56) {
                message_block[message_block_index++] = 0;
            }
        } else {
            message_block[message_block_index++] = 0x80;
            while(message_block_index < 56) {
                message_block[message_block_index++] = 0;
            }

        }

        // Store the message length as the last 8 octets
        message_block[56] = (length_high >> 24) & 0xFF;
        message_block[57] = (length_high >> 16) & 0xFF;
        message_block[58] = (length_high >> 8) & 0xFF;
        message_block[59] = (length_high) & 0xFF;
        message_block[60] = (length_low >> 24) & 0xFF;
        message_block[61] = (length_low >> 16) & 0xFF;
        message_block[62] = (length_low >> 8) & 0xFF;
        message_block[63] = (length_low) & 0xFF;

        process_message_block();
    }

    /// Performs a circular left shift operation
    /**
     * @param [in] bits How many bits to shift
     * @param [in] word The word to shift
     * @return The shifted word
     */
    inline uint32_t CircularShift(int bits, uint32_t word) {
        return ((word << bits) & 0xFFFFFFFF) | ((word & 0xFFFFFFFF) >> (32-bits));
    }

    uint32_t H[5];                      // Message digest buffers

    uint32_t length_low;                // Message length in bits
    uint32_t length_high;               // Message length in bits

    unsigned char message_block[64];    // 512-bit message blocks
    int message_block_index;            // Index into message block array

    bool computed;                      // Is the digest computed?
    bool corrupted;                     // Is the message digest corruped?

};

} // namespace websocketpp

#endif // _SHA1_H_
