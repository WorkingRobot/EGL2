#pragma once

#define LSTR_LOC(str) L"" #str ".htm"

#include <filesystem>
#include <wx/wxprec.h>
#include <wx/bmpbuttn.h>

namespace fs = std::filesystem;

class wxHelpButton : public wxBitmapButton {
public:
    wxHelpButton();

    wxHelpButton(wxWindow* parent, wxWindowID id, const wchar_t* topic);

    static bool LoadHelp(const fs::path& cachePath);

private:
    static inline fs::path File;

    const wchar_t* Topic = nullptr;

    void OnClick(wxCommandEvent& evt);
};