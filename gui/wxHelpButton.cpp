#include "wxHelpButton.h"
#include <wx/aboutdlg.h>

// taken from https://github.com/wxWidgets/wxWidgets/blob/cc931612eec2e3ea49200ebff45042135f3c3f9c/src/common/cshelp.cpp#L282
constexpr const char* csquery_xpm[] = { // this is cleaner than wxBITMAP(csquery)
"12 11 2 1",
"  c None",
". c #000000",
"            ",
"    ....    ",
"   ..  ..   ",
"   ..  ..   ",
"      ..    ",
"     ..     ",
"     ..     ",
"            ",
"     ..     ",
"     ..     ",
"            " };
static const wxBitmap csquery_bmp(csquery_xpm);

wxHelpButton::wxHelpButton() : wxBitmapButton() { }

wxHelpButton::wxHelpButton(wxWindow* parent, wxWindowID id, const wxString& description) :
	wxBitmapButton(parent, id, csquery_bmp, wxDefaultPosition, wxSize(24, 24)),
	clickData(description)
{
	Bind(wxEVT_BUTTON, &wxHelpButton::_OnClick, this);
	Bind(wxEVT_SIZE, [this] (wxSizeEvent& evt) { // keeps is 1:1 aspect ratio
		auto size = std::min(evt.GetSize().GetWidth(), evt.GetSize().GetHeight());
		evt.SetSize(wxSize(size, size));
		SetSize(wxSize(size, size));
	});
}

void wxHelpButton::_OnClick(wxCommandEvent& evt)
{
	wxMessageBox(clickData, "Help - EGL2", wxICON_NONE | wxOK, this);
}
