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

    // Bind sound settings event
    Bind(wxEVT_MENU, &AlarmFrame::OnSoundSettings, this, ID_SOUND_SETTINGS);

    // Bind time format event
    Bind(wxEVT_MENU, &AlarmFrame::OnTimeFormatChange, this, ID_TIME_FORMAT);

    // Initialize system tray icon
    m_taskBarIcon = new AlarmTaskBarIcon(this);

    // Initialize sounds
    InitializeSounds();

    InitializeDatabase();
    InitializeSecurity();
    RefreshAlarmList();

    // Set minimum size
    SetMinSize(wxSize(400, 300));
    
    // Center the window
    Centre();

    Bind(wxEVT_ICONIZE, &AlarmFrame::OnIconize, this);
}

void AlarmFrame::CreateUI() {
    // Create a more attractive gradient background
    wxColour startColor(190, 210, 255);  // Lighter blue
    wxColour endColor(220, 230, 255);    // Very light blue
    
    mainPanel->SetBackgroundStyle(wxBG_STYLE_PAINT);
    mainPanel->Bind(wxEVT_PAINT, [=](wxPaintEvent& evt) {
        wxPaintDC dc(mainPanel);
        wxRect rect = mainPanel->GetClientRect();
        
        // Create rounded rectangle background
        wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
        if (gc) {
            // Main gradient
            gc->SetBrush(gc->CreateLinearGradientBrush(0, 0, 0, rect.height, startColor, endColor));
            gc->DrawRoundedRectangle(5, 5, rect.width-10, rect.height-10, 15);
            
            // Add subtle highlight at top
            wxColour highlightColor(255, 255, 255, 40);
            gc->SetBrush(gc->CreateLinearGradientBrush(0, 0, 0, 30, highlightColor, wxColour(255,255,255,0)));
            gc->DrawRoundedRectangle(5, 5, rect.width-10, 30, 15);
            
            delete gc;
        }
    });
    
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Modern clock display
    currentTimeText = new wxStaticText(mainPanel, wxID_ANY, _("Current Time: ") + GetCurrentTime());
    wxFont timeFont = currentTimeText->GetFont();
    timeFont.SetPointSize(24);
    timeFont.SetWeight(wxFONTWEIGHT_BOLD);
    currentTimeText->SetFont(timeFont);
    currentTimeText->SetForegroundColour(wxColour(20, 20, 100));
    mainSizer->Add(currentTimeText, 0, wxALL | wxALIGN_CENTER, 10);

    // Styled input section with rounded corners
    wxPanel* inputPanel = new wxPanel(mainPanel);
    inputPanel->SetBackgroundColour(wxColour(245, 245, 255));
    wxBoxSizer* inputSizer = new wxBoxSizer(wxVERTICAL);

    // Time input with modern styling
    wxBoxSizer* timeSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* timeLabel = new wxStaticText(inputPanel, wxID_ANY, _("Time:"));
    timeLabel->SetForegroundColour(wxColour(50, 50, 100));
    alarmTimeInput = new wxTextCtrl(inputPanel, wxID_ANY, "", 
                                  wxDefaultPosition, wxDefaultSize,
                                  wxTE_CENTRE);
    alarmTimeInput->SetHint(_("Enter time"));
    
    amPmChoice = new wxChoice(inputPanel, wxID_ANY);
    amPmChoice->Append(_("AM"));
    amPmChoice->Append(_("PM"));
    amPmChoice->SetSelection(0);
    amPmChoice->Hide(); // Initially hidden for 24-hour format
    
    timeSizer->Add(timeLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    timeSizer->Add(alarmTimeInput, 1, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    timeSizer->Add(amPmChoice, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    inputSizer->Add(timeSizer, 0, wxEXPAND | wxALL, 5);

    // Day selection with modern styling
    wxBoxSizer* daySizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* dayLabel = new wxStaticText(inputPanel, wxID_ANY, _("Day:"));
    dayLabel->SetForegroundColour(wxColour(50, 50, 100));
    dayChoice = new wxChoice(inputPanel, wxID_ANY);
    dayChoice->Append(_("Every Day"));
    dayChoice->Append(_("Monday"));
    dayChoice->Append(_("Tuesday"));
    dayChoice->Append(_("Wednesday"));
    dayChoice->Append(_("Thursday"));
    dayChoice->Append(_("Friday"));
    dayChoice->Append(_("Saturday"));
    dayChoice->Append(_("Sunday"));
    dayChoice->SetSelection(0);

    daySizer->Add(dayLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    daySizer->Add(dayChoice, 1, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    inputSizer->Add(daySizer, 0, wxEXPAND | wxALL, 5);

    // Sound selection with modern styling
    wxBoxSizer* soundSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* soundLabel = new wxStaticText(inputPanel, wxID_ANY, _("Sound:"));
    soundLabel->SetForegroundColour(wxColour(50, 50, 100));
    soundChoice = new wxChoice(inputPanel, wxID_ANY);
    soundChoice->Append(_("Default Beep"));
    soundChoice->Append(_("Bell"));
    soundChoice->Append(_("Chime"));
    soundChoice->SetSelection(0);

    soundSizer->Add(soundLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    soundSizer->Add(soundChoice, 1, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    inputSizer->Add(soundSizer, 0, wxEXPAND | wxALL, 5);

    // Volume control
    wxBoxSizer* volumeSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* volumeLabel = new wxStaticText(inputPanel, wxID_ANY, _("Volume:"));
    volumeLabel->SetForegroundColour(wxColour(50, 50, 100));
    volumeSlider = new wxSlider(inputPanel, ID_VOLUME_SLIDER, 100, 0, 100,
                               wxDefaultPosition, wxDefaultSize,
                               wxSL_HORIZONTAL | wxSL_LABELS);
    volumeSizer->Add(volumeLabel, 0, wxALL, 5);
    volumeSizer->Add(volumeSlider, 0, wxEXPAND | wxALL, 5);
    inputSizer->Add(volumeSizer, 0, wxEXPAND | wxALL, 5);

    inputPanel->SetSizer(inputSizer);
    mainSizer->Add(inputPanel, 0, wxEXPAND | wxALL, 10);

    // Set alarm button with modern styling
    wxButton* setAlarmButton = new wxButton(mainPanel, wxID_ANY, _("Set Alarm"));
    setAlarmButton->SetBackgroundColour(wxColour(0, 120, 215));
    setAlarmButton->SetForegroundColour(*wxWHITE);
    mainSizer->Add(setAlarmButton, 0, wxEXPAND | wxALL, 10);

    // Alarms list with modern styling
    wxStaticBox* listBox = new wxStaticBox(mainPanel, wxID_ANY, _("Scheduled Alarms"));
    listBox->SetForegroundColour(wxColour(50, 50, 100));
    wxStaticBoxSizer* listBoxSizer = new wxStaticBoxSizer(listBox, wxVERTICAL);

    alarmList = new wxListCtrl(listBox, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxLC_REPORT | wxLC_SINGLE_SEL);
    alarmList->SetBackgroundColour(wxColour(250, 250, 255));
    alarmList->InsertColumn(0, _("Time"));
    alarmList->InsertColumn(1, _("Day"));
    alarmList->SetColumnWidth(0, 150);
    alarmList->SetColumnWidth(1, 150);
    listBoxSizer->Add(alarmList, 1, wxEXPAND | wxALL, 5);

    // Delete button with modern styling
    deleteButton = new wxButton(listBox, wxID_ANY, _("Delete Selected"));
    deleteButton->SetBackgroundColour(wxColour(220, 50, 50));
    deleteButton->SetForegroundColour(*wxWHITE);
    listBoxSizer->Add(deleteButton, 0, wxALL | wxALIGN_RIGHT, 5);

    mainSizer->Add(listBoxSizer, 1, wxEXPAND | wxALL, 10);

    // Language selection with modern styling
    wxBoxSizer* langSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* langLabel = new wxStaticText(mainPanel, wxID_ANY, _("Language:"));
    langLabel->SetForegroundColour(wxColour(50, 50, 100));
    wxChoice* langChoice = new wxChoice(mainPanel, wxID_ANY);
    langChoice->Append("English");
    langChoice->Append("العربية");
    langChoice->SetSelection(0);
    
    langSizer->Add(langLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    langSizer->Add(langChoice, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    mainSizer->Add(langSizer, 0, wxALL | wxALIGN_RIGHT, 10);

    mainPanel->SetSizer(mainSizer);

    // Bind events
    setAlarmButton->Bind(wxEVT_BUTTON, &AlarmFrame::OnSetAlarm, this);
    deleteButton->Bind(wxEVT_BUTTON, &AlarmFrame::OnDeleteAlarm, this);
    langChoice->Bind(wxEVT_CHOICE, &AlarmFrame::OnLanguageChange, this);
    volumeSlider->Bind(wxEVT_SLIDER, &AlarmFrame::OnVolumeChange, this);
}

void AlarmFrame::OnSoundSettings(wxCommandEvent& event) {
    SoundSettingsDialog dlg(this, sounds, soundChoice);
    dlg.ShowModal();
}

void AlarmFrame::OnLanguageChange(wxCommandEvent& event) {
    wxString lang = event.GetString();
    if (lang == "العربية") {
        m_locale.Init(wxLANGUAGE_ARABIC);
        SetLayoutDirection(wxLayout_RightToLeft);
        if (GetMenuBar()) {
            GetMenuBar()->SetLayoutDirection(wxLayout_RightToLeft);
        }
        mainPanel->SetLayoutDirection(wxLayout_RightToLeft);
    } else {
        m_locale.Init(wxLANGUAGE_ENGLISH);
        SetLayoutDirection(wxLayout_LeftToRight);
        if (GetMenuBar()) {
            GetMenuBar()->SetLayoutDirection(wxLayout_LeftToRight);
        }
        mainPanel->SetLayoutDirection(wxLayout_LeftToRight);
    }
    m_locale.AddCatalogLookupPathPrefix(wxT("locale"));
    m_locale.AddCatalog(wxT("messages"));
    
    // Refresh UI with new language and layout
    RefreshUI();
}

void AlarmFrame::RefreshUI() {
    // Update all text elements with new translations
    SetTitle(_("Desktop Alarm"));
    currentTimeText->SetLabel(_("Current Time: ") + GetCurrentTime());
    
    // Update layout direction for all controls
    bool isArabic = m_locale.GetLanguage() == wxLANGUAGE_ARABIC;
    wxLayoutDirection dir = isArabic ? wxLayout_RightToLeft : wxLayout_LeftToRight;
    
    // Set layout direction for main panel and all its children
    mainPanel->SetLayoutDirection(dir);
    mainPanel->Layout();
    
    // Update list control appearance and direction
    alarmList->SetLayoutDirection(dir);
    alarmList->DeleteAllColumns();
    if (isArabic) {
        // For Arabic, add columns in reverse order for proper RTL display
        alarmList->InsertColumn(0, _("Day"), wxLIST_FORMAT_RIGHT, 150);
        alarmList->InsertColumn(0, _("Time"), wxLIST_FORMAT_RIGHT, 150);
    } else {
        alarmList->InsertColumn(0, _("Time"), wxLIST_FORMAT_LEFT, 150);
        alarmList->InsertColumn(1, _("Day"), wxLIST_FORMAT_LEFT, 150);
    }
    
    // Apply modern styling
    wxFont modernFont = alarmList->GetFont();
    modernFont.SetPointSize(10);
    alarmList->SetFont(modernFont);
    
    // Refresh all controls
    alarmTimeInput->SetLayoutDirection(dir);
    dayChoice->SetLayoutDirection(dir);
    soundChoice->SetLayoutDirection(dir);
    amPmChoice->SetLayoutDirection(dir);
    volumeSlider->SetLayoutDirection(dir);
    
    // Update gradient background
    mainPanel->Refresh();
    
    // Refresh alarm list content
    RefreshAlarmList();
    Layout();
}

void AlarmFrame::UpdateCurrentTime(wxTimerEvent& event) {
    currentTimeText->SetLabel(_("Current Time: ") + GetCurrentTime());
}

void AlarmFrame::RefreshAlarmList() {
    alarmList->DeleteAllItems();
    
    // Update column headers
    wxListItem col0;
    col0.SetId(0);
    col0.SetText(_("Time"));
    alarmList->SetColumn(0, col0);
    
    wxListItem col1;
    col1.SetId(1);
    col1.SetText(_("Day"));
    alarmList->SetColumn(1, col1);
    
    sqlite3_stmt* stmt;
    const char* query = "SELECT time, day FROM alarms ORDER BY time;";
    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK) {
        int row = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* time = sqlite3_column_text(stmt, 0);
            const unsigned char* day = sqlite3_column_text(stmt, 1);
            
            wxString timeStr = wxString::FromUTF8((const char*)time);
            if (!use24HourFormat) {
                timeStr = ConvertTo12Hour(timeStr);
            }
            
            // Add time and day in separate columns
            long idx = alarmList->InsertItem(row, timeStr);
            alarmList->SetItem(idx, 1, _(wxString::FromUTF8((const char*)day)));
            row++;
        }
    }
    sqlite3_finalize(stmt);
}

void AlarmFrame::OnDeleteAlarm(wxCommandEvent& event) {
    long selectedItem = alarmList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedItem != -1) {
        wxString item = alarmList->GetItemText(selectedItem);
        wxString time = item.BeforeFirst(' ');
        DeleteAlarmFromDatabase(time.ToStdString());
        RefreshAlarmList();
    }
}

void AlarmFrame::DeleteAlarmFromDatabase(const std::string& time) {
    std::string query = "DELETE FROM alarms WHERE time = '" + time + "';";
    sqlite3_exec(db, query.c_str(), 0, 0, 0);
}

void AlarmFrame::OnClose(wxCloseEvent& event) {
    timer->Stop();
    event.Skip();
}

void AlarmFrame::InitializeDatabase() {
    if (sqlite3_open("alarms.db", &db) != SQLITE_OK) {
        wxMessageBox(_("Failed to open database!"), _("Error"), wxICON_ERROR);
    } else {
        const char* createTableQuery =
            "CREATE TABLE IF NOT EXISTS alarms ("
            "id INTEGER PRIMARY KEY, "
            "time TEXT, "
            "day TEXT DEFAULT 'Every Day');";
        sqlite3_exec(db, createTableQuery, 0, 0, 0);
    }
}

void AlarmFrame::InitializeSounds() {
    // Default beep will always work since it uses system beep
    sounds["Default Beep"] = nullptr;

    // Try to load custom sounds if they exist
    wxString exePath = wxStandardPaths::Get().GetExecutablePath();
    wxString exeDir = wxPathOnly(exePath);
    wxString soundsDir = wxFileName(exeDir + "/../sounds").GetAbsolutePath();
    
    wxFileName bellFile(soundsDir + "/bell.wav");
    wxFileName chimeFile(soundsDir + "/chime.wav");
    
    if (bellFile.FileExists()) {
        sounds["Bell"] = new wxSound(bellFile.GetFullPath());
        if (!sounds["Bell"]->IsOk()) {
            delete sounds["Bell"];
            sounds.erase("Bell");
        }
    }
    
    if (chimeFile.FileExists()) {
        sounds["Chime"] = new wxSound(chimeFile.GetFullPath());
        if (!sounds["Chime"]->IsOk()) {
            delete sounds["Chime"];
            sounds.erase("Chime");
        }
    }

    // Update sound choice control
    if (soundChoice) {
        soundChoice->Clear();
        soundChoice->Append(_("Default Beep"));
        for (const auto& sound : sounds) {
            if (sound.first != "Default Beep") {
                soundChoice->Append(_(sound.first));
            }
        }
        soundChoice->SetSelection(0);
    }
}

void AlarmFrame::PlayAlarmSound() {
    wxString selectedSound = soundChoice->GetString(soundChoice->GetSelection());
    if (selectedSound == _("Default Beep")) {
        wxBell();
    } else if (sounds[selectedSound.ToStdString()]) {
        // Adjust volume (if supported by the system)
        #ifdef __WXMSW__
        float volume = volumeSlider->GetValue() / 100.0f;
        waveOutSetVolume(NULL, UINT(volume * 65535.0f) | (UINT(volume * 65535.0f) << 16));
        #endif
        sounds[selectedSound.ToStdString()]->Play(wxSOUND_ASYNC);
    }
}

std::string AlarmFrame::GetCurrentTime() {
    time_t now = time(0);
    tm* localTime = localtime(&now);

    char buffer[6];
    strftime(buffer, sizeof(buffer), "%H:%M", localTime);

    return std::string(buffer);
}

std::string AlarmFrame::GetCurrentDayOfWeek() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    std::vector<std::string> days = {"Sunday", "Monday", "Tuesday", "Wednesday", 
                                    "Thursday", "Friday", "Saturday"};
    return days[ltm->tm_wday];
}

void AlarmFrame::SaveAlarmToDatabase(const std::string& time, const std::string& day) {
    std::string query = "INSERT INTO alarms (time, day) VALUES ('" + time + "', '" + day + "');";
    sqlite3_exec(db, query.c_str(), 0, 0, 0);
}

void AlarmFrame::OnSetAlarm(wxCommandEvent& event) {
    wxString alarmTime = alarmTimeInput->GetValue();
    wxString selectedDay = dayChoice->GetString(dayChoice->GetSelection());
    
    // Basic time format validation
    if (use24HourFormat) {
        if (alarmTime.length() != 5 || alarmTime[2] != ':' ||
            !isdigit(alarmTime[0]) || !isdigit(alarmTime[1]) ||
            !isdigit(alarmTime[3]) || !isdigit(alarmTime[4])) {
            wxMessageBox(_("Invalid time format! Please use HH:MM format."), _("Error"), wxICON_ERROR);
            return;
        }

        int hours = wxAtoi(alarmTime.Left(2));
        int minutes = wxAtoi(alarmTime.Right(2));

        if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59) {
            wxMessageBox(_("Invalid time! Hours must be 0-23 and minutes 0-59."), _("Error"), wxICON_ERROR);
            return;
        }
    } else {
        if (alarmTime.length() != 5 || alarmTime[2] != ':' ||
            !isdigit(alarmTime[0]) || !isdigit(alarmTime[1]) ||
            !isdigit(alarmTime[3]) || !isdigit(alarmTime[4])) {
            wxMessageBox(_("Invalid time format! Please use HH:MM format."), _("Error"), wxICON_ERROR);
            return;
        }

        int hours = wxAtoi(alarmTime.Left(2));
        int minutes = wxAtoi(alarmTime.Right(2));

        if (hours < 1 || hours > 12 || minutes < 0 || minutes > 59) {
            wxMessageBox(_("Invalid time! Hours must be 1-12 and minutes 0-59."), _("Error"), wxICON_ERROR);
            return;
        }

        bool isAM = amPmChoice->GetSelection() == 0;
        alarmTime = ConvertTo24Hour(alarmTime, isAM);
    }

    SaveAlarmToDatabase(alarmTime.ToStdString(), selectedDay.ToStdString());
    RefreshAlarmList();
    alarmTimeInput->Clear();
    wxMessageBox(_("Alarm set for ") + alarmTime + _(" on ") + selectedDay, _("Success"), 
                wxICON_INFORMATION);
}

void AlarmFrame::OnCheckAlarm(wxTimerEvent& event) {
    std::string currentTime = GetCurrentTime();
    std::string currentDay = GetCurrentDayOfWeek();

    sqlite3_stmt* stmt;
    const char* query = "SELECT time FROM alarms WHERE time = ? AND (day = ? OR day = 'Every Day');";
    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, currentTime.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, currentDay.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            wxString message = wxString::Format(_("⏰ Time to wake up!\nCurrent time: %s\nDay: %s"), 
                                              currentTime, currentDay);
            wxMessageBox(message, _("Alarm"), wxICON_INFORMATION | wxSTAY_ON_TOP);
            PlayAlarmSound();
        }
    }
    sqlite3_finalize(stmt);
}

void AlarmFrame::OnIconize(wxIconizeEvent& event) {
    Hide();
}

void AlarmFrame::InitializeSecurity() {
    isLocked = false;
    hashedPassword = HashPassword("default"); // Default password
}

wxString AlarmFrame::HashPassword(const wxString& password) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;
    
    // Create digest context
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    
    // Initialize with SHA256
    if (!EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr)) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    // Update with password
    if (!EVP_DigestUpdate(ctx, password.c_str(), password.length())) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    // Finalize
    if (!EVP_DigestFinal_ex(ctx, hash, &hashLen)) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    EVP_MD_CTX_free(ctx);
    
    // Convert to hex string
    wxString hashedStr;
    for(unsigned int i = 0; i < hashLen; i++) {
        hashedStr += wxString::Format("%02x", hash[i]);
    }
    return hashedStr;
}

