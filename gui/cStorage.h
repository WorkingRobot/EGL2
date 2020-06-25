#pragma once

#include "../containers/cancel_flag.h"
#include "../MountedBuild.h"
#include "wxModalWindow.h"

#include <atomic>
#include <functional>
#include <wx/wx.h>

class cStorage : public wxModalWindow
{
public:
	cStorage(wxWindow* main, std::unique_ptr<MountedBuild>& build, uint32_t threadCount, std::function<void()> onClose);
	~cStorage();

private:
	wxGauge* storageBar;
	wxStaticText* storageTxt;

	wxGauge* downloadBar;
	wxStaticText* downloadTxt;

	wxGauge* diskBar;
	wxStaticText* diskTxt;

	// [decomp, decomp%, comp, comp%, compRatio]
	wxStaticText* compTexts[ChunkFlagCompCount][5];

	// [decompressed, compressed]
	std::atomic_size_t compStats[ChunkFlagCompCount][2];
	std::atomic_size_t compMainStats[2];

	cancel_flag flag;
	std::atomic_uint32_t chunkCount = 0;
	std::atomic_uint32_t chunkDlCount = 0;
	std::atomic<std::chrono::steady_clock::time_point> lastUpdate;
	void AddChunkData(const ChunkMetadata& meta, bool isLast);

	std::function<void()> onClose;
	void OnClose(wxCloseEvent& evt);
};

