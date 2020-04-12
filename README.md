# EGL2
## An alternative to the Epic Games Launcher

## Features
 - Extremely quick updating
 - Or, download the update it in the background while playing! (Not recommended, but possible!)
 - Implements directly with your file system
 - Low memory footprint
 - Use your favorite datamining tools or extractors seamlessly
 - Trade performance for disk space, keep your game install compressed
 - Not added yet:	 
	 - Load multiple versions of games side by side

## Installation / Getting Started
The process can be a little awkward, so here's a simple step by step guide:
 1. Install [WinFsp](http://www.secfs.net/winfsp/), which can be downloaded [here](https://github.com/billziss-gh/winfsp/releases/download/v1.7B1/winfsp-1.7.20038.msi). (Only the "Core" feature is necessary)
 2. Download the latest install of EGL2 [here](https://github.com/WorkingRobot/EGL2/releases/latest/download/EGL2.exe).
 3. When ran, you should be greeted with a window with a several buttons. If instead you see an error, follow the instructions it shows.
 4. Click "Setup", where a new window will prompt you to setup where you want your data to be stored.
 5. If you want to be able to play the game (and not just download it), you will need to select a game directory as well. If you do not want to play, deselect the checkbox in the bottom right corner.
 6. By default, it is configured to download and install the game in a decompressed state. This will simply download and decompress your game to it's full size.
 7. Feel free to use the other compression methods at the expense of download/update time (due to compression). I recommend using "LZ4" and using the slowest compression level setting you feel comfortable with. LZ4 has decompression speeds that are extremely fast (~4 GB/s), and the slowest compression level can compress your install by over 2x. The slowest setting with LZ4 can compress ~40MB/s on a good computer, so do be conservative if your computer isn't great.
 8. Close the setup window when done.
 9. If the folders you chose in your setup exist, the other buttons should be enabled.
 10. Click "Update". This will take about an hour or so depending on the settings you chose and the computer you have. (You are downloading an entire game and possibly compressing it, too, y'know.)
 11. When your game finishes updating/installing, click "Start".
 12. If all goes well, you should be able to click "Play". Once you enter in your exchange code, Fortnite should start!
 13. When relaunching, it's recommended to click "Update" again, in the event that a new hotfix or update has occurred that you weren't aware of.
 14. You will need to relaunch EGL2 if an update was pushed after starting.

## Building
I use [CMake for Visual Studio](https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio), so your results may vary.

### Prerequisites
 - CMake
 - MSVC (with C++20 support)
 - vcpkg
	 - (Install these packages with `x64-windows-static`)
	 - OpenSSL
	 - RapidJSON
	 - zLib
	 - LZ4
 - wxWidgets (if compiling with a GUI)
 - WinFsp with the "Developer" feature installed

### CMake Build Options
 - `WITH_GUI` - if set to false, a command line version will be built instead. (Going to be honest, I haven't checked for any crashes/errors in the console version, but it should work)
 - `WX_DIR` - if you want to build EGL2 with a GUI, set this path to your wxWidgets directory.
