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


}
