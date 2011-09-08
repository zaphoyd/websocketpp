/*
 *  shacmp.cpp
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved
 *
 *****************************************************************************
 *  $Id: shacmp.cpp 12 2009-06-22 19:34:25Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This utility will compare two files by producing a message digest
 *      for each file using the Secure Hashing Algorithm and comparing
 *      the message digests.  This function will return 0 if they
 *      compare or 1 if they do not or if there is an error.
 *      Errors result in a return code higher than 1.
 *
 *  Portability Issues:
 *      none.
 *
 */

#include <stdio.h>
#include <string.h>
#include "sha1.h"

/*
 *  Return codes
 */
#define SHA1_COMPARE        0
#define SHA1_NO_COMPARE     1
#define SHA1_USAGE_ERROR    2
#define SHA1_FILE_ERROR     3

/*
 *  Function prototype
 */
void usage();

/*  
 *  main
 *
 *  Description:
 *      This is the entry point for the program
 *
 *  Parameters:
 *      argc: [in]
 *          This is the count of arguments in the argv array
 *      argv: [in]
 *          This is an array of filenames for which to compute message digests
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
int main(int argc, char *argv[])
{
    SHA1        sha;                        // SHA-1 class
    FILE        *fp;                        // File pointer for reading files
    char        c;                          // Character read from file
    unsigned    message_digest[2][5];       // Message digest for files
    int         i;                          // Counter
    bool        message_match;              // Message digest match flag
    int         returncode;

    /*
     *  If we have two arguments, we will assume they are filenames.  If
     *  we do not have to arguments, call usage() and exit.
     */
    if (argc != 3)
    {
        usage();
        return SHA1_USAGE_ERROR;
    }

    /*
     *  Get the message digests for each file
     */
    for(i = 1; i <= 2; i++)
    {
        sha.Reset();

        if (!(fp = fopen(argv[i],"rb")))
        {
            fprintf(stderr, "sha: unable to open file %s\n", argv[i]);
            return SHA1_FILE_ERROR;
        }

        c = fgetc(fp);
        while(!feof(fp))
        {
            sha.Input(c);
            c = fgetc(fp);
        }

        fclose(fp);

        if (!sha.Result(message_digest[i-1]))
        {
            fprintf(stderr,"shacmp: could not compute message digest for %s\n",
                    argv[i]);
            return SHA1_FILE_ERROR;
        }
    }

    /*
     *  Compare the message digest values
     */
    message_match = true;
    for(i = 0; i < 5; i++)
    {
        if (message_digest[0][i] != message_digest[1][i])
        {
            message_match = false;
            break;
        }
    }

    if (message_match)
    {
        printf("Fingerprints match:\n");
        returncode = SHA1_COMPARE;
    }
    else
    {
        printf("Fingerprints do not match:\n");
        returncode = SHA1_NO_COMPARE;
    }

    printf( "\t%08X %08X %08X %08X %08X\n",
            message_digest[0][0],
            message_digest[0][1],
            message_digest[0][2],
            message_digest[0][3],
            message_digest[0][4]);
    printf( "\t%08X %08X %08X %08X %08X\n",
            message_digest[1][0],
            message_digest[1][1],
            message_digest[1][2],
            message_digest[1][3],
            message_digest[1][4]);

    return returncode;
}

/*  
 *  usage
 *
 *  Description:
 *      This function will display program usage information to the user.
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
void usage()
{
    printf("usage: shacmp <file> <file>\n");
    printf("\tThis program will compare the message digests (fingerprints)\n");
    printf("\tfor two files using the Secure Hashing Algorithm (SHA-1).\n");
}
