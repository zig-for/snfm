#include "snfm.h"
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <wx/treectrl.h>
#include <wx/dnd.h>
#include <wx/artprov.h>
#include "snes.xpm"

#define PROTECT_SYSTEM_FOLDERS(fileId) if (GetRootItem() == fileId || GetItemText(fileId) == "/" || (GetItemText(fileId) == "sd2snes" && GetItemParent(fileId) == GetRootItem())){ return false; }

class SNFileTree : public wxTreeCtrl, wxFileDropTarget
{
    wxDECLARE_DYNAMIC_CLASS(SNFileTree);

    enum SNIMenuItem
    {
        SNIMenuItem_Run,
        SNIMenuItem_Refresh,
        SNIMenuItem_Rename,
        SNIMenuItem_CreateDirectory,
        SNIMenuItem_Delete,
    };

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
            wxDefaultPosition, wxSize(500, 500), wxTR_HAS_BUTTONS | wxTR_TWIST_BUTTONS | wxTR_EDIT_LABELS | wxTR_MULTIPLE)
    {
        Bind(wxEVT_COMMAND_TREE_ITEM_EXPANDING, &SNFileTree::OnExpanding, this);
        Bind(wxEVT_TREE_ITEM_ACTIVATED, &SNFileTree::OnActivate, this);
        Bind(wxEVT_TREE_KEY_DOWN, &SNFileTree::OnKeyDown, this);
        Bind(wxEVT_TREE_ITEM_MENU, &SNFileTree::OnRightClick, this);
        Bind(wxEVT_TREE_BEGIN_DRAG, &SNFileTree::OnTreeDragBegin, this);
        Bind(wxEVT_TREE_END_DRAG, &SNFileTree::OnTreeDragEnd, this);
        Bind(wxEVT_TREE_END_LABEL_EDIT, &SNFileTree::OnLabelEdit, this);

        dir_menu_ = new wxMenu();
        dir_menu_->Append(SNIMenuItem_Run, "Run");
        dir_menu_->Append(SNIMenuItem_Rename, "Rename");
        dir_menu_->Append(SNIMenuItem_Refresh, "Refresh");
        dir_menu_->Append(SNIMenuItem_CreateDirectory, "Create Directory");
        //dir_menu_->Append(SNIMenuItem_CreateDirectory, "Import file");
        //dir_menu_->Append(SNIMenuItem_CreateDirectory, "Export file");
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, &SNFileTree::OnContextMenuSelected, this);

        parent->SetDropTarget(this);
    }

    void setUri(const std::string& uri)
    {
        uri_ = uri;
        DeleteAllItems();
        
        wxTreeItemId rootId = AddRoot("/");

        SetItemImage(rootId, 0);
        SetItemImage(rootId, 1, wxTreeItemIcon_Expanded);

        if (!uri_.empty())
        {
            refreshFolder(rootId);
        }

        Expand(rootId);
    }

    std::filesystem::path constructPath(wxTreeItemId folder)
    {
        wxTreeItemId parent = GetItemParent(folder);

        if (!parent.IsOk())
        {
            return std::filesystem::path("/", std::filesystem::path::generic_format);
        }
        return constructPath(parent) / GetItemText(folder).ToStdString();
    }

    bool isPlaceHolder(wxTreeItemId id)
    {
        return GetItemText(id) == "..." || GetItemText(id) == "(empty)";
    }

    void refreshFolder(wxTreeItemId folderId)
    {        
        wxTreeItemId parentId = GetItemParent(folderId);
        if (parentId.IsOk() && !HasChildren(folderId))
        {
            return refreshFolder(parentId);
        }
        wxTreeItemIdValue cookie;
        wxTreeItemId childId = GetFirstChild(folderId, cookie);
        if(childId.IsOk() && isPlaceHolder(childId))
        {        
            DeleteChildren(folderId);
        }

        std::unordered_map<wxString, wxTreeItemId> nodes;
        while (childId.IsOk())
        {
            nodes[GetItemText(childId)] = childId;
            childId = GetNextChild(folderId, cookie);
        }
        std::string full_path = constructPath(folderId).generic_string();
        
        ReadDirectoryResponse response = sni_->readDirectory(uri_, full_path);
        for (const DirEntry& entry : response.entries())
        {
            if (entry.name() == "." || entry.name() == "..")
            {
                continue;
            }

            // Don't add nodes that already exist
            // technically this won't catch the edge case of a file type changing, I don't really care
            auto it = nodes.find(entry.name());
            if (it != nodes.end())
            {
                nodes.erase(it);
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
        for (const auto& p : nodes)
        {
            Delete(p.second);
        }
        SortChildren(folderId);
        fixupEmptyFolder(folderId);
    }

    // TODO: make sure this is sfc/smc
    void requestBoot(wxTreeItemId fileId)
    {         
        if (isPlaceHolder(fileId))
        {
            return;
        }
        if (HasChildren(fileId))
        {
            return;
        }
        std::string full_path = constructPath(fileId).generic_string();
        sni_->bootFile(uri_, full_path);
    }

    bool requestDelete(wxTreeItemId fileId, bool top = true)
    {
        if (isPlaceHolder(fileId))
        {
            return false;
        }
        std::string full_path = constructPath(fileId).generic_string();

        // S A F E T Y
        PROTECT_SYSTEM_FOLDERS(fileId);

        if (HasChildren(fileId))
        {
            refreshFolder(fileId);
            
            wxTreeItemIdValue cookie;
            wxTreeItemId childId = GetFirstChild(fileId, cookie);
            while(childId.IsOk())
            {
                requestDelete(childId, false);
                childId = GetNextChild(fileId, cookie);
            }
        }
        
        if (sni_->deleteFile(uri_, full_path))
        {
            if (top)
            {
                wxTreeItemId parentId = GetItemParent(fileId);
                Delete(fileId);
                fixupEmptyFolder(parentId);
            }
            return true;
        }
        return false;
    }

    // todo: we should handle this via the delete event
    void fixupEmptyFolder(wxTreeItemId folderId)
    {
        if (!ItemHasChildren(folderId))
        {
            AppendItem(folderId, "(empty)");
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

    wxTreeItemId showNameUnder(wxTreeItemId folder, const std::string name)
    {
        wxTreeItemIdValue cookie;
        wxTreeItemId childId = GetFirstChild(folder, cookie);
        while (childId.IsOk())
        {
            if (GetItemText(childId) == name)
            {
                EnsureVisible(childId);
                break;
            }
            childId = GetNextChild(folder, cookie);
        }
        return childId;
    }

    void createDirectory(wxTreeItemId fileId, std::string directory_name)
    {
        // If we make a dir on a file, make it a sibling
        if (!HasChildren(fileId))
        {
            return createDirectory(GetItemParent(fileId), directory_name);
        }
        std::filesystem::path path = constructPath(fileId) / directory_name;
        sni_->makeDirectory(uri_, path.generic_string());
        
        refreshFolder(fileId);
        wxTreeItemId childId = showNameUnder(fileId, directory_name);
        if (childId.IsOk())
        {
            UnselectAll();
            SelectItem(childId);
        }
    }

    wxTreeItemId parentDirIfNotDir(wxTreeItemId id)
    {
        if (!ItemHasChildren(id))
        {
            return GetItemParent(id);
        }
        return id;
    }

    bool moveToFolder(wxTreeItemId from, wxTreeItemId to)
    {
        PROTECT_SYSTEM_FOLDERS(from);
        to = parentDirIfNotDir(to);
        if (!to.IsOk())
        {
            // this happens if you move stuff around enough? something is out of sync
            return false;
        }
        std::string file_name = GetItemText(from).ToStdString();
        std::filesystem::path to_path = constructPath(to) / file_name;

        if (sni_->renameFile(uri_, constructPath(from), to_path))
        {
            wxTreeItemId from_parent = GetItemParent(from);
            Delete(from);
            fixupEmptyFolder(from_parent);
            refreshFolder(to);
            wxTreeItemId childId = showNameUnder(to, file_name);
            SelectItem(childId);
            return true;
        }
        return false;
    }

    bool rename(wxTreeItemId file, const std::string& name)
    {
        PROTECT_SYSTEM_FOLDERS(file);

        if (GetItemText(file) == name)
        {
            return false;
        }

        wxTreeItemId parent = GetItemParent(file);
        
        std::filesystem::path from_path = constructPath(file);
        std::filesystem::path to_path = constructPath(parent) / name;

        bool ok = sni_->renameFile(uri_, from_path, to_path);
        if (ok)
        {
            SortChildren(parent);
        }
        return ok;
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
    void OnRightClick(wxTreeEvent& event)
    {
        if (HasChildren(event.GetItem()))
        {
            PopupMenu(dir_menu_);
        }
        else
        {
            PopupMenu(dir_menu_);
        }
    }
    void OnTreeDragBegin(wxTreeEvent& event)
    {
        event.Allow();
        
        GetSelections(start_drag_items_);
    }

    bool isAncestor(wxTreeItemId maybeAncestor, wxTreeItemId check)
    {
        while (check.IsOk())
        {
            if (check == maybeAncestor)
            {
                return true;
            }
            check = GetItemParent(check);
        }
        return false;
    }

    void OnTreeDragEnd(wxTreeEvent& event)
    {
        UnselectAll();
        wxTreeItemId to = event.GetItem();
        for(wxTreeItemId from : start_drag_items_)
        {
            if (from.IsOk())
            {
                if (!isAncestor(from, to))
                {
                    moveToFolder(from, to);
                }
            }
        }
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
    void OnContextMenuSelected(wxCommandEvent& event)
    {
        switch (event.GetId())
        {
        case SNIMenuItem_Run:
            requestBoot(GetFocusedItem());
            break;
        case SNIMenuItem_Rename:
            EditLabel(GetFocusedItem());
        case SNIMenuItem_Refresh:
            refreshFolder(GetFocusedItem());
            break;
        case SNIMenuItem_CreateDirectory:
        {
            wxString text = wxGetTextFromUser("Directory name?");
            if (!text.empty())
            {
                if (text.Contains("/"))
                {
                    wxLogError("Invalid directory name.");
                }
                else
                {
                    createDirectory(GetFocusedItem(), text.ToStdString());
                }
            }
        }
            break;
        case SNIMenuItem_Delete:
            requestDeleteOfSelection();
            break;
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

    void OnLabelEdit(wxTreeEvent& event)
    {
        if (event.IsEditCancelled())
        {
            return;
        }
        if (!rename(event.GetItem(), event.GetLabel().ToStdString()))
        {
            event.Veto();
        }
    }

    virtual bool OnDropFiles(wxCoord 	x,
        wxCoord 	y, 
        const wxArrayString& filenames
    ) override
    {
        int mask = wxTREE_HITTEST_ONITEMBUTTON | wxTREE_HITTEST_ONITEMICON | wxTREE_HITTEST_ONITEMLABEL | wxTREE_HITTEST_ONITEMRIGHT;
        int flags = 0;
        wxPoint point{ x,y };
        point -= GetPosition();
        wxTreeItemId hit = HitTest(point, flags);

        if ((flags & mask) && hit.IsOk())
        {
            hit = parentDirIfNotDir(hit);
            std::filesystem::path device_path = constructPath(hit);

            for (const wxString& file : filenames)
            {
                sni_->putFile(uri_, file.ToStdString(), device_path);
            }

            refreshFolder(hit);
            return true;
        }

        return false;
    }

    SNIConnection* sni_ = nullptr;
    std::string uri_;
    wxMenu* dir_menu_;

    //wxTreeItemId start_drag_item_;
    wxArrayTreeItemIds start_drag_items_;
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
    
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();

    void OnDeviceSelect(wxCommandEvent& event);


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
    
    wxArrayString choices;
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
    
    refreshBox->Add(devicesDropdown, 1, wxEXPAND);

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

    
    treeCtrl_->setUri(current_uri_);
    
    bigBox->Layout();
    SetSizer(bigBox);

    Bind(wxEVT_CHOICE, &MyFrame::OnDeviceSelect, this);
}

void MyFrame::OnDeviceSelect(wxCommandEvent& event)
{
    treeCtrl_->setUri(uris_[event.GetSelection()]);
}

void MyFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}
void MyFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox("It's a file manager for your SNES. There's drag and drop. Double click to run. Have fun.",
        "About SNFM", wxOK | wxICON_INFORMATION);
}
