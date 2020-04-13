#pragma once

#include "../MountedBuild.h"
#include "settings.h"
#include <wx/wx.h>

class cMain : public wxFrame
{
public:
	cMain(fs::path settingsPath, fs::path manifestPath);
	~cMain();

protected:
	wxPanel* panel = nullptr;

	wxButton* setupBtn = nullptr;
	wxButton* verifyBtn = nullptr;
	wxButton* purgeBtn = nullptr;
	wxButton* preloadBtn = nullptr;
	wxButton* startBtn = nullptr;
	wxButton* startFnBtn = nullptr;

	wxStaticBoxSizer* descBox = nullptr;
	wxStaticText* descTxt = nullptr;

	wxStaticText* statusBar = nullptr;
	wxStaticText* selloutBar = nullptr;

	void OnButtonHover(const char* string);

	void OnSetupClicked();
	void OnVerifyClicked();
	void OnPurgeClicked();
	void OnPreloadClicked();
	void OnStartClicked();
	void OnPlayClicked();
	
	void OnClose(wxCloseEvent& evt);

	void SetStatus(const char* string);

private:
	std::unique_ptr<MountedBuild>& GetMountedBuild();
	void Unmount();

	fs::path SettingsPath;
	SETTINGS Settings;

	std::unique_ptr<MountedBuild> Build;

	MANIFEST_AUTH* ManifestAuth;
	MANIFEST* Manifest;
};

