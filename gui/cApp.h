#pragma once

#include "../web/personal/PersonalAuth.h"
#include "cMain.h"

#include <memory>
#include <wx/snglinst.h>
#include <wx/wx.h>

class cApp : public wxApp
{
public:
	cApp();
	~cApp();

	virtual bool OnInit();
	bool InitThread();

	wxWindow* SplashWindow = nullptr;

	fs::path DataFolder;
	FILE* LogFile = nullptr;

	wxSingleInstanceChecker* InstanceChecker = nullptr;
	std::shared_ptr<PersonalAuth> AuthDetails;
};