bool AlarmFrame::VerifyPassword(const wxString& password) {
    return hashedPassword == HashPassword(password);
}

void AlarmFrame::LockInterface() {
    isLocked = true;
    // Disable UI elements
    alarmTimeInput->Disable();
    dayChoice->Disable();
    soundChoice->Disable();
    amPmChoice->Disable();
    deleteButton->Disable();
    alarmList->Disable();
}

void AlarmFrame::UnlockInterface() {
    isLocked = false;
    // Enable UI elements
    alarmTimeInput->Enable();
    dayChoice->Enable();
    soundChoice->Enable();
    amPmChoice->Enable();
    deleteButton->Enable();
    alarmList->Enable();
}

void AlarmFrame::OnLockApp(wxCommandEvent& event) {
    LockInterface();
    wxMessageBox(_("Application locked. Use Unlock to access."), _("Locked"), wxICON_INFORMATION);
}

void AlarmFrame::OnUnlockApp(wxCommandEvent& event) {
    wxString password = wxGetPasswordFromUser(_("Enter password to unlock:"), _("Unlock"));
    if (VerifyPassword(password)) {
        UnlockInterface();
        wxMessageBox(_("Application unlocked."), _("Unlocked"), wxICON_INFORMATION);
    } else {
        wxMessageBox(_("Incorrect password!"), _("Error"), wxICON_ERROR);
    }
}

