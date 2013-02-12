/**
 * @file yapp_viewmetadata.c
 * Reads the config file/header for a given dynamic spectrum data and prints
 * relevant configuration information
 *
 * @verbatim
 * Usage: yapp_viewmetadata [options] <data-file>
 *     -h  --help                           Display this usage information
 *     -v  --version                        Display the version @endverbatim
 *
 * @author Jayanth Chennamangalam
 * @date 2009.06.25
 */

#include "yapp.h"
#include "yapp_sigproc.h"   /* for SIGPROC filterbank file format support */

/**
 * The build version string, maintained in the file version.c, which is
 * generated by makever.c.
 */
extern const char *g_pcVersion;

int main(int argc, char *argv[])
{
    char *pcFileSpec = NULL;
    int iFormat = DEF_FORMAT;
    int iRet = YAPP_RET_SUCCESS;
    YUM_t stYUM = {{0}};
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

    /* handle expanded wildcards */
    iNextOpt = optind;
    while ((argc - iNextOpt) != 0)  /* handle expanded wildcards */
    {
        /* get the input filename */
        pcFileSpec = argv[iNextOpt];

        if (argc != 2)  /* more than one input file */
        {
            (void) printf("File: %s\n", pcFileSpec);
        }

        /* determine the file type */
        iFormat = YAPP_GetFileType(pcFileSpec);
        if (YAPP_RET_ERROR == iFormat)
        {
            (void) fprintf(stderr,
                           "ERROR: File type determination failed!\n");
            return YAPP_RET_ERROR;
        }

        iRet = YAPP_ReadMetadata(pcFileSpec, iFormat, &stYUM);
        if (iRet != YAPP_RET_SUCCESS)
        {
            (void) fprintf(stderr,
                           "ERROR: Reading metadata failed for file %s! "
                           "Moving to next file.\n",
                           pcFileSpec);
            ++iNextOpt;
            continue;
        }

        (void) printf("Observing site                    : %s\n",
                      stYUM.acSite);
        (void) printf("Field name                        : %s\n",
                      stYUM.acPulsar);
        (void) printf("Start time                        : %.10g MJD\n",
                      stYUM.dTStart);
        (void) printf("Centre frequency                  : %.10g MHz\n",
                      stYUM.fFCentre);
        (void) printf("Bandwidth                         : %.10g MHz\n",
                      stYUM.fBW);
        (void) printf("Sampling interval                 : %.10g ms\n",
                      stYUM.dTSamp);
        (void) printf("Number of channels                : %d\n",
                      stYUM.iNumChans);
        (void) printf("Number of good channels           : %d\n",
                      stYUM.iNumGoodChans);
        (void) printf("Channel bandwidth                 : %.10g MHz\n",
                      stYUM.fChanBW);
        (void) printf("Lowest frequency                  : %.10g MHz\n",
                      stYUM.fFMin);
        (void) printf("Highest frequency                 : %.10g MHz\n",
                      stYUM.fFMax);
        if (YAPP_TRUE == stYUM.cIsBandFlipped)
        {
            (void) printf("                                    "
                          "Flipped band\n");
        }
        (void) printf("Estimated number of bands         : %d\n",
                      stYUM.iNumBands);
        if (stYUM.iBFTimeSects != 0)    /* no beam-flip information */
        {
            (void) printf("First beam-flip time              : %.10g s\n",
                          stYUM.dTNextBF);
            (void) printf("Beam-flip interval                : %.10g s\n",
                          stYUM.dTBFInt);
            (void) printf("Number of beam-flip time sections : %d\n",
                          stYUM.iBFTimeSects);
        }
        (void) printf("Number of bad time sections       : %d\n",
                      stYUM.iNumBadTimes);
        (void) printf("Number of bits per sample         : %d\n",
                      stYUM.iNumBits);
        if (stYUM.iNumIFs != 0)
        {
            (void) printf("Number of IFs                     : %d\n",
                          stYUM.iNumIFs);
        }
        (void) printf("Duration of data in\n");
        (void) printf("    Bytes                         : %ld\n",
                      (stYUM.lDataSizeTotal / stYUM.iNumChans));
        (void) printf("    Time samples                  : %d\n",
                      stYUM.iTimeSamps);
        (void) printf("    Time                          : %g s\n",
                      (stYUM.iTimeSamps * (stYUM.dTSamp / 1e3)));
        (void) printf("Length of header                  : %d\n",
                      stYUM.iHeaderLen);

        if ((argc - iNextOpt) != 1)
        {
            (void) printf("----------------------------------------");
            (void) printf("----------------------------------------\n");
        }
        ++iNextOpt;
    }

    /* clean up */
    YAPP_CleanUp();

    return YAPP_RET_SUCCESS;
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

