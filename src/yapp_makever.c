/*
 * yapp_makever.c
 * Program that creates yapp_version.c
 *
 * Created by Jayanth Chennamangalam
 */

#define _GNU_SOURCE     /* for getline() */
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>
#include <errno.h>

#define RET_SUCCESS         0
#define RET_ERROR           -1

#define MAX_LEN_GENSTRING   256

/* TODO: read the path as argument */
#define FILE_VERSRC         "src/yapp_version.c"
#define VAR_VER             "*g_pcVersion"
#define VER_BUILD_PREFIX    "YAPP-DEV-3.6.4-beta"
#define VER_BUILD_DELIM     "-"

time_t GetLatestTimestamp(void);

/*
 * main()
 *
 * The main function
 */
int main(int argc, char *argv[])
{
    FILE *pFileVerSrc = NULL;
    time_t Time;
    struct tm *pstTime = NULL;
    char acVersion[MAX_LEN_GENSTRING] = {0};
    char acBuild[MAX_LEN_GENSTRING] = {0};

    /* get the latest timestamp among all *.c and *.h files */
    Time = GetLatestTimestamp();
    pstTime = localtime(&Time);
    (void) strftime(acBuild, sizeof(acBuild), "%Y%m%d-%H%M%S", pstTime);

    (void) strncpy(acVersion,
                   VER_BUILD_PREFIX,
                   (sizeof(acVersion) - (strlen(acVersion) + 1)));
    (void) strncat(acVersion,
                   VER_BUILD_DELIM,
                   (sizeof(acVersion) - (strlen(acVersion) + 1)));
    (void) strncat(acVersion,
                   acBuild,
                   (sizeof(acVersion) - (strlen(acVersion) + 1)));

    /* open the version source file for writing */
    pFileVerSrc = fopen(FILE_VERSRC, "w");
    if (NULL == pFileVerSrc)
    {
        (void) fprintf(stderr,
                       "Opening file %s failed!\n",
                       FILE_VERSRC);
        return RET_ERROR;
    }

    /* write header comments */
    (void) fprintf(pFileVerSrc,
                   "/*\n"
                   " * %s\n"
                   " * The version file\n"
                   " * \n"
                   " * Generated by the yapp_makever program - DO NOT MODIFY\n"
                   " */\n",
                   FILE_VERSRC);
    (void) fprintf(pFileVerSrc, "\n");

    /* write the global variable definition */
    (void) fprintf(pFileVerSrc,
                   "const char %s = \"%s\";\n",
                   VAR_VER,
                   acVersion);
    (void) fprintf(pFileVerSrc, "\n");

    (void) fclose(pFileVerSrc);

    return RET_SUCCESS;
}

time_t GetLatestTimestamp()
{
    struct stat stFileStats = {0};
    time_t stModTime = {0};
    glob_t stGlobBuf = {0};
    int i = 0;
    int iRet = RET_SUCCESS;

    /* create filters for pattern matching */
    glob("src/*.c", 0, NULL, &stGlobBuf);
    glob("src/*.h", GLOB_APPEND, NULL, &stGlobBuf);

    /* find the latest timestamp */
    for (i = 0; i < stGlobBuf.gl_pathc; ++i)
    {
        /* skip this source file */
        if ((0 == strcmp(stGlobBuf.gl_pathv[i], "src/yapp_makever.c"))
            || (0 == strcmp(stGlobBuf.gl_pathv[i], "src/yapp_version.c")))
        {
            continue;
        }

        iRet = stat(stGlobBuf.gl_pathv[i], &stFileStats);
        if (iRet != RET_SUCCESS)
        {
            (void) fprintf(stderr,
                           "ERROR: Failed to stat %s: %s!\n",
                           stGlobBuf.gl_pathv[i],
                           strerror(errno));
            return RET_ERROR;
        }
        /* compare with the previous file's modification time */
        if (stFileStats.st_mtime > stModTime)
        {
            stModTime = stFileStats.st_mtime;
        }
    }

    /* free the glob buffer */
    globfree(&stGlobBuf);

    return stModTime;
}

