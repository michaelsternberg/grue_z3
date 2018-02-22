/******************************************************************************
 * FILE NAME: interlz3.c 
 *
 * PURPOSE: 
 * Create an Apple II .dsk disk image file using an Infocom 
 * Z-machine interpreter and Z-code (story file) as input.
 * The main task is writing the Z-code in the sector-interleaving
 * order expected by Infocom's Z-machine interpreters for the 
 * Apple II.
 * 
 * This code is based on  work by Steve Nickolas.
 * The original "Interl" was written in QBASIC for DOS/Windows
 * and is available at:
 * http://ifarchive.org/indexes/if-archiveXinfocomXtools.html
 *
 * NOTES:
 * To obtain a Z-machine interpreter
 *  1. Find an Infocom disk image having a ZIP (v3) story file (Deadline). 
 *  2. Find the minor version (A, B, ..., M) by playing the game 
 *     and entering $VERIFY at the parser prompt. 
 *  3. head --bytes 12288 deadline.dsk > info3a.bin (where a is the minor 
 *     version in this example).
 * 
 * To create v3 Z-code, use 'inform 6.1.5' and compatible inform libraries.
 *
 * DEVELOPMENT HISTORY:
 * 2017-05-28  Initial port from QBASIC to C
 * 2017-07-11  General clean up
 * 2017-10-24  Added support for ZIP (E) interpreter - non-interleaved sectors
 *****************************************************************************/

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <libgen.h>

#define STUBSIZE 12288

