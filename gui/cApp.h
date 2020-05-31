#pragma once

#include "../web/personal/PersonalAuth.h"
#include "cMain.h"

#include <memory>
#include <wx/wx.h>

class cApp : public wxApp
{
public:
	cApp();
	~cApp();

	virtual bool OnInit();

	fs::path DataFolder;
	FILE* LogFile;

	std::shared_ptr<PersonalAuth> AuthDetails;
};

