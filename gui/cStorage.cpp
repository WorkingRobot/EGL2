#include "cStorage.h"

#include "../Stats.h"
#include "Localization.h"

#include <algorithm>
#include <wx/gbsizer.h>
#include <wx/statline.h>

cStorage::cStorage(wxWindow* main, std::unique_ptr<MountedBuild>& build, uint32_t threadCount, std::function<void()> onClose) : wxModalWindow(main, wxID_ANY, LTITLE("Storage"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE ^ (wxMAXIMIZE_BOX | wxRESIZE_BORDER)),
	onClose(onClose)
{
	this->SetIcon(wxICON(APP_ICON));
	this->SetMinSize(wxSize(500, -1));
	this->SetMaxSize(wxSize(500, -1));

	auto panel = new wxPanel(this, wxID_ANY,
		wxDefaultPosition,
		wxDefaultSize,
		wxTAB_TRAVERSAL);

	auto mainSizer = new wxBoxSizer(wxVERTICAL);

	auto storageBarSizer = new wxBoxSizer(wxVERTICAL);
	{
		storageBar = new wxGauge(panel, wxID_ANY, 1000);
		storageTxt = new wxStaticText(panel, wxID_ANY, "0.00%");

		auto storageTxtSizer = new wxBoxSizer(wxHORIZONTAL);
		storageTxtSizer->Add(new wxStaticText(panel, wxID_ANY, "Compression Ratio"));
		storageTxtSizer->AddStretchSpacer();
		storageTxtSizer->Add(storageTxt);

		storageBarSizer->Add(storageBar, wxSizerFlags().Expand());
		storageBarSizer->Add(storageTxtSizer, wxSizerFlags().Expand());
	}

	auto downloadBarSizer = new wxBoxSizer(wxVERTICAL);
	{
		downloadBar = new wxGauge(panel, wxID_ANY, 1000);
		downloadTxt = new wxStaticText(panel, wxID_ANY, "0.00%");

		auto downloadTxtSizer = new wxBoxSizer(wxHORIZONTAL);
		downloadTxtSizer->Add(new wxStaticText(panel, wxID_ANY, "Downloaded"));
		downloadTxtSizer->AddStretchSpacer();
		downloadTxtSizer->Add(downloadTxt);

		downloadBarSizer->Add(downloadBar, wxSizerFlags().Expand());
		downloadBarSizer->Add(downloadTxtSizer, wxSizerFlags().Expand());
	}

	auto diskBarSizer = new wxBoxSizer(wxVERTICAL);
	{
		ULARGE_INTEGER total, free;
		if (!GetDiskFreeSpaceEx(build->GetCachePath().c_str(), NULL, &total, &free)) {
			total.QuadPart = 1;
			free.QuadPart = 1;
		}

		diskBar = new wxGauge(panel, wxID_ANY, 1000);
		diskTxt = new wxStaticText(panel, wxID_ANY, Stats::GetReadableSize(total.QuadPart - free.QuadPart));
		diskBar->SetValue(round((total.QuadPart - free.QuadPart) * 1000. / total.QuadPart));

		auto diskTxtSizer = new wxBoxSizer(wxHORIZONTAL);
		diskTxtSizer->Add(new wxStaticText(panel, wxID_ANY, "Used Disk Space"));
		diskTxtSizer->AddStretchSpacer();
		diskTxtSizer->Add(diskTxt, wxSizerFlags().Expand());

		diskBarSizer->Add(diskBar, wxSizerFlags().Expand());
		diskBarSizer->Add(diskTxtSizer, wxSizerFlags().Expand());
	}

	auto compSizer = new wxGridBagSizer(4, 2);

	compSizer->Add(new wxStaticText(panel, wxID_ANY, "Method"), wxGBPosition(0, 0));
	compSizer->Add(new wxStaticText(panel, wxID_ANY, "Data"), wxGBPosition(0, 1));
	compSizer->Add(new wxStaticText(panel, wxID_ANY, "Data %"), wxGBPosition(0, 2));
	compSizer->Add(new wxStaticText(panel, wxID_ANY, "Storage"), wxGBPosition(0, 3));
	compSizer->Add(new wxStaticText(panel, wxID_ANY, "Storage %"), wxGBPosition(0, 4));
	compSizer->Add(new wxStaticText(panel, wxID_ANY, "Comp Ratio"), wxGBPosition(0, 5));

	compSizer->AddGrowableCol(0);
	compSizer->AddGrowableCol(1);
	compSizer->AddGrowableCol(2);
	compSizer->AddGrowableCol(3);
	compSizer->AddGrowableCol(4);
	compSizer->AddGrowableCol(5);

	for (int i = 0; i < ChunkFlagCompCount; ++i) {
		const char* compName;
		switch (i)
		{
		case 0:
			compName = "No Compression";
			break;
		case 1:
			compName = "Zstandard";
			break;
		case 2:
			compName = "Zlib";
			break;
		case 3:
			compName = "LZ4";
			break;
		case 4:
			compName = "Oodle";
			break;
		}

		compTexts[i][0] = new wxStaticText(panel, wxID_ANY, "0 B");
		compTexts[i][1] = new wxStaticText(panel, wxID_ANY, "0.00%");
		compTexts[i][2] = new wxStaticText(panel, wxID_ANY, "0 B");
		compTexts[i][3] = new wxStaticText(panel, wxID_ANY, "0.00%");
		compTexts[i][4] = new wxStaticText(panel, wxID_ANY, "100%");

		compSizer->Add(new wxStaticText(panel, wxID_ANY, compName), wxGBPosition(i + 1, 0));
		compSizer->Add(compTexts[i][0], wxGBPosition(i + 1, 1));
		compSizer->Add(compTexts[i][1], wxGBPosition(i + 1, 2));
		compSizer->Add(compTexts[i][2], wxGBPosition(i + 1, 3));
		compSizer->Add(compTexts[i][3], wxGBPosition(i + 1, 4));
		compSizer->Add(compTexts[i][4], wxGBPosition(i + 1, 5));
	}

	mainSizer->Add(storageBarSizer, wxSizerFlags().Expand().Border(wxUP | wxLEFT | wxRIGHT, 5));
	mainSizer->Add(downloadBarSizer, wxSizerFlags().Expand().Border(wxUP | wxLEFT | wxRIGHT, 5));
	mainSizer->Add(diskBarSizer, wxSizerFlags().Expand().Border(wxALL, 5));
	mainSizer->Add(new wxStaticLine(panel, wxID_ANY), wxSizerFlags().Expand().Border(wxALL, 5));
	mainSizer->Add(compSizer, wxSizerFlags().Expand().Border(wxALL, 5));

	panel->SetSizerAndFit(mainSizer);
	this->Fit();
	this->Show(true);

	Bind(wxEVT_CLOSE_WINDOW, &cStorage::OnClose, this);

	for (int i = 0; i < ChunkFlagCompCount; ++i) {
		compStats[i][0] = 0;
		compStats[i][1] = 0;
	}
	compMainStats[0] = 0;
	compMainStats[1] = 0;

	lastUpdate = std::chrono::steady_clock::now();
	std::thread([=, &build] {
		build->QueryChunks([this](const ChunkMetadata& meta, bool lastUpdate) {
			AddChunkData(meta, lastUpdate);
		}, flag, threadCount);
	}).detach();
}

cStorage::~cStorage()
{

}

void cStorage::AddChunkData(const ChunkMetadata& meta, bool isLast)
{
	if (!isLast) {
		chunkCount++;
		if (meta.Downloaded) {
			chunkDlCount++;

			int chunkI;
			switch (meta.Flags & ChunkFlagCompMask)
			{
			case ChunkFlagDecompressed:
				chunkI = 0;
				break;
			case ChunkFlagZstd:
				chunkI = 1;
				break;
			case ChunkFlagZlib:
				chunkI = 2;
				break;
			case ChunkFlagLZ4:
				chunkI = 3;
				break;
			case ChunkFlagOodle:
				chunkI = 4;
				break;
			default:
				return;
			}

			compStats[chunkI][0] += meta.Chunk->WindowSize;
			compMainStats[0] += meta.Chunk->WindowSize;
			compStats[chunkI][1] += meta.FileSize;
			compMainStats[1] += meta.FileSize;
		}

		auto now = std::chrono::steady_clock::now();
		ch::duration<float> duration = now - lastUpdate.load();
		if (duration.count() < .05f || isLast) {
			return;
		}
		lastUpdate = now;
	}

	for (int i = 0; i < ChunkFlagCompCount; ++i) {
		compTexts[i][0]->SetLabel(Stats::GetReadableSize(compStats[i][0]));
		compTexts[i][2]->SetLabel(Stats::GetReadableSize(compStats[i][1]));

		float decompP = compStats[i][0] * 100.f / compMainStats[0];
		float compP = compStats[i][1] * 100.f / compMainStats[1];
		float ratioP = compStats[i][0] ? compStats[i][1] * 100.f / compStats[i][0] : 100;
		compTexts[i][1]->SetLabel(wxString::Format("%.*f%%", (std::max)(2 - (int)floor(log10(decompP)), 0), decompP));
		compTexts[i][3]->SetLabel(wxString::Format("%.*f%%", (std::max)(2 - (int)floor(log10(compP)), 0), compP));
		compTexts[i][4]->SetLabel(wxString::Format("%.*f%%", (std::max)(2 - (int)floor(log10(ratioP)), 0), ratioP));
	}

	float storageP = compMainStats[1] * 100.f / compMainStats[0];
	storageBar->SetValue(round(storageP * 10));
	storageTxt->SetLabel(wxString::Format("%.*f%%", (std::max)(2 - (int)floor(log10(storageP)), 0), storageP));

	float downloadP = chunkDlCount * 100.f / chunkCount;
	downloadBar->SetValue(round(downloadP * 10));
	downloadTxt->SetLabel(wxString::Format("%.*f%%", (std::max)(2 - (int)floor(log10(downloadP)), 0), downloadP));
}

void cStorage::OnClose(wxCloseEvent& evt)
{
	flag.cancel();
	onClose();
}
