#include "cSplash.h"

#include <wx/dcbuffer.h>

cSplash::cSplash() : wxFrame(nullptr, wxID_ANY, "EGL2 Splash", wxDefaultPosition, wxDefaultSize, wxFRAME_NO_TASKBAR | wxBORDER_NONE),
background(L"SPLASH_ICON", wxBITMAP_TYPE_ICO_RESOURCE, 256, 256) {
    SetClientSize(background.GetSize());

	Bind(wxEVT_PAINT, &cSplash::OnPaint, this);

    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
    SetWindowLong(GetHandle(), GWL_EXSTYLE, GetWindowLong(GetHandle(), GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(GetHandle(), RGB(255, 0, 255), 255, LWA_COLORKEY);

    CenterOnScreen();
}

cSplash::~cSplash() {

}

void cSplash::OnPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    dc.SetBrush(wxBrush(wxColor(255, 0, 255)));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, 256, 256);

    dc.DrawIcon(background, 0, 0);
}
