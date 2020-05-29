#pragma once

#include "cMain.h"

#include <wx/wx.h>

class cApp : public wxApp
{
public:
	cApp();
	~cApp();

	virtual bool OnInit();

	fs::path DataFolder;
	FILE* LogFile;
};

