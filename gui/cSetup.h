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

	wxStaticText* cacheDirTxt = nullptr;
	wxDirPickerCtrl* cacheDirValue = nullptr;

	wxStaticText* mountDirTxt = nullptr;
	wxChoice* mountDirValue = nullptr;
	// TODO: add folder support with a radio button/combo box

	wxStaticText* gameDirTxt = nullptr;
	wxDirPickerCtrl* gameDirValue = nullptr;

	wxStaticText* compMethodTxt = nullptr;
	wxChoice* compMethodValue = nullptr;

	wxStaticText* compLevelTxt = nullptr;
	wxChoice* compLevelValue = nullptr;

	wxCheckBox* verifyCacheCheckbox = nullptr;
	wxCheckBox* gameDirCheckbox = nullptr;

private:
	SETTINGS* Settings;

	void ReadConfig();
	void WriteConfig();
};

