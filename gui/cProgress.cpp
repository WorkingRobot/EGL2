#include "cProgress.h"

#include <wx/appprogress.h>

#include <chrono>
namespace ch = std::chrono;

cProgress::cProgress(cMain* main, wxString taskName, cancel_flag& cancelFlag, float updateFreq, uint32_t maximum) : wxFrame(main, wxID_ANY, taskName + " - EGL2", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE ^ (wxMAXIMIZE_BOX | wxRESIZE_BORDER)) {
	value = 0;
	frequency = updateFreq;
	maxValue = maximum;
	etaTimePoints.push(std::make_pair(0, Clock::now()));

	this->SetIcon(wxICON(APP_ICON));
	this->SetMinSize(wxSize(400, -1));
	this->SetMaxSize(wxSize(400, -1));

	panel = new wxPanel(this, wxID_ANY,
		wxDefaultPosition,
		wxDefaultSize,
		wxTAB_TRAVERSAL);

	progressBar = new wxGauge(panel, wxID_ANY, maxValue, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL | wxGA_SMOOTH);

	progressPercent = new wxStaticText(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
	progressTotal = new wxStaticText(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
	progressTimeElapsed = new wxStaticText(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
	progressTimeETA = new wxStaticText(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);

	progressCancelBtn = new wxButton(panel, wxID_ANY, "Cancel");
	progressTaskbar = new wxAppProgressIndicator(this, maxValue);
	progressDisabler = new wxWindowDisabler(this);

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
	Bind(wxEVT_CLOSE_WINDOW, [this, &cancelFlag](wxCloseEvent& evt) { Cancel(cancelFlag); });
}

cProgress::~cProgress() {
	delete progressTaskbar;
	delete progressDisabler;
}

inline void cProgress::Cancel(cancel_flag& cancelFlag) {
	cancelFlag.cancel();
	progressCancelBtn->Disable();
	progressCancelBtn->SetLabel("Cancelling");
}

inline wxString FormatTime(ch::seconds secs) {
	auto mins = ch::duration_cast<ch::minutes>(secs);
	secs -= ch::duration_cast<ch::seconds>(mins);
	auto hour = ch::duration_cast<ch::hours>(mins);
	mins -= ch::duration_cast<ch::minutes>(hour);

	return wxString::Format("%02d:%02d:%02d", hour.count(), mins.count(), int(secs.count()));
}

inline void cProgress::Update(bool force) {
	auto now = Clock::now();
	ch::duration<float> duration = now - lastUpdate;
	if (duration.count() < frequency || force) {
		return;
	}
	lastUpdate = now;
	progressBar->SetValue(value + 1);
	progressBar->SetValue(value);
	progressTaskbar->SetValue(value);

	{
		progressPercent->SetLabel(wxString::Format("%.2f%%", float(value) * 100 / maxValue));
		progressTotal->SetLabel(wxString::Format("%u / %u", value.load(), maxValue));

		auto& timePoint = etaTimePoints.front();
		auto elapsed = ch::duration_cast<ch::seconds>(now - timePoint.second);
		progressTimeElapsed->SetLabel("Elapsed: " + FormatTime(elapsed));

		auto etaCount = value - timePoint.first;
		auto etaDivisor = float(maxValue - etaCount) / etaCount;
		auto eta = etaDivisor ? ch::duration_cast<ch::seconds>(elapsed * etaDivisor) : ch::seconds::zero();
		progressTimeETA->SetLabel("ETA: " + FormatTime(eta));

		etaTimePoints.push(std::make_pair(value.load(), now));
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