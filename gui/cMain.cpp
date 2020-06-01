#include "cMain.h"

#define CMAIN_W				450
#define CMAIN_H				330

#define LAUNCH_GAME_ARGS   "-AUTH_LOGIN=unused AUTH_TYPE=exchangecode -epicapp=Fortnite -epicenv=Prod -epicportal -epiclocale=en-us -AUTH_PASSWORD=%s %s"

#define MOUNT_FOLDER	   "game"

#ifndef LOG_SECTION
#define LOG_SECTION "cMain"
#endif

#include "../checks/symlink_workaround.h"
#include "../checks/wintoast_handler.h"
#include "../Logger.h"
#include "../Stats.h"
#include "cProgress.h"
#include "cSetup.h"
#include "Localization.h"
#include "taskbar.h"

#include <atomic>
#include <thread>
#include <TlHelp32.h>
#include <wx/gbsizer.h>
#include <wx/notifmsg.h>
#include <wx/windowptr.h>

#define SIDE_BUTTON_CREATE(name, text) \
	auto btn_frame_##name = new wxPanel(panel, wxID_ANY); \
	auto btn_sizer_##name = new wxBoxSizer(wxVERTICAL); \
    auto btn_##name = new wxButton(btn_frame_##name, wxID_ANY, text, wxDefaultPosition, wxSize(120, -1)); \
	btn_sizer_##name->Add(btn_##name, 1, wxEXPAND); \
	btn_frame_##name->SetSizerAndFit(btn_sizer_##name);
#define SIDE_BUTTON_BIND(name, func) \
	btn_##name->Bind(wxEVT_BUTTON, func);
#define SIDE_BUTTON_FRAME(name) btn_frame_##name
#define SIDE_BUTTON_OBJ(name) btn_##name
#define SIDE_BUTTON_DESC(name, desc) \
	btn_frame_##name->Bind(wxEVT_MOTION, std::bind(&cMain::OnButtonHover, this, desc)); \
	btn_##name->Bind(wxEVT_MOTION, std::bind(&cMain::OnButtonHover, this, desc)); \
	btn_frame_##name->Bind(wxEVT_LEAVE_WINDOW, std::bind(&cMain::OnButtonHover, this, LSTR(MAIN_DESC_DEFAULT))); \
	btn_##name->Bind(wxEVT_LEAVE_WINDOW, std::bind(&cMain::OnButtonHover, this, LSTR(MAIN_DESC_DEFAULT)));

