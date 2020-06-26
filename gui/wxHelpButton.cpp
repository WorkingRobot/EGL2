#include "wxHelpButton.h"

#ifndef LOG_SECTION
#define LOG_SECTION "HelpBtn"
#endif

#include "../Logger.h"

#include <HtmlHelp.h>

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

wxHelpButton::wxHelpButton(wxWindow* parent, wxWindowID id, const wchar_t* helpFile) :
	wxBitmapButton(parent, id, csquery_bmp, wxDefaultPosition, wxSize(24, 24)),
	Topic(helpFile)
{
	Bind(wxEVT_BUTTON, &wxHelpButton::_OnClick, this);
	Bind(wxEVT_SIZE, [this] (wxSizeEvent& evt) { // keeps it 1:1 aspect ratio
		auto size = std::min(evt.GetSize().GetWidth(), evt.GetSize().GetHeight());
		evt.SetSize(wxSize(size, size));
		SetSize(wxSize(size, size));
	});
}

bool VerifyChmFile(const fs::path& cacheFile) {
    std::error_code ec;
    if (fs::is_regular_file(cacheFile, ec)) {
        if (fs::file_size(cacheFile, ec) > 8 * 1024) { // it's at least 8 kb, just check if it wasn't written properly
            return true;
        }
    }
    auto resInfo = FindResource(NULL, L"CHM_HELP", RT_RCDATA);
    if (!resInfo) {
        LOG_FATAL("Could not find locale resource!");
        return false;
    }
    auto resData = LoadResource(NULL, resInfo);
    if (!resData) {
        LOG_FATAL("Could not load locale resource!");
        return false;
    }
    auto resPtr = LockResource(resData);
    if (!resPtr) {
        LOG_FATAL("Could not lock locale resource!");
        FreeResource(resData);
        return false;
    }
    auto resSize = SizeofResource(NULL, resInfo);
    if (!resSize) {
        LOG_FATAL("Could not get size of locale resource!");
        FreeResource(resData);
        return false;
    }

    auto filePtr = fopen(cacheFile.string().c_str(), "wb");
    if (!filePtr) {
        LOG_FATAL("Could not open %s for writing!", cacheFile.string().c_str());
        return false;
    }
    fwrite(resPtr, resSize, 1, filePtr);
    fclose(filePtr);

    FreeResource(resData);
    return true;
}

bool wxHelpButton::LoadHelp(const fs::path& cachePath)
{
    File = cachePath / "help.chm";
    if (!VerifyChmFile(File)) {
        return false;
    }
    return true;
}

void wxHelpButton::_OnClick(wxCommandEvent& evt)
{
	auto helpHwnd = HtmlHelp(GetHWND(), File.c_str(), HH_DISPLAY_TOPIC, (DWORD_PTR)Topic);
}