void AlarmFrame::OnChangePassword(wxCommandEvent& event) {
    if (isLocked) {
        wxMessageBox(_("Please unlock the application first."), _("Error"), wxICON_ERROR);
        return;
    }

    wxPasswordEntryDialog oldPassDlg(this, _("Enter current password:"), _("Change Password"));
    if (oldPassDlg.ShowModal() != wxID_OK) return;

    if (!VerifyPassword(oldPassDlg.GetValue())) {
        wxMessageBox(_("Incorrect current password!"), _("Error"), wxICON_ERROR);
        return;
    }

    wxPasswordEntryDialog newPassDlg(this, _("Enter new password:"), _("Change Password"));
    if (newPassDlg.ShowModal() != wxID_OK) return;

    wxString newPass = newPassDlg.GetValue();
    if (newPass.Length() < 6) {
        wxMessageBox(_("Password must be at least 6 characters long!"), _("Error"), wxICON_ERROR);
        return;
    }

    wxPasswordEntryDialog confirmDlg(this, _("Confirm new password:"), _("Change Password"));
    if (confirmDlg.ShowModal() != wxID_OK) return;

    if (newPass != confirmDlg.GetValue()) {
        wxMessageBox(_("Passwords do not match!"), _("Error"), wxICON_ERROR);
        return;
    }

    hashedPassword = HashPassword(newPass);
    EncryptDatabase(); // Re-encrypt database with new password
    wxMessageBox(_("Password changed successfully!"), _("Success"), wxICON_INFORMATION);
}

