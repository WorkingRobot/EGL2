#pragma once

#include "../containers/cancel_flag.h"

#include <chrono>
#include <queue>
#include <wx/wx.h>

namespace ch = std::chrono;

class cProgress : public wxFrame
{
	typedef std::chrono::steady_clock Clock;
	static constexpr int queueSize = 200;

public:
	cProgress(wxWindow* main, wxString taskName, cancel_flag& cancelFlag, std::function<void()> onCancel, float updateFreq = .05f, uint32_t maximum = 1);
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

private:
	std::function<void()> OnCancel;
	void Cancel(cancel_flag& cancelFlag);

	void Update(bool force = false);

	bool finished = false;
	std::queue<std::pair<uint32_t, Clock::time_point>> etaTimePoints;
	Clock::time_point startTime;
	Clock::time_point lastUpdate;
	float frequency;
	std::atomic_uint32_t value;
	uint32_t maxValue;
};

