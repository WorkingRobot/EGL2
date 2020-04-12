#include "cAuth.h"

#define EXCHANGE_INFO	"This exchange code is used to log you into Fortnite. Without it, the "  \
						"game doesn't know you. To get this code, press the get code button "    \
						"above. Copy the giant blob of numbers and letters, not including the "  \
						"quotes. Paste it into the text box above and click launch.\n "\
						"Example exchange code: d9c230c0e0354a619249ba1156df5e63"

#define EXCHANGE_SEC    "This code expires 5 minutes after visiting the URL and can only be "	 \
						"used once. Anyone with this code can impersonate you and take control " \
						"of your account. When using EGL2, this code is passed directly to the " \
						"game and is not read, used, stored, or modified for any purpose other " \
						"than to launch the game."

#define EXCHANGE_URL  "https://www.epicgames.com/id/login?redirectUrl=https%3A%2F%2Fwww.epicgames.com%2Fid%2Fapi%2Fexchange"

#include <wx/gbsizer.h>

cAuth::cAuth(cMain* main) : wxModalWindow(main, wxID_ANY, "Launch Game - EGL2", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE ^ (wxMAXIMIZE_BOX | wxRESIZE_BORDER)) {
	this->SetIcon(wxICON(APP_ICON));
	this->SetMinSize(wxSize(400, 330));
	this->SetMaxSize(wxSize(400, 330));

	panel = new wxPanel(this, wxID_ANY,
		wxDefaultPosition,
		wxDefaultSize,
		wxTAB_TRAVERSAL);

	codeLabel = new wxStaticText(panel, wxID_ANY, "Enter your exchange code", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
	codeInput = new wxTextCtrl(panel, wxID_ANY);
	codeLink = new wxButton(panel, wxID_ANY, "Get Code");
	codeSubmit = new wxButton(panel, wxID_ANY, "Launch");
	codeInfo = new wxStaticText(panel, wxID_ANY, EXCHANGE_INFO);
	codeSecBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Security Info");
	codeSecInfo = new wxStaticText(panel, wxID_ANY, EXCHANGE_SEC);
	codeSecBox->Add(codeSecInfo, 1, wxEXPAND);

	auto sizerBtns = new wxBoxSizer(wxHORIZONTAL);

	sizerBtns->Add(codeLink, 1, wxEXPAND | wxRIGHT, 3);
	sizerBtns->Add(codeSubmit, 1, wxEXPAND | wxLEFT, 3);

	auto sizer = new wxBoxSizer(wxVERTICAL);

	sizer->Add(codeLabel, 0, wxEXPAND);
	sizer->Add(codeInput, 0, wxEXPAND | wxUP | wxDOWN, 5);
	sizer->Add(sizerBtns, 0, wxEXPAND | wxDOWN, 5);
	sizer->Add(codeInfo, 3, wxEXPAND);
	sizer->Add(codeSecBox, 4, wxEXPAND);

	auto topSizer = new wxBoxSizer(wxVERTICAL);
	topSizer->Add(sizer, wxSizerFlags(1).Expand().Border(wxALL, 5));
	panel->SetSizerAndFit(topSizer);
	this->SetSize(wxSize(400, 330));

	codeLink->Bind(wxEVT_BUTTON, &cAuth::OnGetCodeClicked, this);
	codeSubmit->Bind(wxEVT_BUTTON, &cAuth::OnSubmitClicked, this);
}

cAuth::~cAuth() {

}

wxString& cAuth::GetCode() {
	return returnValue;
}

void cAuth::OnGetCodeClicked(wxCommandEvent& evt) {
	wxLaunchDefaultBrowser(EXCHANGE_URL);
}

void cAuth::OnSubmitClicked(wxCommandEvent& evt) {
	if (codeInput->GetValue().IsEmpty()) {
		return;
	}
	returnValue = codeInput->GetValue();
	Close();
}