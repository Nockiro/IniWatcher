#include <shlwapi.h>
#include <strsafe.h>
#include <shellapi.h>
#include <errno.h>
#include <time.h>

#define MAX_LINE_SIZE 255

enum STATUS
{
    NOTFETCHED,
    UNKNOWN,
    DISABLED,
    ENABLED
};

struct INIPARAM
{
    LPWSTR lpszArgFile;
    LPWSTR lpszArgKey;
    LPWSTR lpszArgExpectedValue;
};

void GetIniParameters(LPWSTR * aCommandLine, int iCmdLineArgs, struct INIPARAM * iniParam);
enum STATUS GetStatus(struct INIPARAM iniParam);
void GetConfigPath(LPWSTR lpszTempPath, int maxLen);