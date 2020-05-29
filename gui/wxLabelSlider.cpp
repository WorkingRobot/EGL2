#include "wxLabelSlider.h"

wxLabelSlider::wxLabelSlider() : wxPanel() { }

wxLabelSlider::wxLabelSlider(wxWindow* parent, wxWindowID id, int selectedIndex, const wxArrayString& values, long style, const wxString& name) :
    wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 45)),
    values(values)
{
    if (style & wxSL_SELRANGE) {
        style ^= wxSL_SELRANGE;
    }

    mLabel = style & wxSL_MIN_MAX_LABELS;
    if (style & wxSL_MIN_MAX_LABELS) {
        style ^= wxSL_MIN_MAX_LABELS;
    }

    vLabel = style ^ wxSL_VALUE_LABEL;
    if (style & wxSL_VALUE_LABEL) {
        style ^= wxSL_VALUE_LABEL;
    }

    slider = new wxSlider(this, id, selectedIndex, 0, values.size() - 1, wxDefaultPosition, wxSize(-1, 32), style | wxSL_HORIZONTAL, wxDefaultValidator, name);
    sliderText = new wxStaticText(this, wxID_ANY, values[0], wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);

    sliderSizer = new wxBoxSizer(wxVERTICAL);
    sliderSizer->Add(slider, 1, wxEXPAND);
    sliderSizer->Add(sliderText, 0, wxEXPAND);
    
    panelSizer = new wxBoxSizer(wxHORIZONTAL);

    int labelSize;
    if (mLabel) {
        labelSize = std::max(GetTextExtent(values[0]).x, GetTextExtent(values[values.size() - 1]).x);
        panelSizer->Add(new wxStaticText(this, wxID_ANY, values[0], wxDefaultPosition, wxSize(labelSize, -1), wxALIGN_CENTRE_HORIZONTAL), 0, wxALIGN_CENTRE);
    }
    panelSizer->Add(sliderSizer, 1, wxEXPAND);
    if (mLabel) {
        panelSizer->Add(new wxStaticText(this, wxID_ANY, values[values.size() - 1], wxDefaultPosition, wxSize(labelSize, -1), wxALIGN_CENTRE_HORIZONTAL), 0, wxALIGN_CENTRE);
    }

    slider->Bind(wxEVT_SCROLL_TOP, &wxLabelSlider::_OnTop, this);
    slider->Bind(wxEVT_SCROLL_BOTTOM, &wxLabelSlider::_OnBottom, this);
    slider->Bind(wxEVT_SCROLL_LINEUP, &wxLabelSlider::_OnLineUp, this);
    slider->Bind(wxEVT_SCROLL_LINEDOWN, &wxLabelSlider::_OnLineDown, this);
    slider->Bind(wxEVT_SCROLL_PAGEUP, &wxLabelSlider::_OnPageUp, this);
    slider->Bind(wxEVT_SCROLL_PAGEDOWN, &wxLabelSlider::_OnPageDown, this);
    slider->Bind(wxEVT_SCROLL_THUMBTRACK, &wxLabelSlider::_OnThumbTrack, this);
    slider->Bind(wxEVT_SCROLL_THUMBRELEASE, &wxLabelSlider::_OnThumbRelease, this);
    slider->Bind(wxEVT_SCROLL_CHANGED, &wxLabelSlider::_OnChanged, this);

    SetSizer(panelSizer);
    Refresh();
}

void wxLabelSlider::_OnTop(wxScrollEvent& evt)
{
    _CreateEvent(wxEVT_SCROLL_TOP);
}

void wxLabelSlider::_OnBottom(wxScrollEvent& evt)
{
    _CreateEvent(wxEVT_SCROLL_BOTTOM);
}

void wxLabelSlider::_OnLineUp(wxScrollEvent& evt)
{
    _CreateEvent(wxEVT_SCROLL_LINEUP);
}

void wxLabelSlider::_OnLineDown(wxScrollEvent& evt)
{
    _CreateEvent(wxEVT_SCROLL_LINEDOWN);
}

void wxLabelSlider::_OnPageUp(wxScrollEvent& evt)
{
    _CreateEvent(wxEVT_SCROLL_PAGEUP);
}

void wxLabelSlider::_OnPageDown(wxScrollEvent& evt)
{
    _CreateEvent(wxEVT_SCROLL_PAGEDOWN);
}

void wxLabelSlider::_OnThumbTrack(wxScrollEvent& evt)
{
    _CreateEvent(wxEVT_SCROLL_THUMBTRACK);
}

void wxLabelSlider::_OnThumbRelease(wxScrollEvent& evt)
{
    _CreateEvent(wxEVT_SCROLL_THUMBRELEASE);
}

void wxLabelSlider::_OnChanged(wxScrollEvent& evt)
{
    _CreateEvent(wxEVT_SCROLL_CHANGED);
}

template<typename EventTag>
void wxLabelSlider::_CreateEvent(const EventTag& eventType)
{
    sliderText->SetLabel(values[slider->GetValue()]);

    auto evt = wxScrollEvent(eventType, GetId());
    evt.SetEventObject(this);
    GetEventHandler()->ProcessEvent(evt);
    slider->Refresh();
    Refresh();
}
