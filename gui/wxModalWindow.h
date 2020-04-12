// Taken from https://forums.wxwidgets.org/viewtopic.php?t=20752#p89334

/**********************************************************************************************
 *
 * Filename  : modalwindow.h
 * Purpose   : Allow a modalwindow like wxDialog but allowing menus and such.
 * Author    : John A. Mason
 * Created   : 8/27/2008 07:54:12 AM
 * Copyright : Released under wxWidgets original license.
 *
 **********************************************************************************************/

#ifndef __wx_ModalWindow_h__
#define __wx_ModalWindow_h__

#include <wx/wxprec.h>

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include <wx/frame.h>

#ifndef WX_PRECOMP
#include <wx/utils.h>
#include <wx/app.h>
#endif

#include <wx/evtloop.h>

class wxModalWindow : public wxFrame {
private:
	// while we are showing a modal window we disable the other windows using
	// this object
	wxWindowDisabler* m_windowDisabler;

	// modal window runs its own event loop
	wxEventLoop* m_eventLoop;

	// is modal right now?
	bool m_isShowingModal;

	//The return code of a modal window
	int m_returnCode;
public:
	wxModalWindow();
	wxModalWindow(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_FRAME_STYLE, const wxString& name = "modalwindow");
	virtual ~wxModalWindow();


	void Init();
	bool Show(bool show);
	bool IsModal() const;
	int ShowModal();

	void EndModal(int retCode);
	void SetReturnCode(int retCode);
	int GetReturnCode() const;
};

#endif
