#include "cMain.h"
#include "cSetup.h"
#include "cProgress.h"
#include "cAuth.h"

#include <wx/gbsizer.h>
#include <wx/windowptr.h>
#include <thread>
#include <atomic>

#define DESC_TEXT_DEFAULT "Hover over a button to see what it does"
#define DESC_TEXT_SETUP   "Click this before running for the first time. Has options for setting up your installation."
#define DESC_TEXT_VERIFY  "If you believe some chunks are invalid, click this to verify all chunks that are already downloaded. Redownloads any invalid chunks."
#define DESC_TEXT_PURGE   "Deletes any chunks that aren't used anymore. Useful for slimming down your install after you've updated your game."
#define DESC_TEXT_PRELOAD "Downloads any chunks that you don't already have downloaded for the most recent version, a.k.a updating. It's reccomended to update your install before playing in case you have a hotfix available."
#define DESC_TEXT_START   "Mount your install to a drive letter, and, if selected, move the files necessary to a folder in order to start your game."
#define DESC_TEXT_PLAY    "When clicked, it will prompt you to provide your exchange code. After giving it, the game will launch and you'll be good to go." \
						  "\n\nThis option is only available when playing is enabled."

#define STATUS_NEED_SETUP "Setup where you want your game to be installed."
#define STATUS_NORMAL     "Click start to mount."
#define STATUS_PLAYABLE   "Started! Press \"Play\" to start playing!"
#define STATUS_UNPLAYABLE "Started! If you want to play, enable it in your setup!"

#define LAUNCH_GAME_ARGS  "-AUTH_LOGIN=unused AUTH_TYPE=exchangecode -epicapp=Fortnite -epicenv=Prod -epicportal -epiclocale=en-us -AUTH_PASSWORD=%s"

#define BIND_BUTTON_DESC(btn, desc) \
	btn->Bind(wxEVT_MOTION, std::bind(&cMain::OnButtonHover, this, desc)); \
	btn->Bind(wxEVT_LEAVE_WINDOW, std::bind(&cMain::OnButtonHover, this, DESC_TEXT_DEFAULT));

