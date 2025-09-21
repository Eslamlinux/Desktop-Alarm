#include <iostream>
#include <wx/wx.h>
#include <wx/timer.h>
#include <wx/listctrl.h>
#include <wx/datetime.h>
#include <wx/taskbar.h>
#include <wx/sound.h>
#include <wx/choice.h>
#include <wx/artprov.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/translation.h>
#include <wx/settings.h>
#include <wx/statbmp.h>
#include <wx/slider.h>
#include <wx/graphics.h>
#include <sqlite3.h>
#include <vector>
#include <map>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/aes.h>

using namespace std;

// Add these before the AlarmApp class
enum {
    ID_SHOW = wxID_HIGHEST + 1,
    ID_EXIT,
    ID_LOCK,
    ID_UNLOCK,
    ID_CHANGE_PASSWORD,
    ID_SOUND_SETTINGS,
    ID_VOLUME_SLIDER,
    ID_TIME_FORMAT = wxID_HIGHEST + 100
};

