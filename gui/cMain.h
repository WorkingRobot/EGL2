#pragma once

#define NOMINMAX
#include "../MountedBuild.h"
#include "../web/manifest/auth.h"
#include "../web/personal/PersonalAuth.h"
#include "cProgress.h"
#include "cSetup.h"
#include "cStorage.h"
#include "GameUpdateChecker.h"
#include "settings.h"
#include "UpdateChecker.h"

#include <wx/taskbar.h>
#include <wx/windowptr.h>
#include <wx/wx.h>

#include <memory>
#include <optional>

class cMain : public wxFrame
{
public:
	cMain(wxApp* app, const fs::path& settingsPath, const fs::path& manifestPath, const std::shared_ptr<PersonalAuth>& personalAuth);
	~cMain();

protected:
	wxPanel* panel = nullptr;

	wxButton* verifyBtn = nullptr;
	wxButton* playBtn = nullptr;

	wxStaticBoxSizer* descBox = nullptr;
	wxStaticText* descTxt = nullptr;

	wxStaticBoxSizer* statsBox = nullptr;

#define DEFINE_STAT(name) \
	wxStaticText* stat##name##Label; \
	wxGauge* stat##name##Value; \
	wxStaticText* stat##name##Text;

	DEFINE_STAT(cpu)
	DEFINE_STAT(ram)
	DEFINE_STAT(read)
	DEFINE_STAT(write)
	DEFINE_STAT(provide)
	DEFINE_STAT(download)
	DEFINE_STAT(latency)
	DEFINE_STAT(threads)

#undef DEFINE_STAT

	wxStaticText* statusBar = nullptr;
	wxStaticText* selloutBar = nullptr;

	void OnButtonHover(const wxString& string);

	void OnSettingsClicked(bool onStartup);
	void OnVerifyClicked();
	void OnPlayClicked();
	void OnStorageClicked();
	
	bool OnClose();

	void SetStatus(const wxString& string);

private:
	void Mount(const std::string& Url);

	wxWeakRef<wxApp> App;
	wxSharedPtr<wxTaskBarIcon> Systray;
	wxWindowPtr<cProgress> VerifyWnd;
	wxWindowPtr<cProgress> UpdateWnd;
	wxWindowPtr<cSetup> SetupWnd;
	wxWindowPtr<cStorage> StorageWnd;

	bool FirstAuthLaunched = false;
	std::shared_ptr<PersonalAuth> Auth;

	std::unique_ptr<GameUpdateChecker> GameUpdater;
	bool GameUpdateAvailable;
	std::optional<std::string> GameUpdateUrl;

	void OnGameUpdate(const std::string& Version, const std::optional<std::string>& Url = std::nullopt);
	void BeginGameUpdate();

	std::unique_ptr<UpdateChecker> Updater;
	std::string UpdateUrl;

	void OnUpdate(const UpdateInfo& Info);
	void BeginUpdate();
	
	fs::path SettingsPath;
	SETTINGS Settings;

	std::unique_ptr<MountedBuild> Build;

	friend class SystrayIcon;
};