cMain::cMain(fs::path settingsPath, fs::path manifestPath) : wxFrame(nullptr, wxID_ANY, "EGL2", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE ^ (wxMAXIMIZE_BOX | wxRESIZE_BORDER)) {
	this->SetIcon(wxICON(APP_ICON));
	this->SetMinSize(wxSize(450, 250));
	this->SetMaxSize(wxSize(450, 250));

	panel = new wxPanel(this, wxID_ANY,
		wxDefaultPosition,
		wxDefaultSize,
		wxTAB_TRAVERSAL);

	auto grid = new wxGridBagSizer();

	setupBtn = new wxButton(panel, wxID_ANY, "Setup");
	verifyBtn = new wxButton(panel, wxID_ANY, "Verify");
	purgeBtn = new wxButton(panel, wxID_ANY, "Purge");
	preloadBtn = new wxButton(panel, wxID_ANY, "Update");
	startBtn = new wxButton(panel, wxID_ANY, "Start");
	startFnBtn = new wxButton(panel, wxID_ANY, "Play");
	statusBar = new wxStaticText(panel, wxID_ANY, wxEmptyString);
	selloutBar = new wxStaticText(panel, wxID_ANY, "Use code \"furry\"! (#ad)");

	setupBtn->Bind(wxEVT_BUTTON, std::bind(&cMain::OnSetupClicked, this));
	verifyBtn->Bind(wxEVT_BUTTON, std::bind(&cMain::OnVerifyClicked, this));
	purgeBtn->Bind(wxEVT_BUTTON, std::bind(&cMain::OnPurgeClicked, this));
	preloadBtn->Bind(wxEVT_BUTTON, std::bind(&cMain::OnPreloadClicked, this));
	startBtn->Bind(wxEVT_BUTTON, std::bind(&cMain::OnStartClicked, this));
	startFnBtn->Bind(wxEVT_BUTTON, std::bind(&cMain::OnPlayClicked, this));
	startFnBtn->Disable();

	BIND_BUTTON_DESC(setupBtn, DESC_TEXT_SETUP);
	BIND_BUTTON_DESC(verifyBtn, DESC_TEXT_VERIFY);
	BIND_BUTTON_DESC(purgeBtn, DESC_TEXT_PURGE);
	BIND_BUTTON_DESC(preloadBtn, DESC_TEXT_PRELOAD);
	BIND_BUTTON_DESC(startBtn, DESC_TEXT_START);
	BIND_BUTTON_DESC(startFnBtn, DESC_TEXT_PLAY);

	descBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Description");
	descTxt = new wxStaticText(panel, wxID_ANY, DESC_TEXT_DEFAULT);
	descBox->Add(descTxt, 1, wxEXPAND);

	auto barSizer = new wxBoxSizer(wxHORIZONTAL);
	barSizer->Add(statusBar);
	barSizer->AddStretchSpacer();
	barSizer->Add(selloutBar);

	grid->Add(setupBtn, wxGBPosition(0, 0), wxGBSpan(1, 2), wxEXPAND);
	grid->Add(verifyBtn, wxGBPosition(1, 0), wxGBSpan(1, 2), wxEXPAND);
	grid->Add(purgeBtn, wxGBPosition(2, 0), wxGBSpan(1, 2), wxEXPAND);
	grid->Add(preloadBtn, wxGBPosition(3, 0), wxGBSpan(1, 2), wxEXPAND);
	grid->Add(startBtn, wxGBPosition(4, 0), wxGBSpan(1, 1), wxEXPAND);
	grid->Add(startFnBtn, wxGBPosition(4, 1), wxGBSpan(1, 1), wxEXPAND);
	grid->Add(descBox, wxGBPosition(0, 2), wxGBSpan(5, 1), wxEXPAND);
	grid->Add(barSizer, wxGBPosition(5, 0), wxGBSpan(1, 3), wxEXPAND);

	grid->AddGrowableCol(0, 1);
	grid->AddGrowableCol(1, 1);
	grid->AddGrowableCol(2, 4);

	grid->AddGrowableRow(0);
	grid->AddGrowableRow(1);
	grid->AddGrowableRow(2);
	grid->AddGrowableRow(3);
	grid->AddGrowableRow(4);

	auto topSizer = new wxBoxSizer(wxVERTICAL);
	topSizer->Add(grid, wxSizerFlags(1).Expand().Border(wxALL, 5));
	panel->SetSizerAndFit(topSizer);
	this->SetSize(wxSize(450, 250));

	Bind(wxEVT_CLOSE_WINDOW, &cMain::OnClose, this);

	SettingsPath = settingsPath;
	memset(&Settings, 0, sizeof(Settings));
	auto settingsFp = fopen(SettingsPath.string().c_str(), "rb");
	if (settingsFp) {
		SettingsRead(&Settings, settingsFp);
		fclose(settingsFp);
	}
	else {
		Settings.MountDrive = '\0';
		Settings.CompressionLevel = 4; // Slowest
		Settings.CompressionMethod = 1; // Decompress
		Settings.EnableGaming = true;
		Settings.VerifyCache = true;
	}

	{
		auto valid = SettingsValidate(&Settings);
		verifyBtn->Enable(valid);
		purgeBtn->Enable(valid);
		preloadBtn->Enable(valid);
		startBtn->Enable(valid);
		SetStatus(valid ? STATUS_NORMAL : STATUS_NEED_SETUP);
	}

	ManifestAuthGrab(&ManifestAuth);
	ManifestAuthGetManifest(ManifestAuth, manifestPath, &Manifest);
}

cMain::~cMain() {
	auto settingsFp = fopen(SettingsPath.string().c_str(), "wb");
	SettingsWrite(&Settings, settingsFp);
	fclose(settingsFp);
}