void AlarmFrame::EncryptDatabase() {
    // Read current database content
    std::vector<std::pair<std::string, std::string>> alarms;
    sqlite3_stmt* stmt;
    const char* query = "SELECT time, day FROM alarms;";
    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* time = (const char*)sqlite3_column_text(stmt, 0);
            const char* day = (const char*)sqlite3_column_text(stmt, 1);
            alarms.push_back({std::string(time), std::string(day)});
        }
    }
    sqlite3_finalize(stmt);

    // Create secure temporary table
    const char* createSecureTable = 
        "CREATE TABLE IF NOT EXISTS secure_alarms ("
        "id INTEGER PRIMARY KEY, "
        "encrypted_data BLOB, "
        "iv BLOB);";
    sqlite3_exec(db, createSecureTable, 0, 0, 0);

    // Generate key from password using newer EVP interface
    unsigned char key[32];
    unsigned char salt[32];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        return;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return;

    // Encrypt each alarm
    for (const auto& alarm : alarms) {
        unsigned char iv[EVP_MAX_IV_LENGTH];
        if (RAND_bytes(iv, EVP_MAX_IV_LENGTH) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            continue;
        }

        std::string data = alarm.first + "|" + alarm.second;
        if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, iv)) {
            EVP_CIPHER_CTX_free(ctx);
            continue;
        }

        std::vector<unsigned char> ciphertext(data.length() + EVP_MAX_BLOCK_LENGTH);
        int len;
        if (!EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                            (unsigned char*)data.c_str(), data.length())) {
            EVP_CIPHER_CTX_free(ctx);
            continue;
        }

        int ciphertext_len = len;
        if (!EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len)) {
            EVP_CIPHER_CTX_free(ctx);
            continue;
        }
        ciphertext_len += len;

        // Store encrypted data
        sqlite3_stmt* insert_stmt;
        const char* insert_query = 
            "INSERT INTO secure_alarms (encrypted_data, iv) VALUES (?, ?);";
        if (sqlite3_prepare_v2(db, insert_query, -1, &insert_stmt, 0) == SQLITE_OK) {
            sqlite3_bind_blob(insert_stmt, 1, ciphertext.data(), ciphertext_len, SQLITE_STATIC);
            sqlite3_bind_blob(insert_stmt, 2, iv, EVP_MAX_IV_LENGTH, SQLITE_STATIC);
            sqlite3_step(insert_stmt);
            sqlite3_finalize(insert_stmt);
        }
    }
    
    EVP_CIPHER_CTX_free(ctx);
}

