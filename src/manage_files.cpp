#include "sn_file_tree.h"

class MyApp : public wxApp
{
public:
    virtual bool OnInit();
};
class FileManagerFrame : public wxFrame
{
public:
    FileManagerFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
private:

    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();

    void OnDeviceSelect(wxCommandEvent& event);

    void OnRefreshButton(wxCommandEvent& event);

    void RefreshUris();

    SNIConnection sni;
    std::vector<std::string> uris_;
    std::string current_uri_;

    SNFileTree* treeCtrl_;
    wxComboBox* devicesDropdown_;
};

enum
{
    ID_Hello = 1
};
wxBEGIN_EVENT_TABLE(FileManagerFrame, wxFrame)

EVT_MENU(wxID_EXIT, FileManagerFrame::OnExit)
EVT_MENU(wxID_ABOUT, FileManagerFrame::OnAbout)
wxEND_EVENT_TABLE()
wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    wxInitAllImageHandlers();

    FileManagerFrame* frame = new FileManagerFrame("Super Nintendo File Manager", wxPoint(50, 50), wxSize(450, 340));
    frame->Show(true);


    return true;
}


FileManagerFrame::FileManagerFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
    : wxFrame(NULL, wxID_ANY, title, pos, size)
{
    wxMenu* menuFile = new wxMenu;
    //menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
    //    "Help string shown in status bar for this menu item");
    //menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);
    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);
    CreateStatusBar();




    wxBoxSizer* bigBox = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* refreshBox = new wxBoxSizer(wxHORIZONTAL);

    bigBox->Add(refreshBox, 0);

    wxButton* button = new wxButton(this, wxID_ANY, L"🗘", wxDefaultPosition, wxSize(25, 25));

    devicesDropdown_ = new wxComboBox(this, wxID_ANY, "", wxDefaultPosition, wxSize(2500, 25), {}, wxCB_READONLY);


    refreshBox->Add(devicesDropdown_, 1, wxEXPAND);

    refreshBox->Add(button);

    treeCtrl_ = new SNFileTree(this, current_uri_, &sni);

    bigBox->Add(treeCtrl_, 1, wxEXPAND);


    treeCtrl_->EnableSystemTheme();

    wxBitmap folder = wxArtProvider::GetBitmap(wxART_FOLDER);

    wxImageList* icons = new wxImageList(folder.GetWidth(), folder.GetHeight(), false, 4);
    icons->Add(folder);
    icons->Add(wxArtProvider::GetBitmap(wxART_FOLDER_OPEN)); // please don't ask me why this is needed twice, i dont know
    icons->Add(wxArtProvider::GetBitmap(wxART_NORMAL_FILE));

    wxBitmap snes_icon = wxBitmap(snes_xpm);
    wxBitmap::Rescale(snes_icon, folder.GetSize());
    icons->Add(snes_icon);
    treeCtrl_->SetImageList(icons);


    bigBox->Layout();
    SetSizer(bigBox);

    Bind(wxEVT_CHOICE, &FileManagerFrame::OnDeviceSelect, this);

    Bind(wxEVT_BUTTON, &FileManagerFrame::OnRefreshButton, this);


    RefreshUris();
}



void FileManagerFrame::OnRefreshButton(wxCommandEvent& event)
{
    RefreshUris();
}

void FileManagerFrame::RefreshUris()
{
    auto device_filter =
        [](const DevicesResponse::Device& device) -> bool
    {
        auto caps = device.capabilities();
        return std::find(caps.begin(), caps.end(), DeviceCapability::ReadDirectory) != caps.end();
    };
    SetStatusText("refreshing devices...");
    sni.refreshDevices(device_filter);
    SetStatusText("devices refreshed");

    uris_ = sni.getDeviceUris();


    bool foundUri = false;

    wxArrayString choices;
    for (std::string uri : uris_)
    {
        choices.push_back(uri);
        foundUri &= uri == current_uri_;
    }

    devicesDropdown_->Set(choices);

    if (!foundUri && uris_.size())
    {
        devicesDropdown_->Select(0);
        current_uri_ = choices[0];
    }
    else
    {
        current_uri_ = "";
        devicesDropdown_->Clear();
    }

    treeCtrl_->setUri(current_uri_);
}

void FileManagerFrame::OnDeviceSelect(wxCommandEvent& event)
{
    treeCtrl_->setUri(uris_[event.GetSelection()]);
}

void FileManagerFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}
void FileManagerFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox("It's a file manager for your SNES. There's drag and drop. Double click to run. Have fun.",
        "About SNFM", wxOK | wxICON_INFORMATION);
}

