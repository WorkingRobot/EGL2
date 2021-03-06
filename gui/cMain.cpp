﻿#include "cMain.h"

#define CMAIN_W				550
#define CMAIN_H				330

#define LAUNCH_GAME_ARGS   "-AUTH_LOGIN=unused AUTH_TYPE=exchangecode -epicapp=Fortnite -epicenv=Prod -epicportal -epiclocale=en-us -AUTH_PASSWORD=%s %s"

#define MOUNT_FOLDER	   "fn"

#ifndef LOG_SECTION
#define LOG_SECTION "cMain"
#endif

#include "../checks/symlink_workaround.h"
#include "../Logger.h"
#include "../Stats.h"
#include "Localization.h"
#include "taskbar.h"

#include <atomic>
#include <thread>
#include <TlHelp32.h>
#include <wx/gbsizer.h>
#include <wx/notifmsg.h>
#include <wx/generic/notifmsg.h>

#define CREATE_STAT(name, displayName, range) \
	stat##name##Label = new wxStaticText(panel, wxID_ANY, displayName, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT); \
	stat##name##Value = new wxGauge(panel, wxID_ANY, range, wxDefaultPosition, wxSize(100, -1)); \
	stat##name##Text = new wxStaticText(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(50, -1)); \
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

cMain::cMain(wxApp * app, const fs::path & settingsPath, const fs::path & manifestPath, const std::shared_ptr<PersonalAuth> & personalAuth) : wxFrame(nullptr, wxID_ANY, "EGL2", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE ^ (wxMAXIMIZE_BOX | wxRESIZE_BORDER)),
App(app),
SettingsPath(settingsPath),
Settings(SettingsDefault()),
Auth(personalAuth),
GameUpdateAvailable(false) {
	LOG_DEBUG("Setting up (%s, %s)", settingsPath.string().c_str(), manifestPath.string().c_str());

	this->SetIcon(wxICON(APP_ICON));
	this->SetMinSize(wxSize(CMAIN_W, CMAIN_H));
	this->SetMaxSize(wxSize(CMAIN_W, CMAIN_H));

	LOG_DEBUG("Setting up UI");
	{
		panel = new wxPanel(this, wxID_ANY,
			wxDefaultPosition,
			wxDefaultSize,
			wxTAB_TRAVERSAL);

		{
			auto SizeHandler = [](wxHelpButton* helpBtn) {
				helpBtn->SetPosition(wxPoint(helpBtn->GetParent()->GetSize().GetWidth() - helpBtn->GetSize().GetWidth(), -1));
			};

			settingsBtn = new wxButton(panel, wxID_ANY, LSTR(MAIN_BTN_SETTINGS));
			settingsHelpBtn = new wxHelpButton(settingsBtn, wxID_ANY, LSTR_LOC(MAIN_BTN_SETTINGS));

			settingsBtn->Bind(wxEVT_SIZE, std::bind(SizeHandler, settingsHelpBtn));
			settingsBtn->Bind(wxEVT_BUTTON, std::bind(&cMain::OnSettingsClicked, this, false));

			storageBtn = new wxButton(panel, wxID_ANY, LSTR(MAIN_BTN_STORAGE));
			storageHelpBtn = new wxHelpButton(storageBtn, wxID_ANY, LSTR_LOC(MAIN_BTN_STORAGE));

			storageBtn->Bind(wxEVT_SIZE, std::bind(SizeHandler, storageHelpBtn));
			storageBtn->Bind(wxEVT_BUTTON, std::bind(&cMain::OnStorageClicked, this));

			verifyBtn = new wxButton(panel, wxID_ANY, LSTR(MAIN_BTN_VERIFY));
			verifyHelpBtn = new wxHelpButton(verifyBtn, wxID_ANY, LSTR_LOC(MAIN_BTN_VERIFY));

			verifyBtn->Bind(wxEVT_SIZE, std::bind(SizeHandler, verifyHelpBtn));
			verifyBtn->Bind(wxEVT_BUTTON, std::bind(&cMain::OnVerifyClicked, this));

			playBtn = new wxButton(panel, wxID_ANY, LSTR(MAIN_BTN_PLAY));
			playHelpBtn = new wxHelpButton(playBtn, wxID_ANY, LSTR_LOC(MAIN_BTN_PLAY));

			playBtn->Bind(wxEVT_SIZE, std::bind(SizeHandler, playHelpBtn));
			playBtn->Bind(wxEVT_BUTTON, std::bind(&cMain::OnPlayClicked, this));

			storageBtn->Disable();
			verifyBtn->Disable();
			playBtn->Disable();
		}

		{
			statusBar = new wxStaticText(panel, wxID_ANY, wxEmptyString);
			selloutBar = new wxStaticText(panel, wxID_ANY, LSTR(MAIN_STATUS_SELLOUT));
		}

		{
			statsBox = new wxStaticBoxSizer(wxVERTICAL, panel, LSTR(MAIN_STATS_TITLE));
			auto statsSizer = new wxBoxSizer(wxHORIZONTAL);
			auto statsSizerL = new wxGridBagSizer(2, 4);
			auto statsSizerR = new wxGridBagSizer(2, 4);
			int statColInd = 0;

			CREATE_STAT(cpu, LSTR(MAIN_STATS_CPU), 1000); // divide by 10 to get %
			CREATE_STAT(ram, LSTR(MAIN_STATS_RAM), 512 * 1024 * 1024); // 512 mb
			CREATE_STAT(read, LSTR(MAIN_STATS_READ), 256 * 1024 * 1024); // 256 mb/s
			CREATE_STAT(write, LSTR(MAIN_STATS_WRITE), 64 * 1024 * 1024); // 64 mb/s
			CREATE_STAT(provide, LSTR(MAIN_STATS_PROVIDE), 256 * 1024 * 1024); // 256 mb/s
			CREATE_STAT(download, LSTR(MAIN_STATS_DOWNLOAD), 64 * 1024 * 1024); // 512 mbps
			CREATE_STAT(latency, LSTR(MAIN_STATS_LATENCY), 1000); // divide by 10 to get ms
			CREATE_STAT(threads, LSTR(MAIN_STATS_THREADS), 192); // 192 threads (threads don't ruin performance, probably just indicates overhead)

			statsSizer->Add(statsSizerL);
			statsSizer->AddStretchSpacer();
			statsSizer->Add(statsSizerR);
			statsBox->Add(statsSizer, 1, wxEXPAND);
		}

		wxBoxSizer* barSizer;
		{
			barSizer = new wxBoxSizer(wxHORIZONTAL);
			barSizer->Add(statusBar);
			barSizer->AddStretchSpacer();
			barSizer->Add(selloutBar);
		}

		wxGridSizer* buttonSizer;
		{
			buttonSizer = new wxGridSizer(2, 2, 3, 3);
			buttonSizer->Add(settingsBtn, 1, wxEXPAND);
			buttonSizer->Add(storageBtn, 1, wxEXPAND);
			buttonSizer->Add(verifyBtn, 1, wxEXPAND);
			buttonSizer->Add(playBtn, 1, wxEXPAND);
		}

		wxBoxSizer* grid;
		{
			grid = new wxBoxSizer(wxVERTICAL);
			grid->Add(buttonSizer, wxSizerFlags(1).Expand());
			grid->Add(statsBox, wxSizerFlags().Expand());
			grid->Add(barSizer, wxSizerFlags().Expand());
		}

		{
			auto topSizer = new wxBoxSizer(wxVERTICAL);
			topSizer->Add(grid, wxSizerFlags(1).Expand().Border(wxALL, 5));
			this->SetSize(wxSize(CMAIN_W, CMAIN_H));
			panel->SetSizerAndFit(topSizer);
			grid->RepositionChildren(wxSize(-1, -1));
		}
	}

	LOG_DEBUG("Getting settings");
	{
		auto settingsFp = fopen(SettingsPath.string().c_str(), "rb");
		if (settingsFp) {
			LOG_DEBUG("Reading settings file");
			SettingsRead(&Settings, settingsFp);
			fclose(settingsFp);
		}
		else {
			LOG_DEBUG("Using default settings");
		}
	}

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

	this->Show();
	this->Restore();
	this->Raise();
	this->SetFocus();

	Stats::StartUpdateThread(ch::milliseconds(1000), [this](StatsUpdateData& data) {
		STAT_VALUE(cpu)->SetValue(1000);
		STAT_VALUE(cpu)->SetValue(std::min(data.cpu * 10, 1000.f));
		STAT_TEXT(cpu)->SetLabel(data.cpu > 0 ? wxString::Format("%.*f%%", std::max(2 - (int)floor(log10(data.cpu)), 1), data.cpu) : "0.00%");

		STAT_VALUE(ram)->SetValue(512 * 1024 * 1024);
		STAT_VALUE(ram)->SetValue(std::min(data.ram, (size_t)512 * 1024 * 1024));
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

		STAT_VALUE(threads)->SetValue(192);
		STAT_VALUE(threads)->SetValue(std::min(data.threads, 192));
		STAT_TEXT(threads)->SetLabel(wxString::Format("%d", data.threads));
		return true;
	});

	Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& evt) {
		this->Hide();
	});

	std::thread([=]() {
		SetStatus(LSTR(MAIN_STATUS_STARTING));
		LOG_DEBUG("Creating game update checker");
		GameUpdater = std::make_unique<GameUpdateChecker>(
			manifestPath,
			[this](const std::string& Url, const std::string& Version) { OnGameUpdate(Version, Url); },
			SettingsGetUpdateInterval(&Settings));
		Mount(GameUpdater->GetLatestUrl());
		LOG_DEBUG("Enabling buttons");
		SetStatus(LSTR(MAIN_STATUS_PLAYABLE));
		storageBtn->Enable();
		verifyBtn->Enable();
		playBtn->Enable();
		LOG_DEBUG("Checking chunk count");
		auto ct = Build->GetMissingChunkCount();
		LOG_DEBUG("%d missing chunks", ct);
		if (ct) {
			OnGameUpdate(GameUpdater->GetLatestVersion());
		}
	}).detach();

	std::thread([=]() {
		LOG_DEBUG("Creating update checker");
		Updater = std::make_unique<UpdateChecker>(
			[this](const UpdateInfo& Info) { OnUpdate(Info); },
			ch::milliseconds(5 * 60 * 1000)); // every 5 minutes, both for ratelimiting purposes and no real reason to be lower
		LOG_DEBUG("Created update checker");
	}).detach();

	Systray = new SystrayIcon(this);
	Systray->SetIcon(wxICON(APP_ICON), "EGL2");
}

