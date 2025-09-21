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

// Forward declarations
class AlarmFrame;
class AlarmTaskBarIcon;

class AlarmApp : public wxApp {
public:
    virtual bool OnInit();

private:
    wxLocale m_locale;
};

class SoundSettingsDialog : public wxDialog {
private:
    wxTextCtrl* nameCtrl;
    wxSlider* volumeSlider;
    std::map<std::string, wxSound*>& sounds;
    wxChoice* soundChoice;

public:
    SoundSettingsDialog(wxWindow* parent, std::map<std::string, wxSound*>& sounds, wxChoice* soundChoice)
        : wxDialog(parent, wxID_ANY, _("Sound Settings"), wxDefaultPosition, wxSize(400, 300)),
          sounds(sounds), soundChoice(soundChoice) {
        
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        SetBackgroundColour(wxColour(240, 240, 245));

        // Custom sound section
        wxStaticBoxSizer* customSoundBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Custom Sounds"));
        
        // Sound name input
        wxBoxSizer* nameSizer = new wxBoxSizer(wxHORIZONTAL);
        nameSizer->Add(new wxStaticText(this, wxID_ANY, _("Sound Name:")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        nameCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize);
        nameCtrl->SetHint(_("Enter sound name"));
        nameSizer->Add(nameCtrl, 1, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        customSoundBox->Add(nameSizer, 0, wxEXPAND | wxALL, 5);

        // Add sound button
        wxButton* addBtn = new wxButton(this, wxID_ANY, _("Add Sound"));
        addBtn->SetBackgroundColour(wxColour(0, 120, 215));
        addBtn->SetForegroundColour(*wxWHITE);
        customSoundBox->Add(addBtn, 0, wxEXPAND | wxALL, 5);

        mainSizer->Add(customSoundBox, 0, wxEXPAND | wxALL, 10);

        // Volume control
        wxStaticBoxSizer* volumeBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Volume"));
        volumeSlider = new wxSlider(this, wxID_ANY, 100, 0, 100, 
                                  wxDefaultPosition, wxDefaultSize,
                                  wxSL_HORIZONTAL | wxSL_LABELS);
        volumeBox->Add(volumeSlider, 0, wxEXPAND | wxALL, 5);

        wxButton* testBtn = new wxButton(this, wxID_ANY, _("Test Sound"));
        testBtn->SetBackgroundColour(wxColour(0, 120, 215));
        testBtn->SetForegroundColour(*wxWHITE);
        volumeBox->Add(testBtn, 0, wxEXPAND | wxALL, 5);

        mainSizer->Add(volumeBox, 0, wxEXPAND | wxALL, 10);

        // Close button
        wxButton* closeBtn = new wxButton(this, wxID_CLOSE, _("Close"));
        closeBtn->SetBackgroundColour(wxColour(220, 50, 50));
        closeBtn->SetForegroundColour(*wxWHITE);
        mainSizer->Add(closeBtn, 0, wxEXPAND | wxALL, 10);

        SetSizer(mainSizer);

        // Bind events
        addBtn->Bind(wxEVT_BUTTON, &SoundSettingsDialog::OnAddSound, this);
        testBtn->Bind(wxEVT_BUTTON, &SoundSettingsDialog::OnTestSound, this);
        closeBtn->Bind(wxEVT_BUTTON, &SoundSettingsDialog::OnClose, this);
    }

private:
    void OnAddSound(wxCommandEvent& event) {
        wxString name = nameCtrl->GetValue();
        if (name.IsEmpty()) {
            wxMessageBox(_("Please enter a sound name"), _("Error"), wxICON_ERROR);
            return;
        }

        if (sounds.find(name.ToStdString()) != sounds.end()) {
            wxMessageBox(_("Sound name already exists!"), _("Error"), wxICON_ERROR);
            return;
        }

        wxFileDialog openFileDialog(this, _("Choose Sound File"), "", "",
                                  "Sound files (*.wav)|*.wav", wxFD_OPEN|wxFD_FILE_MUST_EXIST);

        if (openFileDialog.ShowModal() == wxID_CANCEL)
            return;

        wxSound* newSound = new wxSound(openFileDialog.GetPath());
        if (!newSound->IsOk()) {
            delete newSound;
            wxMessageBox(_("Invalid sound file!"), _("Error"), wxICON_ERROR);
            return;
        }

        sounds[name.ToStdString()] = newSound;
        soundChoice->Append(name);
        nameCtrl->Clear();
        wxMessageBox(_("Sound added successfully!"), _("Success"), wxICON_INFORMATION);
    }

    void OnTestSound(wxCommandEvent& event) {
        wxString selected = soundChoice->GetString(soundChoice->GetSelection());
        if (sounds.find(selected.ToStdString()) != sounds.end()) {
            wxSound* sound = sounds[selected.ToStdString()];
            if (sound) {
                sound->Play(wxSOUND_ASYNC);
            }
        }
    }

    void OnClose(wxCommandEvent& event) {
        EndModal(wxID_CLOSE);
    }
};

class AlarmFrame : public wxFrame {
public:
    AlarmFrame(const wxString& title);
    friend class AlarmTaskBarIcon;

private:
    void OnSetAlarm(wxCommandEvent& event);
    void OnDeleteAlarm(wxCommandEvent& event);
    void OnCheckAlarm(wxTimerEvent& event);
    void OnClose(wxCloseEvent& event);
    void RefreshAlarmList();
    void OnIconize(wxIconizeEvent& event);
    void OnLanguageChange(wxCommandEvent& event);
    void RefreshUI();
    void OnSoundSettings(wxCommandEvent& event);
    void OnVolumeChange(wxCommandEvent& event);
    void OnTimeFormatChange(wxCommandEvent& event);
    wxString ConvertTo12Hour(const wxString& time24h, bool* isAM = nullptr);
    wxString ConvertTo24Hour(const wxString& time12h, bool isAM);
    ~AlarmFrame();

    wxTextCtrl* alarmTimeInput;
    wxListCtrl* alarmList;
    wxTimer* timer;
    sqlite3* db;
    wxButton* deleteButton;
    wxStaticText* currentTimeText;
    wxChoice* dayChoice;
    wxChoice* soundChoice;
    wxChoice* amPmChoice; // Add AM/PM choice
    wxSlider* volumeSlider;
    AlarmTaskBarIcon* m_taskBarIcon;
    std::map<std::string, wxSound*> sounds;
    wxPanel* mainPanel;  // Add panel as member

    void InitializeDatabase();
    void SaveAlarmToDatabase(const std::string& time, const std::string& day);
    void DeleteAlarmFromDatabase(const std::string& time);
    std::string GetCurrentTime();
    void UpdateCurrentTime(wxTimerEvent& event);
    void InitializeSounds();
    void PlayAlarmSound();
    void LoadSounds();
    std::string GetCurrentDayOfWeek();
    void CreateUI();

    wxLocale m_locale;

    // Security related members and methods
    bool isLocked;
    wxString hashedPassword;
    void OnLockApp(wxCommandEvent& event);
    void OnUnlockApp(wxCommandEvent& event);
    void OnChangePassword(wxCommandEvent& event);
    bool VerifyPassword(const wxString& password);
    wxString HashPassword(const wxString& password);
    void LockInterface();
    void UnlockInterface();
    void InitializeSecurity();

    // Secure database methods
    void EncryptDatabase();
    wxString SecureQuery(const wxString& query, const std::vector<wxString>& params);

    // Time format settings
    bool use24HourFormat;
};

class AlarmTaskBarIcon : public wxTaskBarIcon {
public:
    AlarmTaskBarIcon(AlarmFrame* frame) : m_frame(frame) {
        wxString exePath = wxStandardPaths::Get().GetExecutablePath();
        wxString iconPath = wxFileName(wxPathOnly(exePath) + "/../icons/alarm-clock.png").GetAbsolutePath();
        wxIcon icon;
        if (icon.LoadFile(iconPath, wxBITMAP_TYPE_PNG)) {
            SetIcon(icon, _("Desktop Alarm"));
        } else {
            wxIcon fallbackIcon;
            fallbackIcon.CopyFromBitmap(wxArtProvider::GetBitmap(wxART_WARNING, wxART_FRAME_ICON));
            SetIcon(fallbackIcon, _("Desktop Alarm"));
        }
    }

private:
    virtual wxMenu* CreatePopupMenu() {
        wxMenu* menu = new wxMenu;
        menu->Append(ID_SHOW, _("Show/Hide"));
        menu->AppendSeparator();
        menu->Append(ID_EXIT, _("Exit"));
        return menu;
    }

    void OnShowHide(wxCommandEvent&) {
        m_frame->Show(!m_frame->IsShown());
    }

    void OnExit(wxCommandEvent&) {
        m_frame->Close(true);
    }

    AlarmFrame* m_frame;
    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(AlarmTaskBarIcon, wxTaskBarIcon)
    EVT_MENU(ID_SHOW, AlarmTaskBarIcon::OnShowHide)
    EVT_MENU(ID_EXIT, AlarmTaskBarIcon::OnExit)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP(AlarmApp);

bool AlarmApp::OnInit() {
    // Create a professional-looking icon
    wxBitmap finalIcon(128, 128, 32);
    wxMemoryDC dc(finalIcon);
    
    // Set transparent background
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();
    
    // Create gradient background for icon
    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if (gc) {
        wxColour startColor(0, 120, 215);  // Professional blue
        wxColour endColor(0, 80, 170);     // Darker blue
        gc->SetBrush(gc->CreateLinearGradientBrush(0, 0, 128, 128, startColor, endColor));
        gc->DrawRoundedRectangle(0, 0, 128, 128, 20);
        delete gc;
    }
    
    // Draw clock circle
    dc.SetBrush(wxBrush(*wxWHITE));
    dc.SetPen(wxPen(*wxWHITE, 3));
    dc.DrawCircle(64, 64, 50);
    
    // Draw clock hands
    dc.SetPen(wxPen(*wxBLACK, 3));
    // Hour hand (pointing to 10)
    dc.DrawLine(64, 64, 64 - 25, 64 - 15);
    // Minute hand (pointing to 2)
    dc.DrawLine(64, 64, 64 + 35, 64 - 10);
    
    // Draw small bell in corner
    wxBitmap bellIcon = wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_OTHER, wxSize(32, 32));
    dc.DrawBitmap(bellIcon, 85, 85, true);
    
    dc.SelectObject(wxNullBitmap);
    
    // Save the icon
    wxString iconPath = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath() + "/icons/alarm-clock.png";
    finalIcon.SaveFile(iconPath, wxBITMAP_TYPE_PNG);
    
    // Set application icon
    wxIcon appIcon;
    appIcon.CopyFromBitmap(finalIcon);
    
    // Initialize language support
    m_locale.Init(wxLANGUAGE_DEFAULT);
    m_locale.AddCatalogLookupPathPrefix(wxT("locale"));
    m_locale.AddCatalog(wxT("messages"));

    AlarmFrame* frame = new AlarmFrame(_("Desktop Alarm"));
    frame->SetIcon(appIcon);
    frame->Show(true);
    return true;
}

AlarmFrame::AlarmFrame(const wxString& title)
    : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(500, 400)) {
    wxString exePath = wxStandardPaths::Get().GetExecutablePath();
    wxString iconPath = wxFileName(wxPathOnly(exePath) + "/../icons/alarm-clock.png").GetAbsolutePath();
    wxIcon frameIcon;
    if (frameIcon.LoadFile(iconPath, wxBITMAP_TYPE_PNG)) {
        SetIcon(frameIcon);
    }

    // Create menu bar with security options
    wxMenuBar *menuBar = new wxMenuBar;
    wxMenu *securityMenu = new wxMenu;
    securityMenu->Append(ID_LOCK, _("Lock Application"));
    securityMenu->Append(ID_UNLOCK, _("Unlock Application"));
    securityMenu->AppendSeparator();
    securityMenu->Append(ID_CHANGE_PASSWORD, _("Change Password"));
    menuBar->Append(securityMenu, _("Security"));

    // Add Settings menu
    wxMenu* settingsMenu = new wxMenu;
    settingsMenu->Append(ID_SOUND_SETTINGS, _("Sound Settings"));
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(ID_TIME_FORMAT, _("Use 24-hour format"));
    settingsMenu->Check(ID_TIME_FORMAT, true);  // Default to 24-hour
    menuBar->Append(settingsMenu, _("Settings"));

    SetMenuBar(menuBar);

    // Initialize mainPanel
    mainPanel = new wxPanel(this, wxID_ANY);
    CreateUI();

    // Timer Setup
    timer = new wxTimer(this);
    Bind(wxEVT_TIMER, &AlarmFrame::OnCheckAlarm, this);
    Bind(wxEVT_TIMER, &AlarmFrame::UpdateCurrentTime, this);
    timer->Start(1000); // Check every second

    // Bind security events
    Bind(wxEVT_MENU, &AlarmFrame::OnLockApp, this, ID_LOCK);
    Bind(wxEVT_MENU, &AlarmFrame::OnUnlockApp, this, ID_UNLOCK);
    Bind(wxEVT_MENU, &AlarmFrame::OnChangePassword, this, ID_CHANGE_PASSWORD);

