#include "cSetup.h"

#include <wx/gbsizer.h>

#include "settings.h"

static const char* compMethods[] = {
	"Keep Compressed as Downloaded (zLib)",
	"Decompress",
	"Use LZ4",
	"Use zLib"
};

static const char* compLevels[] = {
	"Fastest",
	"Fast",
	"Normal",
	"Slow",
	"Slowest"
};

#define VERIFY_TOOLTIP	"Verify data that is read from the cache and redownload\n" \
						"it if the data is invalid.\n" \
						"Note: You may take a small performance hit."

#define GAME_TOOLTIP	"In order to launch the game, a workaround must be done\n" \
						"where all binaries are copied to a physical drive in order to\n" \
						"prevent the anticheat from getting grumpy.\n" \
						"Note: Depending on the install, an additional 300-400MB\n" \
						"of data will need to be allocated on your hard drive."

#define DRIVE_ALPHABET  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

cSetup::cSetup(cMain* main, SETTINGS* settings) : wxModalWindow(main, wxID_ANY, "Setup - EGL2", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE ^ (wxMAXIMIZE_BOX | wxRESIZE_BORDER)) {
	Settings = settings;

	this->SetIcon(wxICON(APP_ICON));
	this->SetMinSize(wxSize(500, -1));
	this->SetMaxSize(wxSize(500, -1));

	wxPanel* panel = new wxPanel(this, wxID_ANY,
		wxDefaultPosition,
		wxDefaultSize,
		wxTAB_TRAVERSAL);

	settingsContainer = new wxWindow(panel, wxID_ANY);
	checkboxContainer = new wxWindow(panel, wxID_ANY);

	auto settingsGrid = new wxGridBagSizer(2, 2);

	cacheDirTxt = new wxStaticText(settingsContainer, wxID_ANY, "Cache Folder");
	cacheDirValue = new wxDirPickerCtrl(settingsContainer, wxID_ANY);

	wxArrayString drives;
	auto usedDrives = GetLogicalDrives();
	for (int b = 0; b < strlen(DRIVE_ALPHABET); ++b) {
		if (!((usedDrives >> b) & 1)) {
			drives.push_back(wxString(DRIVE_ALPHABET[b]) + ':');
		}
	}

	mountDirTxt = new wxStaticText(settingsContainer, wxID_ANY, "Mount Drive");
	mountDirValue = new wxChoice(settingsContainer, wxID_ANY, wxDefaultPosition, wxDefaultSize, drives);

	gameDirTxt = new wxStaticText(settingsContainer, wxID_ANY, "Game Folder");
	gameDirValue = new wxDirPickerCtrl(settingsContainer, wxID_ANY);

	compMethodTxt = new wxStaticText(settingsContainer, wxID_ANY, "Compression Method");
	compMethodValue = new wxChoice(settingsContainer, wxID_ANY);
	compMethodValue->Append(wxArrayString(_countof(compMethods), compMethods));

	compLevelTxt = new wxStaticText(settingsContainer, wxID_ANY, "Compression Level");
	compLevelValue = new wxChoice(settingsContainer, wxID_ANY);
	compLevelValue->Append(wxArrayString(_countof(compLevels), compLevels));


	verifyCacheCheckbox = new wxCheckBox(checkboxContainer, wxID_ANY, "Verify cache when read from");
	verifyCacheCheckbox->SetToolTip(VERIFY_TOOLTIP);

	gameDirCheckbox = new wxCheckBox(checkboxContainer, wxID_ANY, "Enable playing of game");
	gameDirCheckbox->SetToolTip(GAME_TOOLTIP);

	settingsGrid->Add(cacheDirTxt, wxGBPosition(0, 0), wxGBSpan(1, 1), wxEXPAND);
	settingsGrid->Add(cacheDirValue, wxGBPosition(0, 2), wxGBSpan(1, 1), wxEXPAND);

	settingsGrid->Add(mountDirTxt, wxGBPosition(1, 0), wxGBSpan(1, 1), wxEXPAND);
	settingsGrid->Add(mountDirValue, wxGBPosition(1, 2), wxGBSpan(1, 1), wxEXPAND);

	settingsGrid->Add(gameDirTxt, wxGBPosition(2, 0), wxGBSpan(1, 1), wxEXPAND);
	settingsGrid->Add(gameDirValue, wxGBPosition(2, 2), wxGBSpan(1, 1), wxEXPAND);

	settingsGrid->Add(compMethodTxt, wxGBPosition(3, 0), wxGBSpan(1, 1), wxEXPAND);
	settingsGrid->Add(compLevelTxt, wxGBPosition(4, 0), wxGBSpan(1, 1), wxEXPAND);
	settingsGrid->Add(compMethodValue, wxGBPosition(3, 2), wxGBSpan(1, 1), wxEXPAND);
	settingsGrid->Add(compLevelValue, wxGBPosition(4, 2), wxGBSpan(1, 1), wxEXPAND);

	settingsGrid->AddGrowableCol(2);
	settingsGrid->Add(5, 1, wxGBPosition(0, 1));

	settingsContainer->SetSizerAndFit(settingsGrid);

	auto checkboxSizer = new wxBoxSizer(wxHORIZONTAL);

	checkboxSizer->Add(verifyCacheCheckbox, 1);
	checkboxSizer->Add(gameDirCheckbox, 1);

	checkboxContainer->SetSizerAndFit(checkboxSizer);

	auto sizer = new wxBoxSizer(wxVERTICAL);

	sizer->Add(settingsContainer, wxSizerFlags().Expand().Border(wxUP | wxRIGHT | wxLEFT, 10));
	sizer->AddSpacer(10);
	sizer->Add(checkboxContainer, wxSizerFlags().Expand().Border(wxDOWN | wxRIGHT | wxLEFT, 10));

	panel->SetSizerAndFit(sizer);
	this->Fit();

	wxToolTip::SetAutoPop(15000);

	ReadConfig();
	if (Settings->MountDrive == '\0') {
		mountDirValue->SetSelection(0);
	}
	// disable when first 2 options are selected
	compLevelValue->Enable(compMethodValue->GetSelection() >= 2);
	compMethodValue->Bind(wxEVT_CHOICE, [this](wxCommandEvent& evt) {
		compLevelValue->Enable(compMethodValue->GetSelection() >= 2);
	});
	Bind(wxEVT_CLOSE_WINDOW, std::bind(&cSetup::WriteConfig, this));
}

cSetup::~cSetup() {

}

void cSetup::ReadConfig() {
	cacheDirValue->SetPath(Settings->CacheDir);
	mountDirValue->SetStringSelection(wxString(Settings->MountDrive) + ':');
	gameDirValue->SetPath(Settings->GameDir);

	compMethodValue->SetSelection(Settings->CompressionMethod);
	compLevelValue->SetSelection(Settings->CompressionLevel);

	verifyCacheCheckbox->SetValue(Settings->VerifyCache);
	gameDirCheckbox->SetValue(Settings->EnableGaming);
}

void cSetup::WriteConfig() {
	strcpy_s(Settings->CacheDir, cacheDirValue->GetPath().c_str());
	Settings->MountDrive = mountDirValue->GetStringSelection()[0];
	strcpy_s(Settings->GameDir, gameDirValue->GetPath().c_str());

	Settings->CompressionMethod = compMethodValue->GetSelection();
	Settings->CompressionLevel = compLevelValue->GetSelection();

	Settings->VerifyCache = verifyCacheCheckbox->IsChecked();
	Settings->EnableGaming = gameDirCheckbox->IsChecked();

	this->Destroy();
}