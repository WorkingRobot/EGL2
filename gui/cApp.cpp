#include "cApp.h"

#include "../winfspcheck.h"

#include <ShlObj_core.h>

#define MESSAGE_ERROR(format, ...) wxMessageBox(wxString::Format(format, __VA_ARGS__), "Error", wxICON_ERROR | wxOK | wxCENTRE)

cApp::cApp() {

}

cApp::~cApp() {

}

bool cApp::OnInit() {
	auto result = LoadWinFsp();
	if (result != WinFspCheckResult::LOADED) {
		switch (result)
		{
		case WinFspCheckResult::CANNOT_ENUMERATE:
			MESSAGE_ERROR("Could not iterate over drivers to get WinFsp install. System-specific error: %d", GetLastError());
			break;
		case WinFspCheckResult::NOT_FOUND:
			MESSAGE_ERROR("Could not find WinFsp as an installed driver. Maybe you don't have it installed?");
			break;
		case WinFspCheckResult::NO_DLL:
			MESSAGE_ERROR("Could not find WinFsp's DLL in the driver's folder. Try reinstalling WinFsp.");
			break;
		case WinFspCheckResult::CANNOT_LOAD:
			MESSAGE_ERROR("Could not load WinFsp's DLL in the driver's folder. Try reinstalling WinFsp.");
			break;
		default:
			MESSAGE_ERROR("An unknown error occurred when trying to load WinFsp's DLL: %d", result);
			break;
		}
		return false;
	}

	fs::path DataFolder;
	{
		PWSTR appDataFolder;
		if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataFolder) != S_OK) {
			MESSAGE_ERROR("Could not get the location of your AppData folder.", result);
			return false;
		}
		DataFolder = appDataFolder;
		CoTaskMemFree(appDataFolder);
	}
	DataFolder /= "EGL2";
	if (!fs::create_directories(DataFolder) && !fs::is_directory(DataFolder)) {
		MESSAGE_ERROR("Could not create EGL2 folder.", result);
		return false;
	}

	m_frame1 = new cMain(DataFolder / "config", DataFolder);
	m_frame1->Show();
	return true;
}