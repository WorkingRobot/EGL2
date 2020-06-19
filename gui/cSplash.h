#pragma once

#include <wx/wx.h>

class cSplash : public wxFrame
{
public:
	cSplash();
	~cSplash();

private:
	wxIcon background;

	void OnPaint(wxPaintEvent& event);
};