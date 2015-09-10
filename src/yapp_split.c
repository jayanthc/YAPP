/*
 * @file yapp_split.c
 * Program to split a dedispersed time series into multiple time sections.
 *
 * @verbatim
 * Usage: yapp_split [options] <data-file>
 *     -h  --help                           Display this usage information
 *     -t  --time <time>                    Requested time duration of each
 *                                          time section. The true duration
 *                                          will be rounded up to a power-of-2
 *                                          number of samples.
 *                                          (default is 30 min.)
 *     -v  --version                        Display the version @endverbatim
 *
 * @author Jayanth Chennamangalam
 * @date 2014.06.11
 */

#include "yapp.h"
#include "yapp_sigproc.h"   /* for SIGPROC filterbank file format support */

unsigned int GetNextPowerOf2(unsigned int iVal);

/**
 * The build version string, maintained in the file version.c, which is
 * generated by makever.c.
 */
extern const char *g_pcVersion;

/* PGPLOT device ID */
extern int g_iPGDev;

/* data file */
extern FILE *g_pFData;

/* the following are global only to enable cleaning up in case of abnormal
   termination, such as those triggered by SIGINT or SIGTERM */
float *g_pfBuf = NULL;

int main(int argc, char *argv[])
{
    FILE *pFOut = NULL;
    char *pcFileData = NULL;
    char *pcFileOut = NULL;
    char acFileOut[LEN_GENSTRING] = {0};
    int iFormat = DEF_FORMAT;
    YUM_t stYUM = {{0}};
    YUM_t stYUMOut = {{0}};
    int iTotSampsPerBlock = 0;  /* iBlockSize */
    double dTSampInSec = 0.0;   /* holds sampling time in s */
    int iTimeSampsToProc = 0;
    int iBlockSize = 0;
    int iNumReads = 0;
    int iReadBlockCount = 0;
    float fTime = 0.0; /* in min. */
    char cIsLastBlock = YAPP_FALSE;
    int iRet = YAPP_RET_SUCCESS;
    int iReadItems = 0;
    int iNumSamps = 0;
    int iSampsPerSect = 0;
    int iNumTimeSects = 0;
    int iTimeSectNum = 0;
    int iBlocksPerTimeSect = 0;
    const char *pcProgName = NULL;
    int iNextOpt = 0;
    /* valid short options */
    const char* const pcOptsShort = "ht:v";
    /* valid long options */
    const struct option stOptsLong[] = {
        { "help",                   0, NULL, 'h' },
        { "time",                   1, NULL, 't' },
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

            case 't':   /* -t or --time */
                /* set option */
                fTime = atof(optarg);
                break;

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
    if (!((YAPP_FORMAT_DTS_TIM == iFormat)
          || (YAPP_FORMAT_DTS_DAT == iFormat)))
    {
        (void) fprintf(stderr,
                       "ERROR: Invalid file type!\n");
        return YAPP_RET_ERROR;
    }

    /* read metadata */
    iRet = YAPP_ReadMetadata(pcFileData, iFormat, &stYUM);
    if (iRet != YAPP_RET_SUCCESS)
    {
        (void) fprintf(stderr,
                       "ERROR: Reading metadata failed for file %s!\n",
                       pcFileData);
        return YAPP_RET_ERROR;
    }

    /* convert sampling interval to seconds */
    dTSampInSec = stYUM.dTSamp / 1e3;

    /* calculate the number of samples that is a power of 2, in one time
       section, closest to the requested time duration */
    /* NOTE: fTime is in minutes and stYUM.dTSamp is in ms */
    /* TODO: make iSampsPerSect unsigned - will also need to make
             stYUMOut.iTimeSamps unsigned */
    iSampsPerSect = GetNextPowerOf2((unsigned int) roundf((fTime * 60e3) / stYUM.dTSamp));
    (void) printf("Updating requested time duration %g min. to %g min.\n",
                   fTime,
                   (iSampsPerSect * stYUM.dTSamp) / 60e3);

    /* copy metadata for output */
    (void) memcpy(&stYUMOut, &stYUM, sizeof(YUM_t));
    /* update the number of output time samples */
    /* NOTE: ignoring bad time section, alt-az start, etc. */
    /* TODO: make stYUMOut.iTimeSamps and iSampsPerSect unsigned ints */ 
    stYUMOut.iTimeSamps = iSampsPerSect;

    iBlockSize = MAX_SIZE_BLOCK;

    iTimeSampsToProc = stYUM.iTimeSamps;
    iNumTimeSects = (int) ceilf((float) iTimeSampsToProc / iSampsPerSect);

    iNumReads = (int) ceilf((float) iTimeSampsToProc / iBlockSize);

    iBlocksPerTimeSect = (int) ceilf((float) iSampsPerSect / iBlockSize);

    /* optimisation - store some commonly used values in variables */
    iTotSampsPerBlock = iBlockSize;

    (void) printf("Splitting file %s to %d files of duration %g minutes "
                  "(%d time samples) each, "
                  "in %d reads with block size %d time samples...\n",
                  pcFileData,
                  iNumTimeSects,
                  (iSampsPerSect * stYUM.dTSamp) / 60e3,
                  iSampsPerSect,
                  iNumReads,
                  iBlockSize);

    /* open the time series data file for reading */
    g_pFData = fopen(pcFileData, "r");
    if (NULL == g_pFData)
    {
        (void) fprintf(stderr,
                       "ERROR: Opening file %s failed! %s.\n",
                       pcFileData,
                       strerror(errno));
        YAPP_CleanUp();
        return YAPP_RET_ERROR;
    }

    /* allocate memory for the buffer, based on the number of channels and time
       samples */
    g_pfBuf = (float *) YAPP_Malloc((size_t) iBlockSize,
                                    sizeof(float),
                                    YAPP_FALSE);
    if (NULL == g_pfBuf)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation failed! %s!\n",
                       strerror(errno));
        YAPP_CleanUp();
        return YAPP_RET_ERROR;
    }

    if (1 == iNumReads)
    {
        cIsLastBlock = YAPP_TRUE;
    }

    /* open the time series data file for writing */
    pcFileOut = YAPP_GetFilenameFromPath(pcFileData);
    if (YAPP_FORMAT_DTS_TIM == iFormat)
    {
        (void) sprintf(acFileOut,
                       "%s.%s%d%s",
                       pcFileOut,
                       INFIX_SPLIT,
                       iTimeSectNum,
                       EXT_TIM);
    }
    else
    {
        (void) sprintf(acFileOut,
                       "%s.%s%d%s",
                       pcFileOut,
                       INFIX_SPLIT,
                       iTimeSectNum,
                       EXT_DAT);
    }

    /* write metadata */
    iRet = YAPP_WriteMetadata(acFileOut, iFormat, stYUMOut);
    if (iRet != YAPP_RET_SUCCESS)
    {
        (void) fprintf(stderr,
                       "ERROR: Writing metadata failed for file %s!\n",
                       acFileOut);
        YAPP_CleanUp();
        return YAPP_RET_ERROR;
    }

    if (YAPP_FORMAT_DTS_TIM == iFormat)
    {
        pFOut = fopen(acFileOut, "a");
    }
    else
    {
        pFOut = fopen(acFileOut, "w");
    }
    if (NULL == pFOut)
    {
        (void) fprintf(stderr,
                       "ERROR: Opening file %s failed! %s.\n",
                       acFileOut,
                       strerror(errno));
        YAPP_CleanUp();
        return YAPP_RET_ERROR;
    }

    /* skip the header */
    if (YAPP_FORMAT_DTS_TIM == iFormat)
    {
        (void) fseek(g_pFData, (long) stYUM.iHeaderLen, SEEK_SET);
    }

    while (iNumReads > 0)
    {
        /* read data */
        (void) printf("\rReading data block %d.", iReadBlockCount);
        (void) fflush(stdout);
        iReadItems = YAPP_ReadData(g_pFData,
                                   g_pfBuf,
                                   stYUM.fSampSize,
                                   iTotSampsPerBlock);
        if (YAPP_RET_ERROR == iReadItems)
        {
            (void) fprintf(stderr, "ERROR: Reading data failed!\n");
            (void) fclose(pFOut);
            YAPP_CleanUp();
            return YAPP_RET_ERROR;
        }
        --iNumReads;
        ++iReadBlockCount;

        /* calculate the number of time samples in the block - this may not
           be iBlockSize for the last block, and should be iBlockSize for
           all other blocks */
        iNumSamps = iReadItems;

        /* write data to file */
        (void) fwrite(g_pfBuf,
                      sizeof(float),
                      (long) iNumSamps,
                      pFOut);

        if (0 == iNumReads)
        {
            /* close file */
            (void) fclose(pFOut);
            break;
        }

        if (0 == (iReadBlockCount % iBlocksPerTimeSect))
        {
            /* close file */
            (void) fclose(pFOut);
            pFOut = NULL;

            if (cIsLastBlock)
            {
                break;
            }

            ++iTimeSectNum;
            if (iTimeSectNum == iNumTimeSects - 1)
            {
                /* last time section, so may need to rewind */
                (void) fseek(g_pFData,
                             - (long) (iSampsPerSect * stYUM.fSampSize),
                             SEEK_END);
                iNumReads = iBlocksPerTimeSect;
                iReadBlockCount = 0;
            }

            /* open new time series data file for writing */
            pcFileOut = YAPP_GetFilenameFromPath(pcFileData);
            if (YAPP_FORMAT_DTS_TIM == iFormat)
            {
                (void) sprintf(acFileOut,
                               "%s.%s%d%s",
                               pcFileOut,
                               INFIX_SPLIT,
                               iTimeSectNum,
                               EXT_TIM);
            }
            else
            {
                (void) sprintf(acFileOut,
                               "%s.%s%d%s",
                               pcFileOut,
                               INFIX_SPLIT,
                               iTimeSectNum,
                               EXT_DAT);
            }

            /* update start time */
            stYUMOut.dTStart = stYUM.dTStart
                               + ((double) iTimeSectNum
                                  * iSampsPerSect
                                  * (dTSampInSec / 86400));

            /* write new metadata */
            iRet = YAPP_WriteMetadata(acFileOut, iFormat, stYUMOut);
            if (iRet != YAPP_RET_SUCCESS)
            {
                (void) fprintf(stderr,
                               "ERROR: Writing metadata failed for file %s!\n",
                               acFileOut);
                YAPP_CleanUp();
                return YAPP_RET_ERROR;
            }

            if (YAPP_FORMAT_DTS_TIM == iFormat)
            {
                pFOut = fopen(acFileOut, "a");
            }
            else
            {
                pFOut = fopen(acFileOut, "w");
            }
            if (NULL == pFOut)
            {
                (void) fprintf(stderr,
                               "ERROR: Opening file %s failed! %s.\n",
                               acFileOut,
                               strerror(errno));
                YAPP_CleanUp();
                return YAPP_RET_ERROR;
            }
        }

        if (1 == iNumReads)
        {
            cIsLastBlock = YAPP_TRUE;
        }
    }

    (void) printf("DONE!\n");

    YAPP_CleanUp();

    return YAPP_RET_SUCCESS;
}

/*
 * http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
 */
unsigned int GetNextPowerOf2(unsigned int iVal)
{
    --iVal;
    iVal |= iVal >> 1;
    iVal |= iVal >> 2;
    iVal |= iVal >> 4;
    iVal |= iVal >> 8;
    iVal |= iVal >> 16;
    ++iVal;

    return iVal;
}

/*
 * Prints usage information
 */
void PrintUsage(const char *pcProgName)
{
    (void) printf("Usage: %s [options] <data-file>\n",
                  pcProgName);
    (void) printf("    -h  --help                          ");
    (void) printf("Display this usage information\n");
    (void) printf("    -t  --time <time>                   ");
    (void) printf("Requested time duration of each time\n");
    (void) printf("                                        ");
    (void) printf("section. The true duration will be\n");
    (void) printf("                                        ");
    (void) printf("rounded up to a power-of-2 number of\n");
    (void) printf("                                        ");
    (void) printf("samples.\n");
    (void) printf("                                        ");
    (void) printf("(default is 30 min.)\n");
    (void) printf("    -v  --version                       ");
    (void) printf("Display the version\n");

    return;
}

