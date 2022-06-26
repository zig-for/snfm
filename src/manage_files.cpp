#include "snfm.h"
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <wx/treectrl.h>

#include <wx/artprov.h>
#include "snes.xpm"


class SNFileTree : public wxTreeCtrl
{
    wxDECLARE_DYNAMIC_CLASS(SNFileTree);

    enum SNIcon
    {
        SNIcon_FOLDER_CLOSE,
        SNIcon_FOLDER_OPEN,
        SNIcon_FILE_UNKNOWN,
        SNIcon_FILE_SNES,
    };
public:
    SNFileTree() : wxTreeCtrl() {}
    SNFileTree(wxWindow* parent, const std::string& uri, SNIConnection* sni)
        : uri_(uri), sni_(sni),
        wxTreeCtrl(parent, wxID_ANY,
            wxDefaultPosition, wxSize(500, 500), wxTR_NO_BUTTONS | wxTR_HIDE_ROOT | wxTR_EDIT_LABELS | wxTR_MULTIPLE)
    {
        Bind(wxEVT_COMMAND_TREE_ITEM_EXPANDING, &SNFileTree::OnExpanding, this);
        Bind(wxEVT_TREE_ITEM_ACTIVATED, &SNFileTree::OnActivate, this);
        Bind(wxEVT_TREE_KEY_DOWN, &SNFileTree::OnKeyDown, this);
    }


    std::filesystem::path constructPath(wxTreeCtrl* tree, wxTreeItemId folder)
    {
        wxTreeItemId parent = tree->GetItemParent(folder);

        if (!parent.IsOk())
        {
            return "/";
        }
        return constructPath(tree, parent) / tree->GetItemText(folder).ToStdString();
    }

    void refreshFolder(wxTreeItemId folderId)
    {
        DeleteChildren(folderId);

        std::string full_path = constructPath(this, folderId).generic_string();

        ReadDirectoryResponse response = sni_->readDirectory(uri_, full_path);
        for (const DirEntry& entry : response.entries())
        {
            if (entry.name() == "." || entry.name() == "..")
            {
                continue;
            }
            
            auto nodeId = AppendItem(folderId, entry.name());
            if (entry.type() == DirEntryType::Directory)
            {
                SetItemImage(nodeId, SNIcon_FOLDER_CLOSE);
                SetItemImage(nodeId, SNIcon_FOLDER_OPEN, wxTreeItemIcon_Expanded);
                AppendItem(nodeId, "...");
            }
            else
            {
                if (entry.name().ends_with(".sfc") || entry.name().ends_with(".smc"))
                {
                    SetItemImage(nodeId, SNIcon_FILE_SNES);
                }
                else
                {
                    SetItemImage(nodeId, SNIcon_FILE_UNKNOWN);
                }

            }
        }
        SortChildren(folderId);
    }

    void requestBoot(wxTreeItemId fileId)
    {         
        if (HasChildren(fileId))
        {
            return;
        }
        std::string full_path = constructPath(this, fileId).generic_string();
        sni_->bootFile(uri_, full_path);
    }

    void requestDelete(wxTreeItemId fileId)
    {
        std::string full_path = constructPath(this, fileId).generic_string();

        if (HasChildren(fileId))
        {
            refreshFolder(fileId);
            
//            wxTreeItemId child = this->GetFirstChild(fileId);
            wxTreeItemIdValue cookie;
            wxTreeItemId childId = GetFirstChild(fileId, cookie);
            while(childId.IsOk())
            {
                requestDelete(childId);
                childId = GetNextChild(fileId, cookie);
            }
        }
        
        if (sni_->deleteFile(uri_, full_path))
        {
            Delete(fileId);
        }
        
        
    }

    void requestDeleteOfSelection()
    {
        wxArrayTreeItemIds items;
        GetSelections(items);
        for (wxTreeItemId item : items)
        {
            requestDelete(item);
        }
    }
private:
    virtual int OnCompareItems(const wxTreeItemId& a,
        const wxTreeItemId& b) override
    {
        bool a_is_dir = bool(GetChildrenCount(a, false));
        bool b_is_dir = bool(GetChildrenCount(b, false));
        if (a_is_dir && !b_is_dir)
        {
            return -1;
        }
        if (b_is_dir && !a_is_dir)
        {
            return 1;
        }

        int c = strcmpi(GetItemText(a).c_str(), GetItemText(b).c_str());
        if (c)
        {
            return c;
        }

        return wxTreeCtrl::OnCompareItems(a, b);
    }

    void OnExpanding(wxTreeEvent& event)
    {
        refreshFolder(event.GetItem());
    }

    void OnActivate(wxTreeEvent& event)
    {
        auto item = event.GetItem();
        if (HasChildren(item))
        {
            event.Skip();
		}
        else
        {
            requestBoot(item);
        }
    }

    void OnKeyDown(wxTreeEvent& event)
    {
        switch (event.GetKeyCode())
        {
        case WXK_DELETE:
            requestDeleteOfSelection();
            break;
        default:
            event.Skip();
        }
    }

