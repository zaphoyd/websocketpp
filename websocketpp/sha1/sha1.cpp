/*
 *  sha1.cpp
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved.
 *
 *****************************************************************************
 *  $Id: sha1.cpp 12 2009-06-22 19:34:25Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This class implements the Secure Hashing Standard as defined
 *      in FIPS PUB 180-1 published April 17, 1995.
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
 */


#include "sha1.h"

/*  
 *  SHA1
 *
 *  Description:
 *      This is the constructor for the sha1 class.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
SHA1::SHA1()
{
    Reset();
}

/*  
 *  ~SHA1
 *
 *  Description:
 *      This is the destructor for the sha1 class
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
SHA1::~SHA1()
{
    // The destructor does nothing
}

/*  
 *  Reset
 *
 *  Description:
 *      This function will initialize the sha1 class member variables
 *      in preparation for computing a new message digest.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
void SHA1::Reset()
{
    Length_Low          = 0;
    Length_High         = 0;
    Message_Block_Index = 0;

    H[0]        = 0x67452301;
    H[1]        = 0xEFCDAB89;
    H[2]        = 0x98BADCFE;
    H[3]        = 0x10325476;
    H[4]        = 0xC3D2E1F0;

    Computed    = false;
    Corrupted   = false;
}

/*  
 *  Result
 *
 *  Description:
 *      This function will return the 160-bit message digest into the
 *      array provided.
 *
 *  Parameters:
 *      message_digest_array: [out]
 *          This is an array of five unsigned integers which will be filled
 *          with the message digest that has been computed.
 *
 *  Returns:
 *      True if successful, false if it failed.
 *
 *  Comments:
 *
 */
bool SHA1::Result(unsigned *message_digest_array)
{
    int i;                                  // Counter

    if (Corrupted)
    {
        return false;
    }

    if (!Computed)
    {
        PadMessage();
        Computed = true;
    }

    for(i = 0; i < 5; i++)
    {
        message_digest_array[i] = H[i];
    }

    return true;
}

/*  
 *  Input
 *
 *  Description:
 *      This function accepts an array of octets as the next portion of
 *      the message.
 *
 *  Parameters:
 *      message_array: [in]
 *          An array of characters representing the next portion of the
 *          message.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
void SHA1::Input(   const unsigned char *message_array,
                    unsigned            length)
{
    if (!length)
    {
        return;
    }

    if (Computed || Corrupted)
    {
        Corrupted = true;
        return;
    }

    while(length-- && !Corrupted)
    {
        Message_Block[Message_Block_Index++] = (*message_array & 0xFF);

        Length_Low += 8;
        Length_Low &= 0xFFFFFFFF;               // Force it to 32 bits
        if (Length_Low == 0)
        {
            Length_High++;
            Length_High &= 0xFFFFFFFF;          // Force it to 32 bits
            if (Length_High == 0)
            {
                Corrupted = true;               // Message is too long
            }
        }

        if (Message_Block_Index == 64)
        {
            ProcessMessageBlock();
        }

        message_array++;
    }
}

/*  
 *  Input
 *
 *  Description:
 *      This function accepts an array of octets as the next portion of
 *      the message.
 *
 *  Parameters:
 *      message_array: [in]
 *          An array of characters representing the next portion of the
 *          message.
 *      length: [in]
 *          The length of the message_array
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
void SHA1::Input(   const char  *message_array,
                    unsigned    length)
{
    Input((unsigned char *) message_array, length);
}

/*  
 *  Input
 *
 *  Description:
 *      This function accepts a single octets as the next message element.
 *
 *  Parameters:
 *      message_element: [in]
 *          The next octet in the message.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
void SHA1::Input(unsigned char message_element)
{
    Input(&message_element, 1);
}

/*  
 *  Input
 *
 *  Description:
 *      This function accepts a single octet as the next message element.
 *
 *  Parameters:
 *      message_element: [in]
 *          The next octet in the message.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
void SHA1::Input(char message_element)
{
    Input((unsigned char *) &message_element, 1);
}

/*  
 *  operator<<
 *
 *  Description:
 *      This operator makes it convenient to provide character strings to
 *      the SHA1 object for processing.
 *
 *  Parameters:
 *      message_array: [in]
 *          The character array to take as input.
 *
 *  Returns:
 *      A reference to the SHA1 object.
 *
 *  Comments:
 *      Each character is assumed to hold 8 bits of information.
 *
 */
SHA1& SHA1::operator<<(const char *message_array)
{
    const char *p = message_array;

    while(*p)
    {
        Input(*p);
        p++;
    }

    return *this;
}

/*  
 *  operator<<
 *
 *  Description:
 *      This operator makes it convenient to provide character strings to
 *      the SHA1 object for processing.
 *
 *  Parameters:
 *      message_array: [in]
 *          The character array to take as input.
 *
 *  Returns:
 *      A reference to the SHA1 object.
 *
 *  Comments:
 *      Each character is assumed to hold 8 bits of information.
 *
 */
SHA1& SHA1::operator<<(const unsigned char *message_array)
{
    const unsigned char *p = message_array;

    while(*p)
    {
        Input(*p);
        p++;
    }

    return *this;
}

