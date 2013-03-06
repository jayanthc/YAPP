/*
 * @file yapp_fits2fil.c
 * Program to convert PSRFITS to filterbank format.
 *
 * @verbatim
 * Usage: yapp_fits2fil [options] <data-files>
 *     -h  --help                           Display this usage information
 *     -v  --version                        Display the version @endverbatim
 *
 * @author Jayanth Chennamangalam
 * @date 2012.02.25
 */

#include "yapp.h"
#include "yapp_sigproc.h"   /* for SIGPROC filterbank file format support */
#include "yapp_psrfits.h"
#include <fitsio.h>

/**
 * The build version string, maintained in the file version.c, which is
 * generated by makever.c.
 */
extern const char *g_pcVersion;

/* data file */
extern FILE *g_pFData;

/* the following are global only to enable cleaning up in case of abnormal
   termination, such as those triggered by SIGINT or SIGTERM */
void *g_pvBuf = NULL;

int main(int argc, char *argv[])
{
    char *pcFileSpec = NULL;
    char *pcFileOut = NULL;
    char acFileOut[LEN_GENSTRING] = {0};
    fitsfile *pstFileData = NULL;
    int iFormat = DEF_FORMAT;
    YUM_t stYUM = {{0}};
    int iRet = YAPP_RET_SUCCESS;
    int iNumSubInt = 0;
    int iColNum = 0;
    char acErrMsg[LEN_GENSTRING] = {0};
    int iSampsPerSubInt = 0;
    long int lBytesPerSubInt = 0;
    int iStatus = 0;
    int i = 0;
    int iDataType = 0;
    char cIsFirst = YAPP_TRUE;
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

    /* handle expanded wildcards */
    iNextOpt = optind;
    while ((argc - iNextOpt) != 0)
    {
        /* get the input filename */
        pcFileSpec = argv[iNextOpt];

        if (argc != 2)  /* more than one input file */
        {
            (void) printf("\rProcessing file %s.", pcFileSpec);
            (void) fflush(stdout);
        }

        /* determine the file type */
        iFormat = YAPP_GetFileType(pcFileSpec);
        if (YAPP_RET_ERROR == iFormat)
        {
            (void) fprintf(stderr,
                           "ERROR: File type determination failed!\n");
            return YAPP_RET_ERROR;
        }
        if (iFormat != YAPP_FORMAT_PSRFITS)
        {
            (void) fprintf(stderr,
                           "ERROR: Invalid file type!\n");
            return YAPP_RET_ERROR;
        }

        if (cIsFirst)
        {
            /* read metadata */
            iRet = YAPP_ReadMetadata(pcFileSpec, iFormat, &stYUM);
            if (iRet != YAPP_RET_SUCCESS)
            {
                (void) fprintf(stderr,
                               "ERROR: Reading metadata failed for file %s!\n",
                               pcFileSpec);
                return YAPP_RET_ERROR;
            }

            /* build output file name */
            pcFileOut = YAPP_GetFilenameFromPath(pcFileSpec, EXT_PSRFITS);
            (void) strcpy(acFileOut, pcFileOut);
            (void) strcat(acFileOut, EXT_FIL);

            /* write metadata */
            iFormat = YAPP_FORMAT_FIL;
            iRet = YAPP_WriteMetadata(acFileOut, iFormat, stYUM);
            if (iRet != YAPP_RET_SUCCESS)
            {
                (void) fprintf(stderr,
                               "ERROR: Writing metadata failed for file %s!\n",
                               acFileOut);
                YAPP_CleanUp();
                return YAPP_RET_ERROR;
            }
        }

        /*  open PSRFITS file */
        (void) fits_open_file(&pstFileData, pcFileSpec, READONLY, &iStatus);
        if  (iStatus != 0)
        {
            fits_get_errstatus(iStatus, acErrMsg); 
            (void) fprintf(stderr, "ERROR: Opening file failed! %s\n", acErrMsg);
            YAPP_CleanUp();
            return YAPP_RET_ERROR;
        }
        /* read SUBINT HDU header to get data parameters */
        (void) fits_movnam_hdu(pstFileData,
                               BINARY_TBL,
                               YAPP_PF_HDUNAME_SUBINT,
                               0,
                               &iStatus);
        if  (iStatus != 0)
        {
            fits_get_errstatus(iStatus, acErrMsg); 
            (void) fprintf(stderr,
                           "ERROR: Moving to HDU %s failed! %s\n",
                           YAPP_PF_HDUNAME_SUBINT,
                           acErrMsg);
            (void) fits_close_file(pstFileData, &iStatus);
            YAPP_CleanUp();
            return YAPP_RET_ERROR;
        }
        /* NOTE: the number of rows may be different in the last file, so this
                 needs to be read for every file */
        (void) fits_read_key(pstFileData,
                             TINT,
                             YAPP_PF_LABEL_NSUBINT,
                             &iNumSubInt,
                             NULL,
                             &iStatus);
        if (cIsFirst)
        {
            (void) fits_read_key(pstFileData,
                                 TINT,
                                 YAPP_PF_LABEL_NSBLK,
                                 &iSampsPerSubInt,
                                 NULL,
                                 &iStatus);
            (void) fits_get_colnum(pstFileData,
                                   CASESEN,
                                   YAPP_PF_LABEL_DATA,
                                   &iColNum,
                                   &iStatus);
            if (iStatus != 0)
            {
                fits_get_errstatus(iStatus, acErrMsg); 
                (void) fprintf(stderr,
                               "ERROR: Getting column number failed! %s\n",
                               acErrMsg);
                (void) fits_close_file(pstFileData, &iStatus);
                YAPP_CleanUp();
                return YAPP_RET_ERROR;
            }

            /* allocate memory for data array */
            lBytesPerSubInt = (long int) iSampsPerSubInt * stYUM.iNumChans
                              * ((float) stYUM.iNumBits / YAPP_BYTE2BIT_FACTOR);
            g_pvBuf = YAPP_Malloc(lBytesPerSubInt, sizeof(char), YAPP_FALSE);
            if (NULL == g_pvBuf)
            {
                (void) fprintf(stderr,
                               "ERROR: Memory allocation failed! %s!\n",
                               strerror(errno));
                (void) fits_close_file(pstFileData, &iStatus);
                YAPP_CleanUp();
                return YAPP_RET_ERROR;
            }

            /* open .fil file */
            g_pFData = fopen(acFileOut, "a");
            if (NULL == g_pFData)
            {
                (void) fprintf(stderr,
                               "ERROR: Opening file %s failed! %s.\n",
                               acFileOut,
                               strerror(errno));
                (void) fits_close_file(pstFileData, &iStatus);
                YAPP_CleanUp();
                return YAPP_RET_ERROR;
            }

            /* read data */
            switch (stYUM.iNumBits)
            {
                case YAPP_SAMPSIZE_4:
                    iDataType = TBYTE;
                    /* update iSampsPerSubInt */
                    iSampsPerSubInt /= 2;
                    break;

                case YAPP_SAMPSIZE_8:
                    iDataType = TBYTE;
                    break;

                case YAPP_SAMPSIZE_16:
                    iDataType = TSHORT;
                    break;

                case YAPP_SAMPSIZE_32:
                    iDataType = TLONG;
                    break;

                default:
                    (void) fprintf(stderr,
                                   "ERROR: Unexpected number of bits!\n");
                    (void) fits_close_file(pstFileData, &iStatus);
                    YAPP_CleanUp();
                    return YAPP_RET_ERROR;
            }
        }
        for (i = 1; i <= iNumSubInt; ++i)
        {
            (void) fits_read_col(pstFileData,
                                 iDataType,
                                 iColNum,
                                 i,
                                 1,
                                 (long int) stYUM.iNumChans * iSampsPerSubInt,
                                 NULL,
                                 g_pvBuf,
                                 NULL,
                                 &iStatus);
            if (iStatus != 0)
            {
                fits_get_errstatus(iStatus, acErrMsg); 
                (void) fprintf(stderr,
                               "ERROR: Getting column number failed! %s\n",
                               acErrMsg);
                (void) fits_close_file(pstFileData, &iStatus);
                YAPP_CleanUp();
                return YAPP_RET_ERROR;
            }
            (void) fwrite(g_pvBuf, sizeof(char), lBytesPerSubInt, g_pFData);
        }

        (void) fits_close_file(pstFileData, &iStatus);
        cIsFirst = YAPP_FALSE;
        ++iNextOpt;
    }

    (void) printf("\n");

    /* clean up */
    YAPP_CleanUp();

    return YAPP_RET_SUCCESS;
}

/*
 * Prints usage information
 */
void PrintUsage(const char *pcProgName)
{
    (void) printf("Usage: %s [options] <data-files>\n",
                  pcProgName);
    (void) printf("    -h  --help                           ");
    (void) printf("Display this usage information\n");
    (void) printf("    -v  --version                        ");
    (void) printf("Display the version\n");

    return;
}