void cMain::OnButtonHover(const char* string) {
	if (strcmp(descTxt->GetLabel().c_str(), string)) {
		descTxt->SetLabel(string);
		descBox->Fit(descTxt);
		descBox->FitInside(descTxt);
		descBox->Layout();
	}
}

void cMain::OnSetupClicked() {
	cSetup(this, &Settings).ShowModal();
	auto valid = SettingsValidate(&Settings);
	verifyBtn->Enable(valid);
	purgeBtn->Enable(valid);
	preloadBtn->Enable(valid);
	startBtn->Enable(valid);
	SetStatus(valid ? STATUS_NORMAL : STATUS_NEED_SETUP);
	if (valid) {
		auto& build = GetMountedBuild();
		build->SetupCacheDirectory();
	}
}

#define RUN_PROGRESS(taskName, funcName, ...)				 \
{															 \
	auto cancelled = new cancel_flag();						 \
	cProgress* progress = new cProgress(this, taskName,		 \
		*cancelled);										 \
	progress->Show(true);									 \
														 	 \
	wxWindowPtr progressPtr(progress);				 		 \
	std::thread([=]() {										 \
		auto& b = GetMountedBuild();						 \
															 \
		b->##funcName(										 \
			[=](uint32_t m) { progressPtr->SetMaximum(m); }, \
			[=]() { progressPtr->Increment(); },			 \
			*cancelled, __VA_ARGS__);						 \
															 \
		progressPtr->Finish();								 \
		progressPtr->Close();								 \
		delete cancelled;									 \
	}).detach();											 \
}

void cMain::OnVerifyClicked() {
	RUN_PROGRESS("Verifying", VerifyAllChunks, 64);
}

void cMain::OnPurgeClicked() {
	RUN_PROGRESS("Purging", PurgeUnusedChunks);
}

void cMain::OnPreloadClicked() {
	RUN_PROGRESS("Updating", PreloadAllChunks, 64);
}

void cMain::OnStartClicked() {
	auto& build = GetMountedBuild();
	if (build->Mounted()) {
		build->Unmount();
		setupBtn->Enable();
		startFnBtn->Disable();
		startBtn->SetLabel("Start");
		SetStatus(STATUS_NORMAL);
	}
	else {
		if (build->Mount()) {
			if (Settings.EnableGaming) {
				RUN_PROGRESS("Setting Up", SetupGameDirectory, 64, Settings.GameDir);
			}

			setupBtn->Disable();
			startFnBtn->Enable(Settings.EnableGaming);
			startBtn->SetLabel("Stop");
			SetStatus(Settings.EnableGaming ? STATUS_PLAYABLE : STATUS_UNPLAYABLE);
		}
	}
}

void cMain::OnPlayClicked() {
	cAuth auth(this);
	auth.ShowModal();
	if (!auth.GetCode().IsEmpty()) {
		auto& build = GetMountedBuild();
		build->LaunchGame(Settings.GameDir, wxString::Format(LAUNCH_GAME_ARGS, auth.GetCode()).c_str());
	}
}

void cMain::OnClose(wxCloseEvent& evt)
{
	if (evt.CanVeto() && Build && Build->Mounted()) {
		if (wxMessageBox("Your game is currently mounted. Do you want to exit now?", "Currently Mounted - EGL2", wxICON_QUESTION | wxYES_NO) != wxYES)
		{
			evt.Veto();
			return;
		}
	}

	Destroy();
}

void cMain::SetStatus(const char* string) {
	statusBar->SetLabel(string);
}

std::unique_ptr<MountedBuild>& cMain::GetMountedBuild() {
	if (!Build) {
		Build = std::make_unique<MountedBuild>(Manifest, std::string(1, Settings.MountDrive) + ':', Settings.CacheDir, [](const char* error) {});
		Build->StartStorage(SettingsGetStorageFlags(&Settings));
	}
	return Build;
}