/*****************************************************************************/
void display_usage()
{
    fprintf(stderr, "(Required parameters missing:3 -h for help)\n");
}
/*****************************************************************************/
void display_help()
{
    fprintf(stderr, "Converts an Infocom Z3-type (the most common) data ");
    fprintf(stderr, "file into an Apple\n");
    fprintf(stderr, "disk image.  You need to have the 12K interpreter ");
    fprintf(stderr, "stub from the beginning\n");
    fprintf(stderr, "of an existing 'DOS-order' (.dsk/.do) disk image.\n\n");
    fprintf(stderr, "interlz3 stub.bin source.dat target.dsk\n\n");
    fprintf(stderr, "Example:  For interpreter stub info3m.bin and data ");
    fprintf(stderr, "file minizork.z3 to\n");
    fprintf(stderr, "          generate minizork.dsk, enter the following ");
    fprintf(stderr, "command:\n\n");
    fprintf(stderr, "interlz3 info3m.bin minizork.z3 minizork.dsk\n");
}
/*****************************************************************************/
char check_args(int argc, char* argv[])
{
    bool rc = true;
    char *cvalue = NULL;
    int c;

    opterr = 0;

    while ((c = getopt (argc, argv, "qh")) != -1)
    {
        switch (c)
        {
            case 'h':
                display_help();
                rc = false;
                break;
        }
    }

    if (rc == true && argc != 4)
    {
        display_usage();
        rc = false;
    }

    return rc;
}
/*****************************************************************************/
bool validate_zip(char* exe, char* filename)
{
    bool rc = true;
    struct stat st;

    stat(filename, &st);

    if (st.st_size != STUBSIZE)
    {
        fprintf( stderr
               , "%s: (Stub file [%s] invalid - size [%lu], not %d)\n"
               , basename(exe)
               , filename
               , st.st_size
               , STUBSIZE
               );
        rc = false;
    }

    return rc;
}
/*****************************************************************************/
bool assert_fopen(char* argv[], FILE* fp, int argc)
{
    bool rc = true;

    if (!fp)
    {
        fprintf( stderr
               , "%s: Unable to open file [%s]\n"
               , basename(argv[0])
               , argv[argc]
               );

        rc = false;
    }

    return rc;
}
/*****************************************************************************/
unsigned int copy_zip(FILE* zip, FILE* tgt)
{
    unsigned char ch;
    unsigned int c = 0;

    ch = fgetc(zip);
    while (!feof(zip))
    {
        c++;
        fputc(ch, tgt);
        ch = fgetc(zip);
    }

    if (c != STUBSIZE)
    {
        fprintf( stderr
               , "[%d] bytes written. [%d] expected\n"
               , c
               , STUBSIZE
               );
    }
    return c;
}
/*****************************************************************************/
void display_scoreboard(char* argv[])
{
    printf("\n");
    printf(" ─╥─ ╥─╖ ╥ ╓─╥─╖ ╥───╖ ╥───╖ ╥\n");
    printf("  ║  ║ ║ ║   ║   ║     ║   ║ ║     INFOCOM DATA FILE RE-INTERLEAVE\n");
    printf("  ║  ║ ║ ║   ║   ║     ║   ║ ║     AND APPLE II DISK IMAGE MAKER\n");
    printf("  ║  ║ ║ ║   ║   ╟─╢   ╟──╥╜ ║\n");
    printf("  ║  ║ ║ ║   ║   ║     ║  ║  ║     BASED ON THE WORK OF THE USOTSUKI\n");
    printf("  ║  ║ ║ ║   ║   ║     ║  ║  ║     (c) 2002 DOSIUS SOFTWARE CO.\n");
    printf(" ─╨─ ╨ ╙─╜   ╨   ╨───╜ ╨  ╙╜ ╨───╜ PORTED TO C BY M.STERNBERG (2017)\n\n");
    printf("Creating disk:  %s\n", argv[3]);
    printf("From data file: %s\n", argv[2]);
    printf("Using stub:     %s\n\n", argv[1]);
}
/*****************************************************************************/
bool interleave(FILE* zip, FILE* src, FILE* tgt)
{
    bool rc = true;
    unsigned int c, i, n, N, T;
    uint8_t ch;

    unsigned int sect1[16] = { 0x0,0xD,0xB,0x9,0x7,0x5,0x3,0x1
                            , 0xE,0xC,0xA,0x8,0x6,0x4,0x2,0xF
                            };

    unsigned int sect2[16] = { 0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7
                             , 0x8,0x9,0xA,0xB,0xC,0xD,0xE,0xF
                             };
    unsigned int *sect;

    /* reserve 4k (16) 256-byte sectors */
    uint8_t buf[0x10][0x100];

    /* Check if the z-machine interpreter is one 
      of the rare non-interleaved schemes (Version E)
     */
    fseek(zip, 0x33L, SEEK_SET);
    ch = (uint8_t) fgetc(zip);

    if (ch == 0x49) /* Version E */
    {
        printf("[%X] Using non-interleave scheme\n", ch);
        sect = sect2;
    }
    else
    {
        printf("[%X] Using interleave scheme\n", ch);
        sect = sect1;
    }

    /* Reset to head of file */
    fseek(zip, 0x0L, SEEK_SET);

    /* Total bytes read/written */
    T = 0;
    while (!feof(src))
    {
        N = 0;
        n = 0x100;
        for (i = 0; i < 16 && n == 0x100; i++)
        {
            n = fread(buf[sect[i]], 1, 0x100, src);
            N += n;
        }
        T += fwrite(buf, 1, N, tgt);
    }

    printf("Data re-interleave/copy complete.\n");

    /* pad remainder of floppy with 0's */
    printf("Padding\n");

    n = 143360 - STUBSIZE - T;
    for (i = 0; i < n; i++)
        fputc('\0', tgt);


    return rc;
}
/*****************************************************************************/
int main (int argc, char* argv[])
{
    FILE *zip, *src, *tgt;

    if (! check_args(argc, argv)) exit(1);
    if (!validate_zip(argv[0], argv[1])) exit(1);

    zip = fopen(argv[1], "rb");
    src = fopen(argv[2], "rb");
    tgt = fopen(argv[3], "wb");

    if (!assert_fopen(argv, zip, 1)) exit(1);
    if (!assert_fopen(argv, src, 2)) exit(1);
    if (!assert_fopen(argv, tgt, 3)) exit(1);

    display_scoreboard(argv);

    if (copy_zip(zip, tgt) != STUBSIZE) exit(1);

    printf("Stub copied\n");

    if (!interleave(zip, src, tgt)) exit(1);
    
    printf("Done!\n\n");

    fclose(tgt);
    fclose(src);
    fclose(zip);
}
