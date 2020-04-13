#pragma once

// Symlinks can only be made if:
// - The program is in administrator mode
// - Developer mode is active (Win10 only)
// - Local Security Policy (secpol.msc): Local Policies -> User Rights Assignment -> Create symbolic links: must include the user's group/SID here (ties into first bullet point)
//
// A terrible security workaround, but I'm unsure if there is any other way around it.

// Implementation of workaround:
// if (!IsDeveloperModeEnabled()) {
//     if (!IsUserAdmin()) {
//         Message("Please restart the program in administator mode!");
//     }
//     else {
//         EnableDeveloperMode();
//     }
// }


bool IsUserAdmin();

bool IsDeveloperModeEnabled();

bool EnableDeveloperMode();