cMain::~cMain() {

}

void cMain::OnSettingsClicked(bool onStartup) {
	if (!SetupWnd) {
		SetupWnd = new cSetup(this, &Settings, onStartup, [this](SETTINGS* settings) {
			auto settingsFp = fopen(SettingsPath.string().c_str(), "wb");
			if (settingsFp) {
				SettingsWrite(settings, settingsFp);
				fclose(settingsFp);
			}
			else {
				LOG_ERROR("Could not open settings file to write");
			}
		}, &SettingsValidate, [this]() {
			if (GameUpdater) {
				GameUpdater->SetInterval(SettingsGetUpdateInterval(&Settings));
			}
			SetupWnd.reset();
			this->Raise();
			this->SetFocus();
		});
	}
	else {
		SetupWnd->Restore();
		SetupWnd->Raise();
		SetupWnd->SetFocus();
	}
}

void cMain::OnStorageClicked()
{
	if (StorageWnd) {
		StorageWnd->Restore();
		StorageWnd->Raise();
		StorageWnd->SetFocus();
		return;
	}

	if (UpdateWnd) {
		UpdateWnd->Restore();
		UpdateWnd->Raise();
		UpdateWnd->SetFocus();
		return;
	}

	StorageWnd = new cStorage(this, Build, Settings.ThreadCount, [=] {
		StorageWnd->Destroy();
		StorageWnd.reset();
		this->Raise();
		this->SetFocus();
	});
	StorageWnd->Show(true);
}

