#include "symlink_workaround.h"

#define DEVELOPER_MODE_REGKEY   L"Software\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock"
#define DEVELOPER_MODE_REGVALUE L"AllowDevelopmentWithoutDevLicense"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winerror.h>

// Taken from https://docs.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-checktokenmembership
bool IsUserAdmin()
{
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    BOOL b = AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup);
    if (b)
    {
        if (!CheckTokenMembership(NULL, AdministratorsGroup, &b))
        {
            b = false;
        }
        FreeSid(AdministratorsGroup);
    }

    return b;
}

bool IsDeveloperModeEnabled() {
    HKEY key;
    auto Result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, DEVELOPER_MODE_REGKEY, NULL, KEY_READ, &key);

    if (Result != ERROR_SUCCESS) { // should be ERROR_NO_MATCH or ERROR_FILE_NOT_FOUND
        return false; // In reality, AppModelUnlock should always exist, so this shouldn't run
    }
    else {
        DWORD data;
        DWORD type = REG_DWORD;
        DWORD size = sizeof(data);
        Result = RegQueryValueEx(key, DEVELOPER_MODE_REGVALUE, NULL, &type, (LPBYTE)&data, &size);
        RegCloseKey(key);
        return Result == ERROR_SUCCESS ? data : false;
    }
}

bool EnableDeveloperMode() {
    DWORD data = 1;
    return RegSetKeyValue(HKEY_LOCAL_MACHINE, DEVELOPER_MODE_REGKEY, DEVELOPER_MODE_REGVALUE, REG_DWORD, &data, sizeof(data)) == ERROR_SUCCESS;
}