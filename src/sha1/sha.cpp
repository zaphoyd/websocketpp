/*
 *  sha.cpp
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved
 *
 *****************************************************************************
 *  $Id: sha.cpp 13 2009-06-22 20:20:32Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This utility will display the message digest (fingerprint) for
 *      the specified file(s).
 *
 *  Portability Issues:
 *      None.
 */

#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "sha1.h"

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
    unsigned    message_digest[5];          // Message digest from "sha"
    int         i;                          // Counter
    bool        reading_stdin;              // Are we reading standard in?
    bool        read_stdin = false;         // Have we read stdin?

    /*
     *  Check the program arguments and print usage information if -?
     *  or --help is passed as the first argument.
     */
    if (argc > 1 && (!strcmp(argv[1],"-?") || !strcmp(argv[1],"--help")))
    {
        usage();
        return 1;
    }

    /*
     *  For each filename passed in on the command line, calculate the
     *  SHA-1 value and display it.
     */
    for(i = 0; i < argc; i++)
    {
        /*
         *  We start the counter at 0 to guarantee entry into the for loop.
         *  So if 'i' is zero, we will increment it now.  If there is no
         *  argv[1], we will use STDIN below.
         */
        if (i == 0)
        {
            i++;
        }

        if (argc == 1 || !strcmp(argv[i],"-"))
        {
#ifdef WIN32
            _setmode(_fileno(stdin), _O_BINARY);
#endif
            fp = stdin;
            reading_stdin = true;
        }
        else
        {
            if (!(fp = fopen(argv[i],"rb")))
            {
                fprintf(stderr, "sha: unable to open file %s\n", argv[i]);
                return 2;
            }
            reading_stdin = false;
        }

        /*
         *  We do not want to read STDIN multiple times
         */
        if (reading_stdin)
        {
            if (read_stdin)
            {
                continue;
            }

            read_stdin = true;
        }

        /*
         *  Reset the SHA1 object and process input
         */
        sha.Reset();

        c = fgetc(fp);
        while(!feof(fp))
        {
            sha.Input(c);
            c = fgetc(fp);
        }

        if (!reading_stdin)
        {
            fclose(fp);
        }

        if (!sha.Result(message_digest))
        {
            fprintf(stderr,"sha: could not compute message digest for %s\n",
                    reading_stdin?"STDIN":argv[i]);
        }
        else
        {
            printf( "%08X %08X %08X %08X %08X - %s\n",
                    message_digest[0],
                    message_digest[1],
                    message_digest[2],
                    message_digest[3],
                    message_digest[4],
                    reading_stdin?"STDIN":argv[i]);
        }
    }

    return 0;
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
    printf("usage: sha <file> [<file> ...]\n");
    printf("\tThis program will display the message digest (fingerprint)\n");
    printf("\tfor files using the Secure Hashing Algorithm (SHA-1).\n");
}