#define RUN_PROGRESS(taskName, wndPtr, runAfter, funcName, ...)	\
{															 \
	auto cancelled = new cancel_flag();						 \
	auto onEnd = [=](bool isCancel) {						 \
		wndPtr->Finish();									 \
		wndPtr->Destroy();									 \
		wndPtr.reset();										 \
		runAfter(isCancel);									 \
		delete cancelled;									 \
		this->Raise();										 \
		this->SetFocus();									 \
	};														 \
	wndPtr = new cProgress(this, taskName, 					 \
		*cancelled, [=](){ onEnd(true); });					 \
	wndPtr->Show(true);										 \
														 	 \
	std::thread([=]() {										 \
		Build->##funcName(									 \
			[=](uint32_t m) {								 \
				if (wndPtr && !cancelled->cancelled()) {	 \
					wndPtr->SetMaximum(m);					 \
				}											 \
			},												 \
			[=]() {											 \
				if (wndPtr && !cancelled->cancelled()) {	 \
					wndPtr->Increment();					 \
				}											 \
			},												 \
			[=]() {											 \
				if (wndPtr && !cancelled->cancelled()) {	 \
					onEnd(false);							 \
				}											 \
			},												 \
			*cancelled, __VA_ARGS__);						 \
															 \
	}).detach();											 \
}