    void OnDelete(wxTreeEvent& event)
    {
        //requestDelete(event.GetItem());
    }
    SNIConnection* sni_ = nullptr;
    std::string uri_;

};
wxIMPLEMENT_DYNAMIC_CLASS(SNFileTree, wxTreeCtrl);


class MyApp : public wxApp
{
public:
    virtual bool OnInit();
};
class MyFrame : public wxFrame
{
public:
    MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
private:
    void OnHello(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();


    SNIConnection sni;
    std::vector<std::string> uris_;
    std::string current_uri_;

    SNFileTree* treeCtrl_;
};

enum
{
    ID_Hello = 1
};
wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_MENU(ID_Hello, MyFrame::OnHello)
EVT_MENU(wxID_EXIT, MyFrame::OnExit)
EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
wxEND_EVENT_TABLE()
wxIMPLEMENT_APP(MyApp);
bool MyApp::OnInit()
{
    wxInitAllImageHandlers();

    MyFrame* frame = new MyFrame("Super Nintendo File Manager", wxPoint(50, 50), wxSize(450, 340));
    frame->Show(true);
    return true;
}


MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
    : wxFrame(NULL, wxID_ANY, title, pos, size)
{


    wxMenu* menuFile = new wxMenu;
    menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
        "Help string shown in status bar for this menu item");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);
    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);
    CreateStatusBar();
    

    auto device_filter =
        [](const DevicesResponse::Device& device) -> bool
    {
        auto caps = device.capabilities();
        return std::find(caps.begin(), caps.end(), DeviceCapability::ReadDirectory) != caps.end();
    };
    SetStatusText("refreshing devices...");
    sni.refreshDevices(device_filter);
    SetStatusText("devices refreshed");

    wxBoxSizer* bigBox = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* refreshBox = new wxBoxSizer(wxHORIZONTAL);

    bigBox->Add(refreshBox, 0);

    wxButton* button = new wxButton(this, wxID_ANY, L"🗘", wxDefaultPosition, wxSize(25, 25));

    uris_ = sni.getDeviceUris();
    //wxArrayString choices(1, { "fxpakpro://./COM4" });
    wxArrayString choices;// (uris.front(), uris.back());
    for (std::string uri : uris_)
    {
        choices.push_back(uri);
    }
    wxChoice* devicesDropdown = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxSize(2500, 25), choices);
    if (!choices.empty())
    {
        devicesDropdown->Select(0);
        current_uri_ = choices[0];
    }
    //devicesDropdown->AppendString();
    //devicesDropdown->SetHint("fxpakpro://./COM4");
    refreshBox->Add(devicesDropdown, 1, wxEXPAND);
    //refreshBox->SetSizeHints(this);
    refreshBox->Add(button);

    treeCtrl_ = new SNFileTree(this, current_uri_, &sni);
    
    bigBox->Add(treeCtrl_, 1, wxEXPAND);


    treeCtrl_->EnableSystemTheme();

    wxBitmap folder = wxArtProvider::GetBitmap(wxART_FOLDER);
    
    wxImageList* icons = new wxImageList(folder.GetWidth(), folder.GetHeight(), false, 2);
    icons->Add(folder);
    icons->Add(wxArtProvider::GetBitmap(wxART_FOLDER_OPEN));
    icons->Add(wxArtProvider::GetBitmap(wxART_NORMAL_FILE));
    wxBitmap snes_icon = wxBitmap(snes_xpm);
    wxBitmap::Rescale(snes_icon, folder.GetSize());
    icons->Add(snes_icon);
    treeCtrl_->SetImageList(icons);

    wxTreeItemId rootId = treeCtrl_->AddRoot("/");

    treeCtrl_->SetItemImage(rootId, 0);
    treeCtrl_->SetItemImage(rootId, 1, wxTreeItemIcon_Expanded);


    if (!current_uri_.empty())
    {    
        treeCtrl_->refreshFolder(rootId);
    }
 
    bigBox->Layout();
    SetSizer(bigBox);
   
}


void MyFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}
void MyFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox("This is a wxWidgets' Hello world sample",
        "About Hello World", wxOK | wxICON_INFORMATION);
}
void MyFrame::OnHello(wxCommandEvent& event)
{
    //wxLogMessage("Hello world from wxWidgets!");
}
#if 0
int main(int argc, char* argv[])
{
    MyApp app;
        
    /*
    SNIConnection sni;

    auto device_filter =
        [](const DevicesResponse::Device& device) -> bool
    {
        auto caps = device.capabilities();
        return std::find(caps.begin(), caps.end(), DeviceCapability::MakeDirectory) != caps.end() &&
            std::find(caps.begin(), caps.end(), DeviceCapability::PutFile) != caps.end() &&
            std::find(caps.begin(), caps.end(), DeviceCapability::BootFile) != caps.end();
    };
    std::cout << "refreshing devices" << std::endl;
    sni.refreshDevices(device_filter);
    auto uri = sni.getFirstDeviceUri();
    if (!uri)
    {
        std::cout << "no uri" << std::endl;
        return 1;
    }*/
    return 0;
}
#endif
