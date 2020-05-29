#pragma once

#define NOMINMAX
#include "../MountedBuild.h"
#include "../web/manifest/auth.h"
#include "UpdateChecker.h"
#include "settings.h"

#include <wx/wx.h>

#include <optional>

class cMain : public wxFrame
{
public:
	cMain(fs::path settingsPath, fs::path manifestPath);
	~cMain();

protected:
	wxPanel* panel = nullptr;

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

	void OnButtonHover(const char* string);

	void OnSettingsClicked(bool onStartup);
	void OnVerifyClicked();
	void OnPlayClicked();
	
	void OnClose(wxCloseEvent& evt);

	void SetStatus(const char* string);

	void OnUpdate(const std::string& Version, const std::optional<std::string>& Url = std::nullopt);
	void BeginUpdate();

private:
	void Mount(const std::string& Url);

	bool UpdateAvailable;
	std::optional<std::string> UpdateUrl;
	
	fs::path SettingsPath;
	SETTINGS Settings;

	std::unique_ptr<UpdateChecker> Checker;
	std::unique_ptr<MountedBuild> Build;
};