void cMain::OnVerifyClicked() {
	if (VerifyWnd) {
		VerifyWnd->Restore();
		VerifyWnd->Raise();
		VerifyWnd->SetFocus();
		return;
	}

	RUN_PROGRESS(LSTR(MAIN_PROG_VERIFY), VerifyWnd, [](bool cancelled) { }, VerifyAllChunks, Settings.ThreadCount);
}

void cMain::OnPlayClicked() {
	if (GameUpdateAvailable) {
		BeginGameUpdate();
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

void cMain::Mount(const std::string& Url) {
	LOG_INFO("Setting up cache directory");
	MountedBuild::SetupCacheDirectory(Settings.CacheDir);
	LOG_INFO("Mounting new url: %s", Url.c_str());
	Build.reset(new MountedBuild(GameUpdater->GetManifest(Url), fs::path(Settings.CacheDir) / MOUNT_FOLDER, Settings.CacheDir, SettingsGetStorageFlags(&Settings), Settings.BufferCount));
	LOG_INFO("Setting up game dir");
	Build->SetupGameDirectory([](unsigned int m) {}, []() {}, []() {}, cancel_flag(), Settings.ThreadCount);
}

void cMain::OnGameUpdate(const std::string& Version, const std::optional<std::string>& Url)
{
	GameUpdateAvailable = true;
	playBtn->SetLabel(LSTR(MAIN_BTN_UPDATE));
	if (Url.has_value()) {
		GameUpdateUrl = Url;
	}

	CallAfter([=]() {
		auto notif = new wxGenericNotificationMessage(LSTR(MAIN_NOTIF_TITLE), wxString::Format(LSTR(MAIN_NOTIF_DESC), GameUpdateChecker::GetReadableVersion(Version)), this);
		notif->SetIcon(wxICON(APP_ICON));
		if (!notif->AddAction(42, LSTR(MAIN_NOTIF_ACTION))) {
			LOG_WARN("Actions aren't supported");
		}
		notif->Bind(wxEVT_NOTIFICATION_MESSAGE_ACTION, std::bind(&cMain::BeginGameUpdate, this));
		notif->Bind(wxEVT_NOTIFICATION_MESSAGE_CLICK, std::bind(&cMain::BeginGameUpdate, this));
		if (!notif->Show(wxNotificationMessage::Timeout_Never)) {
			LOG_ERROR("Couldn't launch game update notification");
		}
	});
}

void cMain::BeginGameUpdate()
{
	if (UpdateWnd) {
		UpdateWnd->Restore();
		UpdateWnd->Raise();
		UpdateWnd->SetFocus();
		return;
	}

	if (!GameUpdateAvailable) {
		return;
	}

	if (StorageWnd) {
		StorageWnd->Close(true);
	}

	std::thread([this]() {
		if (!GameUpdateUrl.has_value()) {
			LOG_DEBUG("Downloading unavailable chunks");
		}
		else {
			LOG_DEBUG("Beginning update: %s", GameUpdateUrl->c_str());
			Mount(*GameUpdateUrl);
		}

		this->CallAfter([this]() {
			RUN_PROGRESS(LSTR(MAIN_PROG_UPDATE), UpdateWnd, [this](bool cancelled) {
				LOG_DEBUG("EXIT NOTICED");
				if (!cancelled) {
					GameUpdateAvailable = false;
					playBtn->SetLabel(LSTR(MAIN_BTN_PLAY));
					GameUpdateUrl.reset();
				}
			}, PreloadAllChunks, Settings.ThreadCount);
		});
	}).detach();
}

void cMain::OnUpdate(const UpdateInfo& Info)
{
	UpdateUrl = Info.Url;

	CallAfter([=]() {
		auto notif = new wxGenericNotificationMessage("New EGL2 Update!", wxString::Format("%s is now available!\n%s, %d downloads", Info.Version, Stats::GetReadableSize(Info.DownloadSize), Info.DownloadCount), this);
		notif->SetIcon(wxICON(APP_ICON));
		if (!notif->AddAction(42, LSTR(MAIN_NOTIF_ACTION))) {
			LOG_WARN("Actions aren't supported");
		}
		notif->Bind(wxEVT_NOTIFICATION_MESSAGE_ACTION, std::bind(&cMain::BeginUpdate, this));
		notif->Bind(wxEVT_NOTIFICATION_MESSAGE_CLICK, std::bind(&cMain::BeginUpdate, this));
		if (!notif->Show(wxNotificationMessage::Timeout_Never)) {
			LOG_ERROR("Couldn't launch update notification");
		}
	});
}

void cMain::BeginUpdate()
{
	if (UpdateUrl.empty()) {
		return;
	}
	wxLaunchDefaultBrowser(UpdateUrl);
	UpdateUrl.clear();
}