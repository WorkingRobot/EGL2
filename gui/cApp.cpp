#include "cApp.h"

#ifndef LOG_SECTION
#define LOG_SECTION "cApp"
#endif

#include "../checks/symlink_workaround.h"
#include "../checks/winfspcheck.h"
#include "../Logger.h"
#include "Localization.h"

#include <ShlObj_core.h>
#include <sstream>
#include <wintoastlib.h>

using namespace WinToastLib;

#define MESSAGE_ERROR(format, ...) wxMessageBox(wxString::Format(LSTR(format), __VA_ARGS__), LTITLE(LSTR(APP_ERROR)), wxICON_ERROR | wxOK | wxCENTRE)

cApp::cApp() {
	if (!Localization::InitializeLocales()) {
		wxMessageBox("Could not load locale data!", LTITLE("Error"), wxICON_ERROR | wxOK | wxCENTRE);
		exit(0);
		return;
	}

	const char* SetupError = nullptr;
	fs::path LogPath;
	{
		PWSTR appDataFolder;
		if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataFolder) != S_OK) {
			MESSAGE_ERROR(APP_ERROR_APPDATA);
			exit(0);
			return;
		}
		DataFolder = appDataFolder;
		CoTaskMemFree(appDataFolder);
	}
	DataFolder /= "EGL2";
	if (!fs::create_directories(DataFolder) && !fs::is_directory(DataFolder)) {
		MESSAGE_ERROR(APP_ERROR_DATA);
		exit(0);
		return;
	}

	if (!fs::create_directories(DataFolder / "logs") && !fs::is_directory(DataFolder / "logs")) {
		MESSAGE_ERROR(APP_ERROR_LOGS);
		exit(0);
		return;
	}

	if (!fs::create_directories(DataFolder / "manifests") && !fs::is_directory(DataFolder / "manifests")) {
		MESSAGE_ERROR(APP_ERROR_MANIFESTS);
		exit(0);
		return;
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

	LogFile = fopen(LogPath.string().c_str(), "w");
	if (!LogFile) {
		MESSAGE_ERROR(APP_ERROR_LOGFILE);
		LOG_ERROR("Could not create log file!");
	}
	else {
		Logger::Callback = [this](Logger::LogLevel level, const char* section, const char* str) {
			fprintf(LogFile, "%s - %s: %s\n", Logger::LevelAsString(level), section, str);
			fflush(LogFile);
		};
	}

	LOG_INFO("Starting cApp");
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
			MESSAGE_ERROR(APP_ERROR_PROGFILES86);
			break;
		case WinFspCheckResult::NO_DLL:
			MESSAGE_ERROR(APP_ERROR_WINFSP_FIND);
			break;
		case WinFspCheckResult::CANNOT_LOAD:
			MESSAGE_ERROR(APP_ERROR_WINFSP_LOAD);
			break;
		default:
			MESSAGE_ERROR(APP_ERROR_WINFSP_UNKNOWN, result);
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

	LOG_INFO("Setting up auth");
	AuthDetails = std::make_shared<PersonalAuth>(DataFolder / "auth", [](const std::string& verifUrl, const std::string& userCode) {
		LOG_DEBUG("Launching browser to authorize with device code: %s", userCode.c_str());
		wxLaunchDefaultBrowser(verifUrl);
	});

	LOG_INFO("Setting up cMain");
	(new cMain(DataFolder / "config", DataFolder / "manifests", AuthDetails))->Show();

	LOG_DEBUG("Set up cApp");
	return true;
}