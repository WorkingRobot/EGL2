#include "cSetup.h"

#include "Localization.h"
#include "settings.h"
#include "wxLabelSlider.h"

#include <wx/filepicker.h>
#include <wx/gbsizer.h>
#include <wx/slider.h>

static const char* compMethods[] = {
	"Zstandard (Not tested)",
	"LZ4 (Recommended)",
	"Decompressed"
};

static const char* compLevels[] = {
	"Fastest",
	"Fast",
	"Normal",
	"Slow",
	"Slowest"
};

static const char* updateLevels[] = {
	"1 second",
	"5 seconds",
	"10 seconds",
	"30 seconds",
	"1 minute",
	"5 minutes",
	"10 minutes",
	"30 minutes",
	"1 hour",
};

#define DEFINE_SECTION(name, displayName) \
	auto sectionBox##name = new wxStaticBoxSizer(wxVERTICAL, panel, displayName); \
	auto sectionGrid##name = new wxGridBagSizer(2, 2); \
	auto sectionColInd##name = 0; \
	sectionGrid##name->Add(5, 1, wxGBPosition(0, 1)); \
	sectionBox##name->Add(sectionGrid##name, wxSizerFlags().Expand().Border(wxALL, 5));

#define ADD_ITEM_BROWSE(section, name, displayName, binder) \
	auto sectionLabel##name = new wxStaticText(panel, wxID_ANY, displayName); \
	auto sectionValue##name = new wxDirPickerCtrl(panel, wxID_ANY); \
	ReadBinds.emplace_back([sectionValue##name](SETTINGS* val) { \
		sectionValue##name->SetPath(val->##binder); \
	}); \
	WriteBinds.emplace_back([sectionValue##name](SETTINGS* val) { \
		strcpy_s(val->##binder, sectionValue##name->GetPath().c_str()); \
	}); \
	sectionGrid##section->Add(sectionLabel##name, wxGBPosition(sectionColInd##section, 0), wxGBSpan(1, 1), wxEXPAND); \
	sectionGrid##section->Add(sectionValue##name, wxGBPosition(sectionColInd##section, 2), wxGBSpan(1, 1), wxEXPAND); \
	sectionColInd##section++;

#define ADD_ITEM_CHOICE(section, name, displayName, choices, under_type, binder) \
	auto sectionLabel##name = new wxStaticText(panel, wxID_ANY, displayName); \
	auto sectionValue##name = new wxChoice(panel, wxID_ANY); \
	ReadBinds.emplace_back([sectionValue##name](SETTINGS* val) { \
		sectionValue##name->SetSelection((int)val->##binder); \
	}); \
	WriteBinds.emplace_back([sectionValue##name](SETTINGS* val) { \
		val->##binder = (under_type)sectionValue##name->GetSelection(); \
	}); \
	sectionValue##name->Append(choices); \
	sectionGrid##section->Add(sectionLabel##name, wxGBPosition(sectionColInd##section, 0), wxGBSpan(1, 1), wxEXPAND); \
	sectionGrid##section->Add(sectionValue##name, wxGBPosition(sectionColInd##section, 2), wxGBSpan(1, 1), wxEXPAND); \
	sectionColInd##section++;

#define ADD_ITEM_TEXTSLIDER(section, name, displayName, choices, under_type, binder) \
	auto sectionLabel##name = new wxStaticText(panel, wxID_ANY, displayName); \
	auto sectionValue##name = new wxLabelSlider(panel, wxID_ANY, 0, choices, wxSL_HORIZONTAL | wxSL_AUTOTICKS); \
	ReadBinds.emplace_back([sectionValue##name](SETTINGS* val) { \
		sectionValue##name->SetValue((int)val->##binder); \
	}); \
	WriteBinds.emplace_back([sectionValue##name](SETTINGS* val) { \
		val->##binder = (under_type)sectionValue##name->GetValue(); \
	}); \
	sectionGrid##section->Add(sectionLabel##name, wxGBPosition(sectionColInd##section, 0), wxGBSpan(1, 1), wxEXPAND); \
	sectionGrid##section->Add(sectionValue##name, wxGBPosition(sectionColInd##section, 2), wxGBSpan(1, 1), wxEXPAND); \
	sectionColInd##section++;

#define ADD_ITEM_SLIDER(section, name, displayName, minValue, maxValue, under_type, binder) \
	auto sectionLabel##name = new wxStaticText(panel, wxID_ANY, displayName); \
	auto sectionValue##name = new wxSlider(panel, wxID_ANY, minValue, minValue, maxValue, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS | wxSL_AUTOTICKS); \
	ReadBinds.emplace_back([sectionValue##name](SETTINGS* val) { \
		sectionValue##name->SetValue((int)val->##binder); \
	}); \
	WriteBinds.emplace_back([sectionValue##name](SETTINGS* val) { \
		val->##binder = (under_type)sectionValue##name->GetValue(); \
	}); \
	sectionGrid##section->Add(sectionLabel##name, wxGBPosition(sectionColInd##section, 0), wxGBSpan(1, 1), wxEXPAND); \
	sectionGrid##section->Add(sectionValue##name, wxGBPosition(sectionColInd##section, 2), wxGBSpan(1, 1), wxEXPAND); \
	sectionColInd##section++;

#define ADD_ITEM_TEXT(section, name, displayName, binder) \
	auto sectionLabel##name = new wxStaticText(panel, wxID_ANY, displayName); \
	auto sectionValue##name = new wxTextCtrl(panel, wxID_ANY); \
	ReadBinds.emplace_back([sectionValue##name](SETTINGS* val) { \
		sectionValue##name->SetValue(val->##binder); \
	}); \
	WriteBinds.emplace_back([sectionValue##name](SETTINGS* val) { \
		strcpy_s(val->##binder, sectionValue##name->GetValue().c_str()); \
	}); \
	sectionGrid##section->Add(sectionLabel##name, wxGBPosition(sectionColInd##section, 0), wxGBSpan(1, 1), wxEXPAND); \
	sectionGrid##section->Add(sectionValue##name, wxGBPosition(sectionColInd##section, 2), wxGBSpan(1, 1), wxEXPAND); \
	sectionColInd##section++;

#define APPEND_SECTION_FIRST(name) \
	sectionGrid##name->AddGrowableCol(2); \
	mainSizer->Add(sectionBox##name, wxSizerFlags().Expand().Border(wxUP | wxRIGHT | wxLEFT, 10));
#define APPEND_SECTION(name) \
	mainSizer->AddSpacer(5); \
	sectionGrid##name->AddGrowableCol(2); \
	mainSizer->Add(sectionBox##name, wxSizerFlags().Expand().Border(wxRIGHT | wxLEFT, 10));

template <typename... Params>
inline const wxArrayString GetChoices(Params... params) {
	return wxArrayStringsAdapter(std::vector<wxString> { params... }).AsArrayString();
}

cSetup::cSetup(cMain* main, SETTINGS* settings, bool startupInvalid, cSetup::flush_callback callback, cSetup::validate_callback validator, cSetup::exit_callback onExit) : wxFrame(main, wxID_ANY, LTITLE(LSTR(SETUP_TITLE)), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE ^ (wxMAXIMIZE_BOX | wxRESIZE_BORDER)),
	Settings(settings),
	OldSettings(*settings),
	InvalidStartup(startupInvalid),
	Callback(callback),
	Validator(validator),
	OnExit(onExit) {
	this->SetIcon(wxICON(APP_ICON));
	this->SetMinSize(wxSize(500, -1));
	this->SetMaxSize(wxSize(500, -1));

	auto panel = new wxPanel(this, wxID_ANY,
		wxDefaultPosition,
		wxDefaultSize,
		wxTAB_TRAVERSAL);

	auto mainSizer = new wxBoxSizer(wxVERTICAL);

	DEFINE_SECTION(general, LSTR(SETUP_GENERAL_LABEL));
	ADD_ITEM_BROWSE(general, cacheDir, LSTR(SETUP_GENERAL_INSTFOLDER), CacheDir);
	ADD_ITEM_CHOICE(general, compMethod, LSTR(SETUP_GENERAL_COMPMETHOD),
		GetChoices(
			LSTR(SETUP_COMP_METHOD_ZSTD),
			LSTR(SETUP_COMP_METHOD_LZ4),
			LSTR(SETUP_COMP_METHOD_DECOMP)
		),
		SettingsCompressionMethod, CompressionMethod);
	ADD_ITEM_TEXTSLIDER(general, compLevel, LSTR(SETUP_GENERAL_COMPLEVEL),
		GetChoices(
			LSTR(SETUP_COMP_LEVEL_FASTEST),
			LSTR(SETUP_COMP_LEVEL_FAST),
			LSTR(SETUP_COMP_LEVEL_NORMAL),
			LSTR(SETUP_COMP_LEVEL_SLOW),
			LSTR(SETUP_COMP_LEVEL_SLOWEST)
		), SettingsCompressionLevel, CompressionLevel);
	ADD_ITEM_TEXTSLIDER(general, updateInt, LSTR(SETUP_GENERAL_UPDATEINT),
		GetChoices(
			LSTR(SETUP_UPDATE_LEVEL_SEC1),
			LSTR(SETUP_UPDATE_LEVEL_SEC5),
			LSTR(SETUP_UPDATE_LEVEL_SEC10),
			LSTR(SETUP_UPDATE_LEVEL_SEC30),
			LSTR(SETUP_UPDATE_LEVEL_MIN1),
			LSTR(SETUP_UPDATE_LEVEL_MIN5),
			LSTR(SETUP_UPDATE_LEVEL_MIN10),
			LSTR(SETUP_UPDATE_LEVEL_MIN30),
			LSTR(SETUP_UPDATE_LEVEL_HOUR1)
		), SettingsUpdateInterval, UpdateInterval);

	DEFINE_SECTION(advanced, LSTR(SETUP_ADVANCED_LABEL));
	ADD_ITEM_SLIDER(advanced, bufCount, LSTR(SETUP_ADVANCED_BUFCT), 1, 512, uint16_t, BufferCount);
	ADD_ITEM_SLIDER(advanced, threadCount, LSTR(SETUP_ADVANCED_THDCT), 1, 128, uint16_t, ThreadCount);
	ADD_ITEM_TEXT(advanced, cmdArgs, LSTR(SETUP_ADVANCED_CMDARGS), CommandArgs);

	APPEND_SECTION_FIRST(general);
	APPEND_SECTION(advanced);

	auto applySettingsPanel = new wxBoxSizer(wxHORIZONTAL);

	auto okBtn = new wxButton(panel, wxID_ANY, LSTR(SETUP_BTN_OK));
	auto cancelBtn = new wxButton(panel, wxID_ANY, LSTR(SETUP_BTN_CANCEL));
	okBtn->Bind(wxEVT_BUTTON, std::bind(&cSetup::OkClicked, this));
	cancelBtn->Bind(wxEVT_BUTTON, std::bind(&cSetup::CancelClicked, this));
	applySettingsPanel->Add(okBtn, wxSizerFlags().Border(wxRIGHT, 5));
	applySettingsPanel->Add(cancelBtn);

	mainSizer->AddSpacer(5);
	mainSizer->Add(applySettingsPanel, wxSizerFlags().Right().Border(wxDOWN | wxRIGHT, 10));

	panel->SetSizerAndFit(mainSizer);
	this->Fit();
	this->Show();

	Disabler = new wxWindowDisabler(this);

	wxToolTip::SetAutoPop(10000);

	ReadConfig();

	Bind(wxEVT_CLOSE_WINDOW, &cSetup::CloseClicked, this);
}

cSetup::~cSetup() {
	delete Disabler;
}

void cSetup::ReadConfig() {
	for (auto& bind : ReadBinds) {
		bind(Settings);
	}
}

void cSetup::WriteConfig() {
	for (auto& bind : WriteBinds) {
		bind(Settings);
	}
}

void cSetup::OkClicked()
{
	WriteConfig();
	if (Validator(Settings)) { // New values are valid
		Callback(Settings);
		OnExit();
		this->Destroy();
	}
}

void cSetup::CancelClicked()
{
	if (InvalidStartup) {
		OnExit();
		this->Destroy();
	}
	else if (Validator(&OldSettings)) { // Old values are valid, it's fine to throw out the current values
		*Settings = OldSettings;
		OnExit();
		this->Destroy();
	}
}

void cSetup::ApplyClicked()
{
	WriteConfig();
	if (Validator(Settings)) { // New values are valid
		Callback(Settings);
	}
}

void cSetup::CloseClicked(wxCloseEvent& evt)
{
	if (Validator(&OldSettings)) { // Old values are valid, it's fine to throw out the current values
		*Settings = OldSettings;
		OnExit();
		this->Destroy();
	}
	else {
		if (!InvalidStartup) {
			evt.Veto();
		}
		else {
			OnExit();
			this->Destroy();
		}
	}
}