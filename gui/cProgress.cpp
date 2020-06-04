#include "cProgress.h"

#include "Localization.h"

#include <wx/appprogress.h>
#include <utility>

cProgress::cProgress(wxWindow* main, wxString taskName, cancel_flag& cancelFlag, std::function<void()> onCancel, float updateFreq, uint32_t maximum) : wxFrame(main, wxID_ANY, LTITLE(taskName), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE ^ (wxMAXIMIZE_BOX | wxRESIZE_BORDER)) {
	OnCancel = onCancel;
	value = 0;
	frequency = updateFreq;
	maxValue = maximum;
	startTime = Clock::now();
	etaTimePoints.push(std::make_pair(0, startTime));

	this->SetIcon(wxICON(APP_ICON));
	this->SetMinSize(wxSize(400, -1));
	this->SetMaxSize(wxSize(400, -1));

	panel = new wxPanel(this, wxID_ANY,
		wxDefaultPosition,
		wxDefaultSize,
		wxTAB_TRAVERSAL);

	progressBar = new wxGauge(panel, wxID_ANY, maxValue, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL | wxGA_SMOOTH);

	progressPercent = new wxStaticText(panel, wxID_ANY, "0.00%", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
	progressTotal = new wxStaticText(panel, wxID_ANY, "0 / 0", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
	progressTimeElapsed = new wxStaticText(panel, wxID_ANY, wxString::Format("%s: 00:00:00", LSTR(PROG_LABEL_ELAPSED)), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
	progressTimeETA = new wxStaticText(panel, wxID_ANY, wxString::Format("%s: 00:00:00", LSTR(PROG_LABEL_ETA)), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);

	progressCancelBtn = new wxButton(panel, wxID_ANY, LSTR(PROG_BTN_CANCEL));
	progressTaskbar = new wxAppProgressIndicator(this, maxValue);

	progressTextSizer = new wxGridSizer(2, 2, 5, 5);
	progressTextSizer->Add(progressPercent, 1, wxEXPAND);
	progressTextSizer->Add(progressTimeElapsed, 1, wxEXPAND);
	progressTextSizer->Add(progressTotal, 1, wxEXPAND);
	progressTextSizer->Add(progressTimeETA, 1, wxEXPAND);

	auto sizer = new wxBoxSizer(wxVERTICAL);

	sizer->Add(progressBar, 0, wxEXPAND);
	sizer->Add(progressTextSizer, 0, wxEXPAND | wxUP | wxDOWN, 5);
	sizer->Add(progressCancelBtn, 0, wxEXPAND);

	auto topSizer = new wxBoxSizer(wxVERTICAL);
	topSizer->Add(sizer, wxSizerFlags(1).Expand().Border(wxALL, 5));
	panel->SetSizerAndFit(topSizer);
	this->Fit();

	progressCancelBtn->Bind(wxEVT_BUTTON, [this, &cancelFlag](wxCommandEvent& evt) { Cancel(cancelFlag); });
	Bind(wxEVT_CLOSE_WINDOW, [this, &cancelFlag](wxCloseEvent& evt) { Cancel(cancelFlag); evt.Veto(); });
}

cProgress::~cProgress() {
	delete progressTaskbar;
}

inline void cProgress::Cancel(cancel_flag& cancelFlag) {
	cancelFlag.cancel();
	progressCancelBtn->Disable();
	progressCancelBtn->SetLabel(LSTR(PROG_BTN_CANCELLING));
	Finish();
	Hide();
	OnCancel();
}

inline wxString FormatTime(ch::seconds secs) {
	auto mins = ch::duration_cast<ch::minutes>(secs);
	secs -= ch::duration_cast<ch::seconds>(mins);
	auto hour = ch::duration_cast<ch::hours>(mins);
	mins -= ch::duration_cast<ch::minutes>(hour);

	return wxString::Format("%02d:%02d:%02d", hour.count(), mins.count(), int(secs.count()));
}

inline ch::seconds GetETA(ch::nanoseconds duration, uint32_t amt, uint32_t targetAmt) {
	return amt ? ch::duration_cast<ch::seconds>(duration * targetAmt / amt) : ch::seconds::zero();
}

inline void cProgress::Update(bool force) {
	auto now = Clock::now();
	ch::duration<float> duration = now - lastUpdate;
	if (duration.count() < frequency || force) {
		return;
	}
	lastUpdate = now;
	auto val = value.load();
	SetMaximum((std::max)(val, maxValue));
	progressBar->SetValue(val + 1);
	progressBar->SetValue(val);
	progressTaskbar->SetValue(val);

	{
		progressPercent->SetLabel(wxString::Format("%.2f%%", float(val) * 100 / maxValue));
		progressTotal->SetLabel(wxString::Format("%u / %u", val, maxValue));

		auto elapsed = ch::duration_cast<ch::seconds>(now - startTime);
		progressTimeElapsed->SetLabel(wxString::Format("%s: %s", LSTR(PROG_LABEL_ELAPSED), FormatTime(elapsed)));

		auto& timePoint = etaTimePoints.front();
		auto eta = GetETA(now - timePoint.second, val - timePoint.first, maxValue - val);
		progressTimeETA->SetLabel(wxString::Format("%s: %s", LSTR(PROG_LABEL_ETA), FormatTime(eta)));

		etaTimePoints.push(std::make_pair(val, now));
		if (etaTimePoints.size() > queueSize) {
			etaTimePoints.pop();
		}
	}
}

void cProgress::SetFrequency(float updateFreq) {
	frequency = updateFreq;
}

void cProgress::SetMaximum(uint32_t maximum) {
	if (maximum != maxValue) {
		maxValue = maximum;
		progressBar->SetRange(maxValue);
		progressTaskbar->SetRange(maxValue);
		Update();
	}
}

void cProgress::Increment() {
	if (finished) {
		return;
	}
	value++;
	Update(value == maxValue);
}

void cProgress::Finish() {
	value = maxValue;
	finished = true;
	Update(true);
}