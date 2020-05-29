#include "cApp.h"

#ifndef LOG_SECTION
#define LOG_SECTION "cApp"
#endif

#include "../checks/symlink_workaround.h"
#include "../checks/winfspcheck.h"
#include "../Logger.h"

#include <ShlObj_core.h>
#include <sstream>
#include <wintoastlib.h>

using namespace WinToastLib;

#define MESSAGE_ERROR(format, ...) wxMessageBox(wxString::Format(format, __VA_ARGS__), "Error - EGL2", wxICON_ERROR | wxOK | wxCENTRE)

cApp::cApp() {
	const char* SetupError = nullptr;
	fs::path LogPath;
	{
		PWSTR appDataFolder;
		if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataFolder) != S_OK) {
			SetupError = "Could not get the location of your AppData folder.";
			goto setupExit;
		}
		DataFolder = appDataFolder;
		CoTaskMemFree(appDataFolder);
	}
	DataFolder /= "EGL2";
	if (!fs::create_directories(DataFolder) && !fs::is_directory(DataFolder)) {
		SetupError = "Could not create EGL2 data folder";
		goto setupExit;
	}

	if (!fs::create_directories(DataFolder / "logs") && !fs::is_directory(DataFolder / "logs")) {
		SetupError = "Could not create EGL2 logs folder";
		goto setupExit;
	}

	if (!fs::create_directories(DataFolder / "manifests") && !fs::is_directory(DataFolder / "manifests")) {
		SetupError = "Could not create EGL2 manifests folder";
		goto setupExit;
	}

	{
		auto logTime = std::time(nullptr);
		std::stringstream ss;
		ss << std::put_time(std::localtime(&logTime), "%F_%T.log"); // ISO 8601 without timezone information.
		auto s = ss.str();
		std::replace(s.begin(), s.end(), ':', '-');
		LogPath = DataFolder / "logs" / s;
	}

	Logger::Setup();
	Logger::Callback = [this](Logger::LogLevel level, const char* section, const char* str) {
		printf("%s%s - %s: %s\n%s", Logger::LevelAsColor(level), Logger::LevelAsString(level), section, str, Logger::ResetColor);
		if (LogFile) {
			fprintf(LogFile, "%s - %s: %s\n", Logger::LevelAsString(level), section, str);
			fflush(LogFile);
		}
	};

	LogFile = fopen(LogPath.string().c_str(), "w");
	if (!LogFile) {
		MESSAGE_ERROR("Could not create a log file! Without it, I can't assist you with any issues.");
		LOG_ERROR("Could not create log file!");
	}

	LOG_INFO("Starting cApp");
	return;

setupExit:
	MESSAGE_ERROR(SetupError);
	exit(0);
}

cApp::~cApp() {
	LOG_INFO("Deconst cApp");
	fclose(LogFile);
}

bool cApp::OnInit() {
	LOG_INFO("Loading WinFsp");
	auto result = LoadWinFsp();
	if (result != WinFspCheckResult::LOADED) {
		LOG_FATAL("WinFsp failed with %d", result);
		switch (result)
		{
		case WinFspCheckResult::NO_PATH:
			MESSAGE_ERROR("Could not get your Program Files (x86) folder. I honestly have no idea how you'd get this error.");
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
	LOG_DEBUG("Loaded WinFsp");

	LOG_INFO("Setting up WinToast");
	if (!WinToast::isCompatible()) {
		LOG_FATAL("WinToast isn't compatible");
		return false;
	}
	LOG_DEBUG("Configuring WinToast");
	WinToast::instance()->setAppName(L"EGL2");
	WinToast::instance()->setAppUserModelId(WinToast::configureAUMI(L"workingrobot", L"egl2"));
	if (!WinToast::instance()->initialize()) {
		LOG_FATAL("Could not intialize WinToast");
		return false;
	}
	LOG_DEBUG("Set up WinToast");

	LOG_INFO("Setting up symlink workaround");
	if (!IsDeveloperModeEnabled()) {
		if (!IsUserAdmin()) {
			auto cmd = GetCommandLine();
			int l = wcslen(argv[0].wc_str());
			if (cmd == wcsstr(cmd, argv[0].wc_str()))
			{
				cmd = cmd + l;
				while (*cmd && isspace(*cmd))
					++cmd;
			}

			ShellExecute(NULL, L"runas", argv[0].wc_str(), cmd, NULL, SW_SHOWDEFAULT);
			return false;
		}
		else {
			EnableDeveloperMode();
			LOG_DEBUG("Set up developer mode");
		}
	}

	LOG_INFO("Setting up cMain");
	(new cMain(DataFolder / "config", DataFolder / "manifests"))->Show();

	LOG_DEBUG("Set up cApp");
	return true;
}