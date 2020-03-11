#include "ProjFS.h"

static ProjFS* filesystem;

static NTSTATUS SvcStart(FSP_SERVICE* Service, ULONG argc, PWSTR* argv)
{
	FspDebugLogSetHandle(GetStdHandle(STD_ERROR_HANDLE));

	filesystem = new ProjFS;
	filesystem->Initialize(L"C:\\aaaaw", 1583570221, 4096);

	return STATUS_SUCCESS;
}

static NTSTATUS SvcStop(FSP_SERVICE* Service)
{
	delete filesystem;

	return STATUS_SUCCESS;
}

int wmain(int argc, wchar_t** argv)
{
	if (!NT_SUCCESS(FspLoad(0)))
		return ERROR_DELAY_LOAD_FAILED;

	return FspServiceRun(L"" PROGNAME, SvcStart, SvcStop, 0);
}