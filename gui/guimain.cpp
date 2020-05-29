#include "cApp.h"

//#define USE_CONSOLE

#ifdef USE_CONSOLE
wxIMPLEMENT_APP_CONSOLE(cApp);
#else
wxIMPLEMENT_APP(cApp);
#endif