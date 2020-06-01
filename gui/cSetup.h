#pragma once

#include "cMain.h"
#include "wxModalWindow.h"

#include <vector>
#include <functional>
#include <wx/filepicker.h>
#include <wx/wx.h>

class cSetup : public wxFrame
{
public:
	using flush_callback = std::function<void(SETTINGS*)>;
	using validate_callback = std::function<bool(SETTINGS*)>;
	using exit_callback = std::function<void()>;

	cSetup(cMain* main, SETTINGS* settings, bool startupInvalid, flush_callback callback, validate_callback validator, exit_callback onExit);
	~cSetup();

private:
	std::vector<std::function<void(SETTINGS* val)>> ReadBinds;
	std::vector<std::function<void(SETTINGS* val)>> WriteBinds;

	SETTINGS* Settings;
	bool InvalidStartup;
	SETTINGS OldSettings;
	flush_callback Callback;
	validate_callback Validator;
	exit_callback OnExit;

	wxWindowDisabler* Disabler;

	void ReadConfig();
	void WriteConfig();

	void OkClicked();
	void CancelClicked();
	void ApplyClicked();
	void CloseClicked(wxCloseEvent& evt);
};

