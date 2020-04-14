#pragma once

#include "cMain.h"
#include "../containers/cancel_flag.h"

#include <wx/wx.h>

#include <queue>

class cProgress : public wxFrame
{
	typedef std::chrono::steady_clock Clock;
	const int queueSize = 60;

public:
	cProgress(cMain* main, wxString taskName, cancel_flag& cancelFlag, float updateFreq = .05f, uint32_t maximum = 1);
	~cProgress();

	void SetFrequency(float updateFreq);
	void SetMaximum(uint32_t maximum);
	void Increment();
	void Finish();

protected:
	wxPanel* panel = nullptr;

	wxGauge* progressBar = nullptr;
	wxSizer* progressTextSizer = nullptr;
	wxStaticText* progressPercent = nullptr;
	wxStaticText* progressTotal = nullptr;
	wxStaticText* progressTimeElapsed = nullptr;
	wxStaticText* progressTimeETA = nullptr;
	wxButton* progressCancelBtn = nullptr;

	wxAppProgressIndicator* progressTaskbar = nullptr;
	wxWindowDisabler* progressDisabler = nullptr;

private:
	void Cancel(cancel_flag& cancelFlag);

	void Update(bool force = false);

	bool finished = false;
	std::queue<std::pair<uint32_t, Clock::time_point>> etaTimePoints;
	Clock::time_point lastUpdate;
	float frequency;
	std::atomic_uint32_t value;
	uint32_t maxValue;
};

