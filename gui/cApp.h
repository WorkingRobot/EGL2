#pragma once

#include "cMain.h"

#include <wx/wx.h>

class cApp : public wxApp
{
public:
	cApp();
	~cApp();

private:
	cMain* m_frame1 = nullptr;

public:
	virtual bool OnInit();
};

