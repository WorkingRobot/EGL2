#pragma once

#include <wx/wxprec.h>
#include <wx/slider.h>

class wxLabelSlider : public wxPanel {
public:
	wxLabelSlider();

    // only wxSL_AUTOTICKS, wxSL_MIN_MAX_LABELS, and wxSL_VALUE_LABEL flags are respected
	wxLabelSlider(wxWindow* parent,
        wxWindowID id,
        int selectedIndex,
        const wxArrayString& values,
        long style = 0,
        const wxString& name = wxSliderNameStr);

    int GetValue() {
        return slider->GetValue();
    }

    void SetValue(int value) {
        slider->SetValue(value);
        _CreateEvent(wxEVT_SCROLL_CHANGED);
    }

private:
    void _OnTop(wxScrollEvent& evt);
    void _OnBottom(wxScrollEvent& evt);
    void _OnLineUp(wxScrollEvent& evt);
    void _OnLineDown(wxScrollEvent& evt);
    void _OnPageUp(wxScrollEvent& evt);
    void _OnPageDown(wxScrollEvent& evt);
    void _OnThumbTrack(wxScrollEvent& evt);
    void _OnThumbRelease(wxScrollEvent& evt);
    void _OnChanged(wxScrollEvent& evt);

    template<typename EventTag>
    void _CreateEvent(const EventTag& eventType);

    wxArrayString values;
    bool mLabel, vLabel;
    wxSlider* slider;
    wxStaticText* sliderText;
    wxBoxSizer* sliderSizer;
    wxBoxSizer* panelSizer;
};