#include "cApp.h"

#ifndef LOG_SECTION
#define LOG_SECTION "cApp"
#endif

#include "../checks/oodle_handler.h"
#include "../checks/symlink_workaround.h"
#include "../checks/winfspcheck.h"
#include "../Logger.h"
#include "../storage/EGSProvider.h"
#include "cSplash.h"
#include "Localization.h"

#include <oodle2.h>
#include <ShlObj_core.h>
#include <sstream>
#include <wx/notifmsg.h>

#define MESSAGE_ERROR(format, ...) wxMessageBox(wxString::Format(LSTR(format), __VA_ARGS__), LTITLE(LSTR(APP_ERROR)), wxICON_ERROR | wxOK | wxCENTRE)

cApp::cApp() {

}

inline uint32_t GetThreadId() {
	auto id = std::this_thread::get_id();
	return *(uint32_t*)&id;
}

cApp::~cApp() {
	if (InstanceChecker) {
		delete InstanceChecker;
	}
	LOG_INFO("Deconst cApp");
	if (LogFile) {
		fclose(LogFile);
	}
}

bool cApp::OnInit() {
	SplashWindow = new cSplash();
	SplashWindow->Show();
	std::thread([this] {
		if (!InitThread()) {
			this->Exit();
		}
	}).detach();
	return true;
}

std::ostringstream preLogStream;

bool cApp::InitThread() {
	Logger::Setup();
	Logger::Callback = [](Logger::LogLevel level, const char* section, const char* str) {
		auto logString = wxString::Format("%s - %d %s: %s\n", Logger::LevelAsString(level), GetThreadId(), section, str);
		preLogStream.write(logString.c_str(), logString.size());
	};

	LOG_INFO("Loading locales");
	if (!Localization::UseLocale(Localization::GetSystemLocale())) {
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
			fprintf(LogFile, "%s - %d %s: %s\n", Logger::LevelAsString(level), GetThreadId(), section, str);
			fflush(LogFile);
		};
		auto preLogData = preLogStream.str();
		fwrite(preLogData.c_str(), preLogData.size(), 1, LogFile);
		preLogStream.clear();
	}
	LOG_DEBUG("Created file logger");

	LOG_INFO("Loading WinFsp");
	{
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
	}
	LOG_DEBUG("Loaded WinFsp");

	LOG_INFO("Loading Oodle");
	{
		auto result = LoadOodle(DataFolder);
		if (result != OodleHandlerResult::LOADED) {
			LOG_FATAL("Oodle failed with %d", result);
			switch (result)
			{
			case OodleHandlerResult::NET_ERROR:
				MESSAGE_ERROR(APP_ERROR_NETWORK);
				break;
			case OodleHandlerResult::LZMA_ERROR:
				MESSAGE_ERROR(APP_ERROR_OODLE_LZMA);
				break;
			case OodleHandlerResult::INDEX_ERROR:
				MESSAGE_ERROR(APP_ERROR_OODLE_INDEX);
				break;
			case OodleHandlerResult::CANNOT_LOAD:
				MESSAGE_ERROR(APP_ERROR_OODLE_LOAD);
				break;
			case OodleHandlerResult::CANNOT_WRITE:
				MESSAGE_ERROR(APP_ERROR_OODLE_WRITE);
				break;
			default:
				MESSAGE_ERROR(APP_ERROR_OODLE_UNKNOWN, result);
				break;
			}
			return false;
		}

		LOG_DEBUG("Checking Oodle compatability");
		U32 dllVer;
		if (!Oodle_CheckVersion(OodleSDKVersion, &dllVer)) {
			LOG_FATAL("Oodle is incompatible! SDK: %08X, DLL: %08X", OodleSDKVersion, dllVer);
			MESSAGE_ERROR(APP_ERROR_OODLE_INCOMPAT, OodleSDKVersion, dllVer);
			return false;
		}
		LOG_DEBUG("Oodle versions: SDK: %08X, DLL: %08X", OodleSDKVersion, dllVer);

		OodleCore_Plugins_SetPrintf([](bool debug, const char* filename, uint32_t line_num, const char* format, ...) {
			std::string frmt(format, strlen(format) - 1); // removes newline (and doesn't overwrite dll data)
			va_list args;
			va_start(args, format);
			if (debug) {
				LOG_VA_DEBUG(frmt.c_str(), args);
			}
			else {
				LOG_VA_ERROR(frmt.c_str(), args);
			}
			va_end(args);
		});

		Oodle_LogHeader();
	}
	LOG_DEBUG("Loaded Oodle");

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
	if (IsUserAdmin()) { // basically just relaunches in user mode because folder permissions
		// taken from https://docs.microsoft.com/en-us/archive/blogs/aaron_margosis/faq-how-do-i-start-a-program-as-the-desktop-user-from-an-elevated-app
		DWORD dwPID;
		GetWindowThreadProcessId(GetShellWindow(), &dwPID);

		HANDLE hShellProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwPID);

		HANDLE hShellProcessToken;
		OpenProcessToken(hShellProcess, TOKEN_DUPLICATE, &hShellProcessToken);

		constexpr DWORD dwTokenRights = TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID;
		HANDLE hPrimaryToken;
		DuplicateTokenEx(hShellProcessToken, dwTokenRights, NULL, SecurityImpersonation, TokenPrimary, &hPrimaryToken);

		auto cmd = GetCommandLine();
		int l = wcslen(argv[0].wc_str());
		if (cmd == wcsstr(cmd, argv[0].wc_str()))
		{
			cmd = cmd + l;
			while (*cmd && isspace(*cmd))
				++cmd;
		}

		PROCESS_INFORMATION pi;
		STARTUPINFO si;

		memset(&pi, 0, sizeof(pi));
		memset(&si, 0, sizeof(si));
		si.cb = sizeof(si);

		if (CreateProcessWithTokenW(hPrimaryToken, 0, argv[0].wc_str(), cmd, 0, NULL, NULL, &si, &pi))
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}

		CloseHandle(hShellProcess);
		CloseHandle(hShellProcessToken);
		CloseHandle(hPrimaryToken);
		return false;
	}
	LOG_DEBUG("Set up workaround");

	LOG_INFO("Checking for existing EGS install");
	LOG_INFO("EGS is %savailable", EGSProvider::Available() ? "" : "not ");

	LOG_INFO("Setting up auth");
	AuthDetails = std::make_shared<PersonalAuth>(DataFolder / "auth", [](const std::string& verifUrl, const std::string& userCode) {
		LOG_DEBUG("Launching browser to authorize with device code: %s", userCode.c_str());
		wxLaunchDefaultBrowser(verifUrl);
	});

	this->CallAfter([this] {
		LOG_INFO("Setting up cMain");
		new cMain(this, DataFolder / "config", DataFolder / "manifests", AuthDetails);

		SplashWindow->Close();
		LOG_DEBUG("Set up cApp");
	});
	return true;
}