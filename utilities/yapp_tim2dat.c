/*
 * @file yapp_tim2dat.c
 * Program to convert dedispersed time series data from SIGPROC .tim format to
 *  PRESTO .dat to SIGPROC .tim format.
 *
 * @verbatim
 * Usage: yapp_tim2dat [options] <data-file>
 *     -h  --help                           Display this usage information
 *     -v  --version                        Display the version @endverbatim
 *
 * @author Jayanth Chennamangalam
 * @date 2013.04.12
 */

#include "yapp.h"
#include "yapp_sigproc.h"
#include "yapp_tim2dat.h"

/**
 * The build version string, maintained in the file version.c, which is
 * generated by makever.c.
 */
extern const char *g_pcVersion;

int main(int argc, char *argv[])
{
    char *pcFileData = NULL;
    char *pcFilename = NULL;
    FILE *pFDat = NULL;
    char acFileDat[LEN_GENSTRING] = {0};
    int iFormat = DEF_FORMAT;
    YUM_t stYUM = {{0}};
    int iRet = EXIT_SUCCESS;

    const char *pcProgName = NULL;
    int iNextOpt = 0;
    /* valid short options */
    const char* const pcOptsShort = "hv";
    /* valid long options */
    const struct option stOptsLong[] = {
        { "help",                   0, NULL, 'h' },
        { "version",                0, NULL, 'v' },
        { NULL,                     0, NULL, 0   }
    };

    /* get the filename of the program from the argument list */
    pcProgName = argv[0];

    /* parse the input */
    do
    {
        iNextOpt = getopt_long(argc, argv, pcOptsShort, stOptsLong, NULL);
        switch (iNextOpt)
        {
            case 'h':   /* -h or --help */
                /* print usage info and terminate */
                PrintUsage(pcProgName);
                return YAPP_RET_SUCCESS;

            case 'v':   /* -v or --version */
                /* display the version */
                (void) printf("%s\n", g_pcVersion);
                return YAPP_RET_SUCCESS;

            case '?':   /* user specified an invalid option */
                /* print usage info and terminate with error */
                (void) fprintf(stderr, "ERROR: Invalid option!\n");
                PrintUsage(pcProgName);
                return YAPP_RET_ERROR;

            case -1:    /* done with options */
                break;

            default:    /* unexpected */
                assert(0);
        }
    } while (iNextOpt != -1);

    /* no arguments */
    if (argc <= optind)
    {
        (void) fprintf(stderr, "ERROR: Input file not specified!\n");
        PrintUsage(pcProgName);
        return YAPP_RET_ERROR;
    }

    /* register the signal-handling function */
    iRet = YAPP_RegisterSignalHandlers();
    if (iRet != YAPP_RET_SUCCESS)
    {
        (void) fprintf(stderr,
                       "ERROR: Handler registration failed!\n");
        return YAPP_RET_ERROR;
    }

    /* get the input filename */
    pcFileData = argv[optind];

    /* determine the file type */
    iFormat = YAPP_GetFileType(pcFileData);
    if (YAPP_RET_ERROR == iFormat)
    {
        (void) fprintf(stderr,
                       "ERROR: File type determination failed!\n");
        return YAPP_RET_ERROR;
    }
    if (iFormat != YAPP_FORMAT_DTS_TIM)
    {
        (void) fprintf(stderr,
                       "ERROR: Invalid file type!\n");
        return YAPP_RET_ERROR;
    }

    iRet = YAPP_ReadMetadata(pcFileData, iFormat, &stYUM);
    if (iRet != EXIT_SUCCESS)
    {
        fprintf(stderr,
                "ERROR: Reading metadata failed!\n");
        return EXIT_FAILURE;
    }

    pcFilename = YAPP_GetFilenameFromPath(pcFileData);
    (void) strcpy(acFileDat, pcFilename);
    (void) strcat(acFileDat, EXT_DAT);
    iRet = YAPP_WriteMetadata(acFileDat, YAPP_FORMAT_DTS_DAT, stYUM);
    if (iRet != EXIT_SUCCESS)
    {
        fprintf(stderr,
                "ERROR: Writing metadata failed!\n");
        (void) fclose(pFDat);
        return EXIT_FAILURE;
    }

    /* open .dat file and write data */
    pFDat = fopen(acFileDat, "w");
    if (NULL == pFDat)
    {
        fprintf(stderr,
                "ERROR: Opening file %s failed! %s.\n",
                acFileDat,
                strerror(errno));
        return EXIT_FAILURE;
    }
    iRet = YAPP_CopyData(pcFileData, pFDat, stYUM.iHeaderLen);
    if (iRet != EXIT_SUCCESS)
    {
        fprintf(stderr,
                "ERROR: Writing data failed!\n");
        (void) fclose(pFDat);
        return EXIT_FAILURE;
    }

    (void) fclose(pFDat);

    return EXIT_SUCCESS;
}


int YAPP_CopyData(char *pcFileData, FILE *pFDat, int iOffset)
{
    FILE *pFData = NULL;
    struct stat stFileStats = {0};
    off_t lByteCount = 0;
    char *pcBuf = NULL;
    int iRet = EXIT_SUCCESS;

    /* get the file size */
    iRet = stat(pcFileData, &stFileStats);
    if (iRet != EXIT_SUCCESS)
    {
        (void) fprintf(stderr,
                       "ERROR: Failed to stat %s: %s!\n",
                       pcFileData,
                       strerror(errno));
        (void) fclose(pFData);
        return EXIT_FAILURE;
    }

    /* open the file */
    pFData = fopen(pcFileData, "r");
    if (NULL == pFData)
    {
        fprintf(stderr,
                "ERROR: Opening file %s failed! %s.\n",
                pcFileData,
                strerror(errno));
        return EXIT_FAILURE;
    }

    /* skip header */
    fseek(pFData, iOffset, SEEK_SET);

    /* allocate memory for the buffer */
    pcBuf = (char *) malloc(SIZE_BUF);
    if (NULL == pcBuf)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation for buffer failed! %s!\n",
                       strerror(errno));
        (void) fclose(pFData);
        return EXIT_FAILURE;
    }

    /* copy data */
    do
    {
        iRet = fread(pcBuf, 1, SIZE_BUF, pFData);
        lByteCount += iRet;
        (void) fwrite(pcBuf, 1, iRet, pFDat);
    }
    while (SIZE_BUF == iRet);

    free(pcBuf);

    /* check if all data has been copied */
    if (lByteCount != (stFileStats.st_size - iOffset))
    {
        (void) fprintf(stderr, "ERROR: Data copy failed!\n");
        printf("%ld, %ld\n", lByteCount, stFileStats.st_size);
        (void) fclose(pFData);
        return EXIT_FAILURE;
    }

    (void) fclose(pFData);

    return EXIT_SUCCESS;
}

/*
 * Prints usage information
 */
void PrintUsage(const char *pcProgName)
{
    (void) printf("Usage: %s [options] <data-file>\n",
                  pcProgName);
    (void) printf("    -h  --help                           ");
    (void) printf("Display this usage information\n");
    (void) printf("    -v  --version                        ");
    (void) printf("Display the version\n");

    return;
}