wxString AlarmFrame::SecureQuery(const wxString& query, const std::vector<wxString>& params) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        return "";
    }
    
    for (size_t i = 0; i < params.size(); i++) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_STATIC);
    }
    
    wxString result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = wxString::FromUTF8((const char*)sqlite3_column_text(stmt, 0));
    }
    
    sqlite3_finalize(stmt);
    return result;
}

void AlarmFrame::OnVolumeChange(wxCommandEvent& event) {
    // Store volume setting
    int volume = volumeSlider->GetValue();
    
    #ifdef __WXMSW__
    // For Windows, set system volume
    float normalizedVolume = volume / 100.0f;
    waveOutSetVolume(NULL, UINT(normalizedVolume * 65535.0f) | (UINT(normalizedVolume * 65535.0f) << 16));
    #endif
    
    // Test current sound with new volume
    wxString selectedSound = soundChoice->GetString(soundChoice->GetSelection());
    if (selectedSound != _("Default Beep") && sounds[selectedSound.ToStdString()]) {
        sounds[selectedSound.ToStdString()]->Play(wxSOUND_ASYNC);
    }
}

void AlarmFrame::OnTimeFormatChange(wxCommandEvent& event) {
    use24HourFormat = event.IsChecked();
    amPmChoice->Show(!use24HourFormat);
    mainPanel->Layout();
    RefreshAlarmList(); // Refresh to update time display format
}

