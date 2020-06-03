#include "cApp.h"

#ifndef LOG_SECTION
#define LOG_SECTION "cApp"
#endif

#include "../checks/symlink_workaround.h"
#include "../checks/winfspcheck.h"
#include "../Logger.h"
#include "../storage/EGSProvider.h"
#include "Localization.h"

#include <ShlObj_core.h>
#include <sstream>
#include <wx/notifmsg.h>

#define MESSAGE_ERROR(format, ...) wxMessageBox(wxString::Format(LSTR(format), __VA_ARGS__), LTITLE(LSTR(APP_ERROR)), wxICON_ERROR | wxOK | wxCENTRE)

cApp::cApp() {

}

cApp::~cApp() {
	LOG_INFO("Deconst cApp");
	if (LogFile) {
		fclose(LogFile);
	}
}

bool cApp::OnInit() {
	Logger::Setup();
	std::ostringstream preLogStream;
	Logger::Callback = [&preLogStream](Logger::LogLevel level, const char* section, const char* str) {
		auto logString = wxString::Format("%s - %s: %s\n", Logger::LevelAsString(level), section, str);
		preLogStream.write(logString.c_str(), logString.size());
	};

	LOG_INFO("Loading locales");
	if (!Localization::InitializeLocales()) {
		wxMessageBox("Could not load locale data!", LTITLE("Error"), wxICON_ERROR | wxOK | wxCENTRE);
		return false;
	}
	LOG_DEBUG("Loaded locales");

	LOG_INFO("Checking for EGL2 instance");
	InstanceChecker = new wxSingleInstanceChecker();
	if (InstanceChecker->Create("EGL2Instance")) {
		if (InstanceChecker->IsAnotherRunning()) {
			MESSAGE_ERROR(APP_ERROR_RUNNING);
			return false;
		}
		LOG_DEBUG("EGL2 was not running");
	}
	else {
		LOG_WARN("Failed to create mutex, another instance may be running");
	}

	LOG_INFO("Loading data dirs");
	fs::path LogPath;
	{
		PWSTR appDataFolder;
		if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataFolder) != S_OK) {
			MESSAGE_ERROR(APP_ERROR_APPDATA);
			return false;
		}
		DataFolder = fs::path(appDataFolder) / "EGL2";
		CoTaskMemFree(appDataFolder);
	}
	if (!fs::create_directories(DataFolder) && !fs::is_directory(DataFolder)) {
		MESSAGE_ERROR(APP_ERROR_DATA);
		return false;
	}
	if (!fs::create_directories(DataFolder / "logs") && !fs::is_directory(DataFolder / "logs")) {
		MESSAGE_ERROR(APP_ERROR_LOGS);
		return false;
	}
	if (!fs::create_directories(DataFolder / "manifests") && !fs::is_directory(DataFolder / "manifests")) {
		MESSAGE_ERROR(APP_ERROR_MANIFESTS);
		return false;
	}
	LOG_DEBUG("Loaded data dirs");

	LOG_INFO("Creating file logger");
	{
		auto logTime = std::time(nullptr);
		std::stringstream ss;
		ss << std::put_time(std::localtime(&logTime), "%F_%T.log"); // ISO 8601 without timezone information.
		auto s = ss.str();
		std::replace(s.begin(), s.end(), ':', '-');
		LogPath = DataFolder / "logs" / s;
	}

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
		auto preLogData = preLogStream.str();
		fwrite(preLogData.c_str(), preLogData.size(), 1, LogFile);
		preLogStream.clear();
	}
	LOG_DEBUG("Created file logger");

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

	LOG_INFO("Setting up notifications");
	if (!wxNotificationMessage::MSWUseToasts("EGL2", "workingrobot.egl2")) {
		LOG_ERROR("Could not setup toasts, maybe you're using Windows 7 (if so, ignore this error)");
	}
	LOG_DEBUG("Set up notfications");

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
			LOG_INFO("Set up developer mode");
		}
	}
	LOG_DEBUG("Set up workaround");

	LOG_INFO("Checking for existing EGS install");
	EGSProvider::Available();

	LOG_INFO("Setting up auth");
	AuthDetails = std::make_shared<PersonalAuth>(DataFolder / "auth", [](const std::string& verifUrl, const std::string& userCode) {
		LOG_DEBUG("Launching browser to authorize with device code: %s", userCode.c_str());
		wxLaunchDefaultBrowser(verifUrl);
	});

	LOG_INFO("Setting up cMain");
	(new cMain(this, DataFolder / "config", DataFolder / "manifests", AuthDetails))->Show();

	LOG_DEBUG("Set up cApp");
	return true;
}