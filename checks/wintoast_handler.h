#pragma once

#include <functional>
#include <wintoastlib.h>

using namespace WinToastLib;

typedef std::function<void(int actionIndex)> ToastHandlerClicked;
typedef std::function<void(IWinToastHandler::WinToastDismissalReason state)> ToastHandlerDismissed;

class ToastHandler : public IWinToastHandler {
public:
	ToastHandler(ToastHandlerClicked clicked, ToastHandlerDismissed dismissed) :
		Clicked(clicked), Dismissed(dismissed) { }

	void toastActivated() const override { Clicked(-1); }
	void toastActivated(int actionIndex) const override { Clicked(actionIndex); }
	void toastDismissed(IWinToastHandler::WinToastDismissalReason state) const override { Dismissed(state); }
	void toastFailed() const override { }

private:
	ToastHandlerClicked Clicked;
	ToastHandlerDismissed Dismissed;
};