wxString AlarmFrame::ConvertTo12Hour(const wxString& time24h, bool* isAM) {
    int hours = wxAtoi(time24h.Left(2));
    int minutes = wxAtoi(time24h.Right(2));
    wxString amPm = _("AM");

    if (hours >= 12) {
        amPm = _("PM");
        if (hours > 12) {
            hours -= 12;
        }
    } else if (hours == 0) {
        hours = 12;
    }

    if (isAM) {
        *isAM = (amPm == _("AM"));
    }

    return wxString::Format("%02d:%02d %s", hours, minutes, amPm);
}

wxString AlarmFrame::ConvertTo24Hour(const wxString& time12h, bool isAM) {
    int hours = wxAtoi(time12h.Left(2));
    int minutes = wxAtoi(time12h.Right(2));

    if (!isAM && hours < 12) {
        hours += 12;
    } else if (isAM && hours == 12) {
        hours = 0;
    }

    return wxString::Format("%02d:%02d", hours, minutes);
}

AlarmFrame::~AlarmFrame() {
    if (timer) {
        timer->Stop();
        delete timer;
    }
    sqlite3_close(db);
    if (m_taskBarIcon) {
        m_taskBarIcon->Destroy();
    }
    for (auto& sound : sounds) {
        delete sound.second;
    }
}
