#include "sn_file_tree.h"
#include "manage_files.xpm"
#include "snes.xpm"

#if WIN32
#include <CommonControls.h>
#endif

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
    void OnResetGame(wxCommandEvent& event);
    void OnResetToMenu(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();

    
    
    void OnDeviceSelect(wxCommandEvent& event);
    void SelectDevice(const std::string& uri);

    void OnRefreshButton(wxCommandEvent& event);

    void RefreshUris();

    SNIConnection sni;
    std::vector<std::string> uris_;
    std::string current_uri_;

    SNFileTree* treeCtrl_;
    wxComboBox* devicesDropdown_;

    wxMenuItem* resetToMenuButton_;
    wxMenuItem* resetGameButton_;
};

enum
{
    MenuID_ResetGame = 1,
    MenuID_ResetToMenu,
    
};
wxBEGIN_EVENT_TABLE(FileManagerFrame, wxFrame)

EVT_MENU(wxID_EXIT, FileManagerFrame::OnExit)
EVT_MENU(wxID_ABOUT, FileManagerFrame::OnAbout)
EVT_MENU(MenuID_ResetGame, FileManagerFrame::OnResetGame)
EVT_MENU(MenuID_ResetToMenu, FileManagerFrame::OnResetToMenu)

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
    SetIcon(manage_files_xpm);

    wxMenu* menuFile = new wxMenu;
    menuFile->Append(wxID_EXIT);

    wxMenu* menuDevice = new wxMenu;
    resetGameButton_ = menuDevice->Append(MenuID_ResetGame, "Reset Game");
    resetToMenuButton_ = menuDevice->Append(MenuID_ResetToMenu, "Reset to Menu");

    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuDevice, "&Device");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);
    CreateStatusBar();

    wxBoxSizer* bigBox = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* refreshBox = new wxBoxSizer(wxHORIZONTAL);

    bigBox->Add(refreshBox, 0);

    wxBitmap refresh_icon = wxArtProvider::GetIcon(wxART_REFRESH);
    wxBitmap::Rescale(refresh_icon, wxSize(15, 15));
    wxBitmapButton* button = new wxBitmapButton(this, wxID_ANY, refresh_icon, wxDefaultPosition, wxSize(25, 25));
    

    devicesDropdown_ = new wxComboBox(this, wxID_ANY, "", wxDefaultPosition, wxSize(2500, 25), {}, wxCB_READONLY);

    refreshBox->Add(devicesDropdown_, 1, wxEXPAND);

    refreshBox->Add(button);

    treeCtrl_ = new SNFileTree(this, current_uri_, &sni);

    bigBox->Add(treeCtrl_, 1, wxEXPAND);


    treeCtrl_->EnableSystemTheme();

    wxBitmap folder = wxArtProvider::GetBitmap(wxART_FOLDER);

    wxImageList* icons = new wxImageList(folder.GetWidth(), folder.GetHeight(), false, 4);
#if WIN32
    SHFILEINFO icon_close = {};
    SHFILEINFO icon_open = {};

    HIMAGELIST imageList;
    HIMAGELIST imageList2;
    imageList = (HIMAGELIST)SHGetFileInfo(
        _T("Doesn't matter"),
        FILE_ATTRIBUTE_DIRECTORY,
        &icon_close, sizeof icon_close,
        SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX);
    imageList2 = (HIMAGELIST)SHGetFileInfo(
        _T("Doesn't matter"),
        FILE_ATTRIBUTE_DIRECTORY,
        &icon_open, sizeof icon_open,
        SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_OPENICON);



    {
        wxIcon icon;
        HICON hicon;
        HIMAGELIST imageList;
        imageList = (HIMAGELIST)SHGetFileInfo(
            _T("Doesn't matter"),
            FILE_ATTRIBUTE_DIRECTORY,
            &icon_close, sizeof icon_close,
            SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX);
        IImageList* imageListInterface;
        HIMAGELIST_QueryInterface(imageList, IID_IImageList, (void**)&imageListInterface);
        imageListInterface->GetIcon(icon_close.iIcon, 0, &hicon);
        icon.SetHandle(hicon);
        icons->Add(icon);
        DestroyIcon(hicon);
    }
    {
        wxIcon icon;
        HICON hicon;
        HIMAGELIST imageList;
        imageList = (HIMAGELIST)SHGetFileInfo(
            _T("Doesn't matter"),
            FILE_ATTRIBUTE_DIRECTORY,
            &icon_close, sizeof icon_close,
            SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_OPENICON | SHGFI_LARGEICON);
        IImageList* imageListInterface;
        HIMAGELIST_QueryInterface(imageList, IID_IImageList, (void**)&imageListInterface);
        imageListInterface->GetIcon(icon_open.iIcon, 0, &hicon);
        icon.SetHandle(hicon);
        icons->Add(icon);
        DestroyIcon(hicon);
    }
#else
    icons->Add(folder);
    icons->Add(wxArtProvider::GetBitmap(wxART_FOLDER_OPEN));
#endif
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
        return HasCapability(device, DeviceCapability::ReadDirectory);
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

    if (!foundUri)
    {
        std::string uri;
        if (uris_.size())
        {
            devicesDropdown_->Select(0);
            uri = choices[0];
        }
        else
        {
            uri = "";
            devicesDropdown_->Clear();
        }
        SelectDevice(uri);
    }
}

void FileManagerFrame::OnDeviceSelect(wxCommandEvent& event)
{
    SelectDevice(uris_[event.GetSelection()]);
}


void FileManagerFrame::SelectDevice(const std::string& uri)
{
    if (uri != current_uri_)
    {
        current_uri_ = uri;
        treeCtrl_->setUri(uri);
        
        auto device = sni.getDevice(uri);
        if (device)
        {
            resetGameButton_->Enable(HasCapability(*device, DeviceCapability::ResetSystem));
            resetToMenuButton_->Enable(HasCapability(*device, DeviceCapability::ResetToMenu));
            
        }

    }
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

void FileManagerFrame::OnResetGame(wxCommandEvent& event)
{
    sni.resetGame(current_uri_);
}
void FileManagerFrame::OnResetToMenu(wxCommandEvent& event)
{
    sni.resetToMenu(current_uri_);
}
