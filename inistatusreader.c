#include "inistatusreader.h"
#include <stdbool.h>

bool getParameter(LPWSTR *aCommandLine, int iCmdLineArgs, LPWSTR lpszParName, LPWSTR *lpszTarget)
{
    int tmpBufSize = 0;

    /* skipping file name, hence i=1 */
    for (int i = 1; i < iCmdLineArgs; i += 2)
    {
        if (!wcscmp(aCommandLine[i], lpszParName))
        {
            /* calc plus one for at least the \0 */
            tmpBufSize = wcslen(aCommandLine[i + 1]) + 1;

            *lpszTarget = (LPWSTR)calloc(tmpBufSize, sizeof(WCHAR));
            wcsncpy(*lpszTarget, aCommandLine[i + 1], tmpBufSize);

            break;
        }
    }

    return tmpBufSize != 0;
}

void GetIniParameters(LPWSTR *aCommandLine, int iCmdLineArgs, struct INIPARAM *iniParam)
{
    printf("Parsing command line..\n");

    if (iCmdLineArgs < 7)
    {
        fprintf(stderr, "Call IniWatcher with -file FILE -key KEY -value VALUE!\n");
        exit(EINVAL);
    }

    if (!getParameter(aCommandLine, iCmdLineArgs, L"-file", &(iniParam->lpszArgFile)))
    {
        fprintf(stderr, "Did not find -file parameter/value!\n");
        exit(EINVAL);
    }

    if (!getParameter(aCommandLine, iCmdLineArgs, L"-key", &(iniParam->lpszArgKey)))
    {
        fprintf(stderr, "Did not find -key parameter/value!\n");
        exit(EINVAL);
    }

    if (!getParameter(aCommandLine, iCmdLineArgs, L"-value", &(iniParam->lpszArgExpectedValue)))
    {
        fprintf(stderr, "Did not find -value parameter/value!\n");
        exit(EINVAL);
    }

    fwprintf(stderr, L"Watching %s for %s to be %s\n", iniParam->lpszArgFile, iniParam->lpszArgKey, iniParam->lpszArgExpectedValue);
}

void GetConfigPath(LPWSTR lpszTempPath, int maxLen)
{
    GetTempPathW(maxLen, lpszTempPath);
    PathCombineW(lpszTempPath, lpszTempPath, L"iniwatcher.dat");
}

enum STATUS GetStatus(struct INIPARAM iniParam)
{
    static int iLastGotStatusTime = 0;

    /* don't fetch more than one time per second */
    if (iLastGotStatusTime == time(NULL))
        return NOTFETCHED;

    iLastGotStatusTime = time(NULL);

    WCHAR sIniLineBuffer[MAX_LINE_SIZE];
    FILE *fp;

    if ((fp = _wfopen(iniParam.lpszArgFile, L"r")) == NULL)
    {
        perror("Failed to open file");
        return UNKNOWN;
    }

    while (fgetws(sIniLineBuffer, MAX_LINE_SIZE, fp) != NULL)
    {
        /* if line does not start with the key, jump to next line */
        if (wcsncmp(sIniLineBuffer, iniParam.lpszArgKey, wcslen(iniParam.lpszArgKey)) != 0)
            continue;

        /* if key matches but no equal sign follows, jump to next line */
        if (wcsncmp(sIniLineBuffer + wcslen(iniParam.lpszArgKey), L"=", 1) != 0)
            continue;

        /* if key is good and the value is equal .. */
        if (!wcsncmp(sIniLineBuffer + wcslen(iniParam.lpszArgKey) + 1, 
            iniParam.lpszArgExpectedValue, 
            wcslen(iniParam.lpszArgExpectedValue)))
        {
            return ENABLED;
        }
        else
        {
            return DISABLED;
        }
    }

    fclose(fp);
    LocalFree(sIniLineBuffer);

    return UNKNOWN;
}