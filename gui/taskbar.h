#pragma once

#include "cMain.h"

#include <wx/taskbar.h>
#include <wx/menu.h>

class cMain;

class SystrayIcon : public wxTaskBarIcon {
public:
	SystrayIcon(cMain* main) : wxTaskBarIcon(),
	Main(main)
	{
		Bind(wxEVT_TASKBAR_LEFT_UP, [this](wxTaskBarIconEvent& evt) {
			Main->Show();
			Main->Restore();
			Main->Raise();
			Main->SetFocus();
		});
	}

protected:
	wxMenu* CreatePopupMenu() override {
		auto menu = new wxMenu();

		menu->Bind(wxEVT_MENU, [this](wxCommandEvent& evt) {
			switch (evt.GetId())
			{
			case SETTINGS_ID:
				Main->OnSettingsClicked(false);
				break;
			case VERIFY_ID:
				Main->OnVerifyClicked();
				break;
			case PLAY_ID:
				Main->OnPlayClicked();
				break;
			case EXIT_ID:
				if (Main->OnClose()) {
					Stats::StopUpdateThread();
					Main->Checker->StopUpdateThread();
					Main->Destroy();
					Main->App->Exit();
				}
				break;
			}
		});

		auto titleItem = menu->Append(TITLE_ID, "EGL2");
		titleItem->Enable(false);
		titleItem->SetBitmap(wxIcon(L"APP_ICON", wxBITMAP_TYPE_ICO_RESOURCE, 16, 16));

		menu->AppendSeparator();

		menu->Append(SETTINGS_ID, LSTR(MAIN_BTN_SETTINGS));
		menu->Append(VERIFY_ID, LSTR(MAIN_BTN_VERIFY));
		menu->Append(PLAY_ID, Main->playBtn->GetLabel());

		menu->AppendSeparator();

		menu->Append(EXIT_ID, "Exit");
		return menu;
	}

private:
	cMain* Main;

	static constexpr int TITLE_ID = 42;
	static constexpr int SETTINGS_ID = 43;
	static constexpr int VERIFY_ID = 44;
	static constexpr int PLAY_ID = 45;
	static constexpr int EXIT_ID = 46;
};