/*  
 *  operator<<
 *
 *  Description:
 *      This function provides the next octet in the message.
 *
 *  Parameters:
 *      message_element: [in]
 *          The next octet in the message
 *
 *  Returns:
 *      A reference to the SHA1 object.
 *
 *  Comments:
 *      The character is assumed to hold 8 bits of information.
 *
 */
SHA1& SHA1::operator<<(const char message_element)
{
    Input((unsigned char *) &message_element, 1);

    return *this;
}

/*  
 *  operator<<
 *
 *  Description:
 *      This function provides the next octet in the message.
 *
 *  Parameters:
 *      message_element: [in]
 *          The next octet in the message
 *
 *  Returns:
 *      A reference to the SHA1 object.
 *
 *  Comments:
 *      The character is assumed to hold 8 bits of information.
 *
 */
SHA1& SHA1::operator<<(const unsigned char message_element)
{
    Input(&message_element, 1);

    return *this;
}

/*  
 *  ProcessMessageBlock
 *
 *  Description:
 *      This function will process the next 512 bits of the message
 *      stored in the Message_Block array.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      Many of the variable names in this function, especially the single
 *      character names, were used because those were the names used
 *      in the publication.
 *
 */
void SHA1::ProcessMessageBlock()
{
    const unsigned K[] =    {               // Constants defined for SHA-1
                                0x5A827999,
                                0x6ED9EBA1,
                                0x8F1BBCDC,
                                0xCA62C1D6
                            };
    int         t;                          // Loop counter
    unsigned    temp;                       // Temporary word value
    unsigned    W[80];                      // Word sequence
    unsigned    A, B, C, D, E;              // Word buffers

    /*
     *  Initialize the first 16 words in the array W
     */
    for(t = 0; t < 16; t++)
    {
        W[t] = ((unsigned) Message_Block[t * 4]) << 24;
        W[t] |= ((unsigned) Message_Block[t * 4 + 1]) << 16;
        W[t] |= ((unsigned) Message_Block[t * 4 + 2]) << 8;
        W[t] |= ((unsigned) Message_Block[t * 4 + 3]);
    }

    for(t = 16; t < 80; t++)
    {
       W[t] = CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
    }

    A = H[0];
    B = H[1];
    C = H[2];
    D = H[3];
    E = H[4];

    for(t = 0; t < 20; t++)
    {
        temp = CircularShift(5,A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        temp &= 0xFFFFFFFF;
        E = D;
        D = C;
        C = CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 20; t < 40; t++)
    {
        temp = CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
        temp &= 0xFFFFFFFF;
        E = D;
        D = C;
        C = CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 40; t < 60; t++)
    {
        temp = CircularShift(5,A) +
               ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        temp &= 0xFFFFFFFF;
        E = D;
        D = C;
        C = CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 60; t < 80; t++)
    {
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

    Message_Block_Index = 0;
}

/*  
 *  PadMessage
 *
 *  Description:
 *      According to the standard, the message must be padded to an even
 *      512 bits.  The first padding bit must be a '1'.  The last 64 bits
 *      represent the length of the original message.  All bits in between
 *      should be 0.  This function will pad the message according to those
 *      rules by filling the message_block array accordingly.  It will also
 *      call ProcessMessageBlock() appropriately.  When it returns, it
 *      can be assumed that the message digest has been computed.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
void SHA1::PadMessage()
{
    /*
     *  Check to see if the current message block is too small to hold
     *  the initial padding bits and length.  If so, we will pad the
     *  block, process it, and then continue padding into a second block.
     */
    if (Message_Block_Index > 55)
    {
        Message_Block[Message_Block_Index++] = 0x80;
        while(Message_Block_Index < 64)
        {
            Message_Block[Message_Block_Index++] = 0;
        }

        ProcessMessageBlock();

        while(Message_Block_Index < 56)
        {
            Message_Block[Message_Block_Index++] = 0;
        }
    }
    else
    {
        Message_Block[Message_Block_Index++] = 0x80;
        while(Message_Block_Index < 56)
        {
            Message_Block[Message_Block_Index++] = 0;
        }

    }

    /*
     *  Store the message length as the last 8 octets
     */
    Message_Block[56] = (Length_High >> 24) & 0xFF;
    Message_Block[57] = (Length_High >> 16) & 0xFF;
    Message_Block[58] = (Length_High >> 8) & 0xFF;
    Message_Block[59] = (Length_High) & 0xFF;
    Message_Block[60] = (Length_Low >> 24) & 0xFF;
    Message_Block[61] = (Length_Low >> 16) & 0xFF;
    Message_Block[62] = (Length_Low >> 8) & 0xFF;
    Message_Block[63] = (Length_Low) & 0xFF;

    ProcessMessageBlock();
}


/*  
 *  CircularShift
 *
 *  Description:
 *      This member function will perform a circular shifting operation.
 *
 *  Parameters:
 *      bits: [in]
 *          The number of bits to shift (1-31)
 *      word: [in]
 *          The value to shift (assumes a 32-bit integer)
 *
 *  Returns:
 *      The shifted value.
 *
 *  Comments:
 *
 */
unsigned SHA1::CircularShift(int bits, unsigned word)
{
    return ((word << bits) & 0xFFFFFFFF) | ((word & 0xFFFFFFFF) >> (32-bits));
}
