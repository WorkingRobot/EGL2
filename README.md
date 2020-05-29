# EGL2
## An alternative to the Epic Games Launcher

## Features
 - Extremely quick updating
 - Or, download the update it in the background while playing! (Not recommended, but possible!)
 - Implements directly with your file system
 - Low memory footprint
 - Use your favorite datamining tools or extractors seamlessly
 - Trade performance for disk space, keep your game install compressed

## Installation / Getting Started
### This setup is currently outdated. An updated version will be put up soon. Right now, all you need to know is that: **Your install folder needs to be empty! This is separate from what the official launcher has!**
The process can be a little awkward, so here's a simple step by step guide:
|Description|Guide (Click one to view it)|
|--|--|
|Install [WinFsp](http://www.secfs.net/winfsp/), which can be downloaded [here](https://github.com/billziss-gh/winfsp/releases/download/v1.7B1/winfsp-1.7.20038.msi). (Only the "Core" feature is necessary)|![WinFsp Installation](https://raw.githubusercontent.com/WorkingRobot/EGL2/master/docs/step1.gif)|
|Download the latest version of EGL2 [here](https://github.com/WorkingRobot/EGL2/releases/latest/download/EGL2.exe).|![Running EGL2](https://raw.githubusercontent.com/WorkingRobot/EGL2/master/docs/step2.gif)|
|When ran, you should be greeted with a window with several buttons. If instead you see an error, follow the instructions it shows.| |
|Click "Setup", where a new window will prompt you to setup where you want your data to be stored.|![Clicking Setup](https://raw.githubusercontent.com/WorkingRobot/EGL2/master/docs/step4.gif)|
|Create an **empty** folder where you want Fortnite to be installed. Set the "Install Folder" location to the folder you've just made.|![Setting the install folder](https://raw.githubusercontent.com/WorkingRobot/EGL2/master/docs/step5.gif)|
|By default, it is configured to download and install the game in a decompressed state. This will simply download and decompress your game to it's full size.| |
|Feel free to use the other compression methods at the expense of download/update time (due to compression). I recommend using "LZ4" and the slowest compression level setting you feel comfortable with. LZ4 has decompression speeds that are extremely fast (~4 GB/s), and the slowest compression level can compress your install by over 2x. The slowest setting with LZ4 can compress ~40MB/s on a good computer, so do be conservative if your computer isn't great.|![Setting compression options](https://raw.githubusercontent.com/WorkingRobot/EGL2/master/docs/step7.gif)|
|Close the setup window when you're done configuring.|![Closing Setup window](https://raw.githubusercontent.com/WorkingRobot/EGL2/master/docs/step8.gif)|If you set your install folder correctly, the other buttons should be enabled.| |
|Click "Update". This will take about an hour or two, depending on the settings you chose and the computer you have. (You are downloading an entire game and probably compressing it, too, y'know.)|![Click Update](https://raw.githubusercontent.com/WorkingRobot/EGL2/master/docs/step10.gif)|
|When your game finishes updating/installing, click "Start".|![Click Start](https://raw.githubusercontent.com/WorkingRobot/EGL2/master/docs/step11.gif)|
|If all goes well, you should be able to click "Play". Once you enter in your exchange code, Fortnite should start!|![Click Play](https://raw.githubusercontent.com/WorkingRobot/EGL2/master/docs/step12.gif)|
|When relaunching, it's recommended to click "Update" again, in case a new update released.| |

## Building
I use [CMake for Visual Studio](https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio), so your results may vary.

### Prerequisites
 - CMake
 - MSVC (with C++20 support)
 - vcpkg
     - (Install these packages with `x64-windows-static`)
     - OpenSSL
     - RapidJSON
     - lz4
     - zstd
     - curl
     - boost
 - wxWidgets
     - Make sure to set the subsystem from /MD to /MT when compiling
 - WinFsp with the "Developer" feature installed

### CMake Build Options
 - `WX_DIR` - Set this path to your wxWidgets directory.