#define CREATE_STAT(name, displayName, range) \
	stat##name##Label = new wxStaticText(panel, wxID_ANY, displayName, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT); \
	stat##name##Value = new wxGauge(panel, wxID_ANY, range, wxDefaultPosition, wxSize(70, -1)); \
	stat##name##Text = new wxStaticText(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(45, -1)); \
	if (statColInd & 0x1) { \
		statsSizerR->Add(stat##name##Label, wxGBPosition(statColInd / 2, 0), wxGBSpan(1, 1), wxEXPAND); \
		statsSizerR->Add(stat##name##Value,      wxGBPosition(statColInd / 2, 1), wxGBSpan(1, 1), wxEXPAND); \
		statsSizerR->Add(stat##name##Text,       wxGBPosition(statColInd / 2, 2), wxGBSpan(1, 1), wxEXPAND); \
	} \
	else { \
		statsSizerL->Add(stat##name##Label, wxGBPosition(statColInd / 2, 0), wxGBSpan(1, 1), wxEXPAND); \
		statsSizerL->Add(stat##name##Value,      wxGBPosition(statColInd / 2, 1), wxGBSpan(1, 1), wxEXPAND); \
		statsSizerL->Add(stat##name##Text,       wxGBPosition(statColInd / 2, 2), wxGBSpan(1, 1), wxEXPAND); \
	} \
	statColInd++;
#define STAT_VALUE(name) stat##name##Value
#define STAT_TEXT(name) stat##name##Text

cMain::cMain(const fs::path& settingsPath, const fs::path& manifestPath, const std::shared_ptr<PersonalAuth>& personalAuth) : wxFrame(nullptr, wxID_ANY, "EGL2", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE ^ (wxMAXIMIZE_BOX | wxRESIZE_BORDER)),
	SettingsPath(settingsPath),
	Settings(SettingsDefault()),
	Auth(personalAuth),
	UpdateAvailable(false) {
	LOG_DEBUG("Setting up (%s, %s)", settingsPath.string().c_str(), manifestPath.string().c_str());

	this->SetIcon(wxICON(APP_ICON));
	this->SetMinSize(wxSize(CMAIN_W, CMAIN_H));
	this->SetMaxSize(wxSize(CMAIN_W, CMAIN_H));

	Systray = new SystrayIcon(this);
	Systray->SetIcon(wxICON(APP_ICON), "EGL2");
	wxNotificationMessage::UseTaskBarIcon(Systray);

	LOG_DEBUG("Setting up UI");

	panel = new wxPanel(this, wxID_ANY,
		wxDefaultPosition,
		wxDefaultSize,
		wxTAB_TRAVERSAL);

	SIDE_BUTTON_CREATE(settings, LSTR(MAIN_BTN_SETTINGS));
	SIDE_BUTTON_CREATE(verify, LSTR(MAIN_BTN_VERIFY));
	SIDE_BUTTON_CREATE(play, LSTR(MAIN_BTN_PLAY));

	SIDE_BUTTON_BIND(settings, std::bind(&cMain::OnSettingsClicked, this, false));
	SIDE_BUTTON_BIND(verify, std::bind(&cMain::OnVerifyClicked, this));
	SIDE_BUTTON_BIND(play, std::bind(&cMain::OnPlayClicked, this));
	this->playBtn = SIDE_BUTTON_OBJ(play);
	this->verifyBtn = SIDE_BUTTON_OBJ(verify);

	SIDE_BUTTON_OBJ(verify)->Disable();
	SIDE_BUTTON_OBJ(play)->Disable();

	SIDE_BUTTON_DESC(settings, LSTR(MAIN_DESC_SETTINGS));
	SIDE_BUTTON_DESC(verify, LSTR(MAIN_DESC_VERIFY));
	SIDE_BUTTON_DESC(play, LSTR(MAIN_DESC_PLAY));

	statusBar = new wxStaticText(panel, wxID_ANY, wxEmptyString);
	selloutBar = new wxStaticText(panel, wxID_ANY, LSTR(MAIN_STATUS_SELLOUT));

	descBox = new wxStaticBoxSizer(wxVERTICAL, panel, LSTR(MAIN_DESC_TITLE));
	descTxt = new wxStaticText(panel, wxID_ANY, LSTR(MAIN_DESC_DEFAULT));
	descBox->Add(descTxt, 1, wxEXPAND);

	statsBox = new wxStaticBoxSizer(wxVERTICAL, panel, LSTR(MAIN_STATS_TITLE));
	auto statsSizer = new wxBoxSizer(wxHORIZONTAL);
	auto statsSizerL = new wxGridBagSizer(2, 4);
	auto statsSizerR = new wxGridBagSizer(2, 4);
	int statColInd = 0;

	CREATE_STAT(cpu, LSTR(MAIN_STATS_CPU), 1000); // divide by 10 to get %
	CREATE_STAT(ram, LSTR(MAIN_STATS_RAM), 384 * 1024 * 1024); // 384 mb
	CREATE_STAT(read, LSTR(MAIN_STATS_READ), 256 * 1024 * 1024); // 256 mb/s
	CREATE_STAT(write, LSTR(MAIN_STATS_WRITE), 64 * 1024 * 1024); // 64 mb/s
	CREATE_STAT(provide, LSTR(MAIN_STATS_PROVIDE), 256 * 1024 * 1024); // 256 mb/s
	CREATE_STAT(download, LSTR(MAIN_STATS_DOWNLOAD), 64 * 1024 * 1024); // 512 mbps
	CREATE_STAT(latency, LSTR(MAIN_STATS_LATENCY), 1000); // divide by 10 to get ms
	CREATE_STAT(threads, LSTR(MAIN_STATS_THREADS), 128); // 128 threads (threads don't ruin performance, probably just indicates overhead)

	statsSizer->Add(statsSizerL);
	statsSizer->AddStretchSpacer();
	statsSizer->Add(statsSizerR);
	statsBox->Add(statsSizer, 1, wxEXPAND);

	auto barSizer = new wxBoxSizer(wxHORIZONTAL);
	barSizer->Add(statusBar);
	barSizer->AddStretchSpacer();
	barSizer->Add(selloutBar);

	auto buttonSizer = new wxBoxSizer(wxVERTICAL);
	buttonSizer->Add(SIDE_BUTTON_FRAME(settings), 1, wxEXPAND);
	buttonSizer->Add(SIDE_BUTTON_FRAME(verify), 1, wxEXPAND);
	buttonSizer->Add(SIDE_BUTTON_FRAME(play), 1, wxEXPAND);

	auto grid = new wxGridBagSizer(2, 2);
	grid->Add(buttonSizer, wxGBPosition(0, 0), wxGBSpan(1, 1), wxEXPAND);
	grid->Add(descBox, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND);
	grid->Add(statsBox, wxGBPosition(1, 0), wxGBSpan(1, 2), wxEXPAND);
	grid->Add(barSizer, wxGBPosition(2, 0), wxGBSpan(1, 2), wxEXPAND);

	grid->AddGrowableCol(0, 1);
	grid->AddGrowableCol(1, 1);
	grid->AddGrowableRow(0);

	auto topSizer = new wxBoxSizer(wxVERTICAL);
	topSizer->Add(grid, wxSizerFlags(1).Expand().Border(wxALL, 5));
	this->SetSize(wxSize(CMAIN_W, CMAIN_H));
	panel->SetSizerAndFit(topSizer);
	grid->RepositionChildren(wxSize(-1, -1));

	LOG_DEBUG("Getting settings");
	auto settingsFp = fopen(SettingsPath.string().c_str(), "rb");
	if (settingsFp) {
		LOG_DEBUG("Reading settings file");
		SettingsRead(&Settings, settingsFp);
		fclose(settingsFp);
	}
	else {
		LOG_DEBUG("Using default settings");
	}

	this->Show();
	this->Restore();
	this->Raise();
	this->SetFocus();

	LOG_DEBUG("Validating settings");
	if (!SettingsValidate(&Settings)) {
		LOG_WARN("Invalid settings");
		OnSettingsClicked(true);
		if (!SettingsValidate(&Settings)) {
			LOG_FATAL("Cancelled out of setup, closing");
			this->Destroy();
			return;
		}
	}

	Stats::StartUpdateThread(ch::milliseconds(500), [this](StatsUpdateData& data) {
		STAT_VALUE(cpu)->SetValue(1000);
		STAT_VALUE(cpu)->SetValue(std::min(data.cpu * 10, 1000.f));
		STAT_TEXT(cpu)->SetLabel(data.cpu > 0 ? wxString::Format("%.*f%%", std::max(2 - (int)floor(log10(data.cpu)), 1), data.cpu) : "0.00%");

		STAT_VALUE(ram)->SetValue(384 * 1024 * 1024);
		STAT_VALUE(ram)->SetValue(std::min(data.ram, (size_t)384 * 1024 * 1024));
		STAT_TEXT(ram)->SetLabel(Stats::GetReadableSize(data.ram));

		STAT_VALUE(read)->SetValue(256 * 1024 * 1024);
		STAT_VALUE(read)->SetValue(std::min(data.read, (size_t)256 * 1024 * 1024));
		STAT_TEXT(read)->SetLabel(Stats::GetReadableSize(data.read));

		STAT_VALUE(write)->SetValue(64 * 1024 * 1024);
		STAT_VALUE(write)->SetValue(std::min(data.write, (size_t)64 * 1024 * 1024));
		STAT_TEXT(write)->SetLabel(Stats::GetReadableSize(data.write));

		STAT_VALUE(provide)->SetValue(256 * 1024 * 1024);
		STAT_VALUE(provide)->SetValue(std::min(data.provide, (size_t)256 * 1024 * 1024));
		STAT_TEXT(provide)->SetLabel(Stats::GetReadableSize(data.provide));

		STAT_VALUE(download)->SetValue(64 * 1024 * 1024);
		STAT_VALUE(download)->SetValue(std::min(data.download, (size_t)64 * 1024 * 1024));
		STAT_TEXT(download)->SetLabel(Stats::GetReadableSize(data.download));

		STAT_VALUE(latency)->SetValue(1000);
		STAT_VALUE(latency)->SetValue(std::min(data.latency * 10, 1000.f));
		STAT_TEXT(latency)->SetLabel(data.latency > 0 ? wxString::Format("%.*f ms", std::max(2 - (int)floor(log10(data.latency)), 1), data.latency) : "0 ms");

		STAT_VALUE(threads)->SetValue(128);
		STAT_VALUE(threads)->SetValue(std::min(data.threads, 128));
		STAT_TEXT(threads)->SetLabel(wxString::Format("%d", data.threads));
	});

	Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& evt) {
		this->Hide();
	});

	std::thread([=]() {
		SetStatus(LSTR(MAIN_STATUS_STARTING));
		LOG_DEBUG("Creating update checker");
		Checker = std::make_unique<UpdateChecker>(
			manifestPath,
			[this](const std::string& Url, const std::string& Version) { OnUpdate(Version, Url); },
			SettingsGetUpdateInterval(&Settings));
		Mount(Checker->GetLatestUrl());
		LOG_DEBUG("Enabling buttons");
		SetStatus(LSTR(MAIN_STATUS_PLAYABLE));
		SIDE_BUTTON_OBJ(verify)->Enable();
		SIDE_BUTTON_OBJ(play)->Enable();
		LOG_DEBUG("Checking chunk count");
		auto ct = Build->GetMissingChunkCount();
		LOG_DEBUG("%d missing chunks", ct);
		if (ct) {
			OnUpdate(Checker->GetLatestVersion());
		}
	}).detach();
}

cMain::~cMain() {

}

void cMain::OnButtonHover(const wxString& string) {
	if (descTxt->GetLabel().compare(string)) {
		descTxt->SetLabel(string);
		descBox->Fit(descTxt);
		descBox->FitInside(descTxt);
		descBox->Layout();
	}
}

void cMain::OnSettingsClicked(bool onStartup) {
	if (!CurrentModal) {
		CurrentModal = new cSetup(this, &Settings, onStartup, [this](SETTINGS* settings) {
			auto settingsFp = fopen(SettingsPath.string().c_str(), "wb");
			if (settingsFp) {
				SettingsWrite(settings, settingsFp);
				fclose(settingsFp);
			}
			else {
				LOG_ERROR("Could not open settings file to write");
			}
		}, &SettingsValidate, [this]() {
			if (Checker) {
				Checker->SetInterval(SettingsGetUpdateInterval(&Settings));
			}
			CurrentModal = nullptr;
			this->Raise();
			this->SetFocus();
		});
	}
	else {
		CurrentModal->Restore();
		CurrentModal->Raise();
		CurrentModal->SetFocus();
	}
}

#define RUN_PROGRESS(taskName, funcName, ...)				 \
{															 \
	auto cancelled = new cancel_flag();						 \
	cProgress* progress = new cProgress(this, taskName,		 \
		*cancelled);										 \
	CurrentModal = progress;								 \
	progress->Show(true);									 \
														 	 \
	wxWindowPtr progressPtr(progress);				 		 \
	std::thread([=]() {										 \
		Build->##funcName(									 \
			[=](uint32_t m) { progressPtr->SetMaximum(m); }, \
			[=]() { progressPtr->Increment(); },			 \
			*cancelled, __VA_ARGS__);						 \
															 \
		progressPtr->Finish();								 \
		progressPtr->Close();								 \
		delete cancelled;									 \
		CurrentModal = nullptr;								 \
		this->Raise();										 \
		this->SetFocus();									 \
	}).detach();											 \
}

void cMain::OnVerifyClicked() {
	if (!CurrentModal) {
		RUN_PROGRESS(LSTR(MAIN_PROG_VERIFY), VerifyAllChunks, Settings.ThreadCount);
	}
	else {
		CurrentModal->Restore();
		CurrentModal->Raise();
		CurrentModal->SetFocus();
	}
}

void cMain::OnPlayClicked() {
	if (UpdateAvailable) {
		BeginUpdate();
	}
	else {
		if (FirstAuthLaunched) {
			LOG_DEBUG("Recreating auth");
			Auth->Recreate();
		}
		std::string code;
		LOG_DEBUG("Getting exchange code");
		if (Auth->GetExchangeCode(code)) {
			LOG_INFO("Launching game");
			Build->LaunchGame(wxString::Format(LAUNCH_GAME_ARGS, code, Settings.CommandArgs).c_str());
			FirstAuthLaunched = true;
			playBtn->Disable();
			std::thread([this]() {
				std::this_thread::sleep_for(ch::seconds(60));
				LOG_DEBUG("Re-enabling play button");
				playBtn->Enable();
			}).detach(); // no spamming :)
		}
		else {
			LOG_ERROR("Could not get exchange code to launch");
		}
	}
}

bool cMain::OnClose()
{
	if (Build) {
		bool gameRunning = false;
		{
			auto procId = GetCurrentProcessId();
			HANDLE hnd = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, procId);
			if (hnd != INVALID_HANDLE_VALUE) {
				PROCESSENTRY32 procEntry;
				procEntry.dwSize = sizeof PROCESSENTRY32;

				if (Process32First(hnd, &procEntry)) {
					do {
						if (procEntry.th32ParentProcessID == procId) {
							gameRunning = true;
							break;
						}
					} while (Process32Next(hnd, &procEntry));
				}

				CloseHandle(hnd);
			}
		}
		if (gameRunning && wxMessageBox(LSTR(MAIN_EXIT_VETOMSG), LTITLE(LSTR(MAIN_EXIT_VETOTITLE)), wxICON_QUESTION | wxYES_NO) != wxYES)
		{
			return false;
		}
	}
	return true;
}

void cMain::SetStatus(const wxString& string) {
	statusBar->SetLabel(string);
}

void cMain::OnUpdate(const std::string& Version, const std::optional<std::string>& Url)
{
	UpdateAvailable = true;
	playBtn->SetLabel(LSTR(MAIN_BTN_UPDATE));
	if (Url.has_value()) {
		UpdateUrl = Url;
	}

	auto notif = new wxNotificationMessage(LSTR(MAIN_NOTIF_TITLE), wxString::Format(LSTR(MAIN_NOTIF_DESC), UpdateChecker::GetReadableVersion(Version)), this);
	notif->SetIcon(wxICON(APP_ICON));
	notif->AddAction(wxID_ANY, LSTR(MAIN_NOTIF_ACTION));
	notif->Bind(wxEVT_NOTIFICATION_MESSAGE_ACTION, std::bind(&cMain::BeginUpdate, this));
	notif->Bind(wxEVT_NOTIFICATION_MESSAGE_CLICK, std::bind(&cMain::BeginUpdate, this));
	if (!notif->Show(wxNotificationMessage::Timeout_Never)) {
		LOG_ERROR("Couldn't launch update notification");
	}
}

void cMain::BeginUpdate()
{
	if (CurrentModal) {
		CurrentModal->Restore();
		CurrentModal->Raise();
		CurrentModal->SetFocus();
		return;
	}

	if (!UpdateAvailable) {
		return;
	}
	std::thread([this]() {
		if (!UpdateUrl.has_value()) {
			LOG_DEBUG("Downloading unavailable chunks");
		}
		else {
			LOG_DEBUG("Beginning update: %s", UpdateUrl->c_str());
			Mount(*UpdateUrl);
		}

		this->CallAfter([this]() {
			RUN_PROGRESS(LSTR(MAIN_PROG_UPDATE), PreloadAllChunks, Settings.BufferCount);
		});

		Build->PurgeUnusedChunks();
		LOG_DEBUG("Purged chunks");
	}).detach();

	UpdateAvailable = false;
	playBtn->SetLabel(LSTR(MAIN_BTN_PLAY));
	UpdateUrl.reset();
}

void cMain::Mount(const std::string& Url) {
	LOG_INFO("Setting up cache directory");
	MountedBuild::SetupCacheDirectory(Settings.CacheDir);
	LOG_INFO("Mounting new url: %s", Url.c_str());
	Build.reset(new MountedBuild(Checker->GetManifest(Url), fs::path(Settings.CacheDir) / MOUNT_FOLDER, Settings.CacheDir, SettingsGetStorageFlags(&Settings), Settings.BufferCount));
	LOG_INFO("Setting up game dir");
	Build->SetupGameDirectory(Settings.ThreadCount);
}