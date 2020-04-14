#pragma once

#include "cMain.h"
#include "wxModalWindow.h"

#include <wx/wx.h>
#include <wx/filepicker.h>

class cSetup : public wxModalWindow
{
public:
	cSetup(cMain* main, SETTINGS* settings);
	~cSetup();

protected:
	wxPanel* topPanel = nullptr;

	wxWindow* settingsContainer = nullptr;
	wxWindow* checkboxContainer = nullptr;
	wxStaticBoxSizer* advancedContainer = nullptr;

	wxStaticText* cacheDirTxt = nullptr;
	wxDirPickerCtrl* cacheDirValue = nullptr;

	wxStaticText* compMethodTxt = nullptr;
	wxChoice* compMethodValue = nullptr;

	wxStaticText* compLevelTxt = nullptr;
	wxChoice* compLevelValue = nullptr;

	wxCheckBox* verifyCacheCheckbox = nullptr;
	wxCheckBox* gameDirCheckbox = nullptr;

	wxStaticText* cmdArgsTxt = nullptr;
	wxTextCtrl* cmdArgsValue = nullptr;

private:
	SETTINGS* Settings;

	void ReadConfig();
	void WriteConfig();
};

