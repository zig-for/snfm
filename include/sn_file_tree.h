#pragma once
#ifdef WIN32
#define WIN_DROP_SOURCE
#endif
#ifdef WIN_DROP_SOURCE
#include "win_dnd.h"
#endif

#include "snfm.h"
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/treectrl.h>
#include <wx/dnd.h>
#include <wx/artprov.h>
#include "snes.xpm"

#include <type_traits>
#include <wx/busyinfo.h>
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
        SNIMenuItem_Import,
        SNIMenuItem_Export,
    };

    enum SNIcon
    {
        SNIcon_FOLDER_CLOSE,
        SNIcon_FOLDER_OPEN,
        SNIcon_FILE_UNKNOWN,
        SNIcon_FILE_SNES,
    };

public:
    wxWindow* parent_window_;

    

    template <typename F>
    auto CommunicateWithDevice(const std::string& msg, F f)
    {
        wxBusyInfo wait(
            (
                wxBusyInfoFlags()
                .Parent(this)
                .Icon(wxArtProvider::GetIcon(wxART_REFRESH))
                .Title(msg)
                .Foreground(*wxWHITE)
                .Background(*wxBLACK)
                .Transparency(4 * wxALPHA_OPAQUE / 5)
                ));
        return f();
    }
    SNFileTree() : wxTreeCtrl() {}
    SNFileTree(wxWindow* parent, const std::string& uri, SNIConnection* sni)
        : parent_window_(parent), uri_(uri), sni_(sni),
        wxTreeCtrl(parent, wxID_ANY,
            wxDefaultPosition, wxSize(500, 500), wxTR_HAS_BUTTONS | wxTR_TWIST_BUTTONS | wxTR_EDIT_LABELS | wxTR_MULTIPLE)
    {
#ifdef WIN_DROP_SOURCE
        AfxGetApp()->m_pMainWnd = new CDummyWindow((HWND)wxTheApp->GetTopWindow()->GetHWND());
#endif
        wxLog::SetLogLevel(wxLOG_Debug);
        Bind(wxEVT_COMMAND_TREE_ITEM_EXPANDING, &SNFileTree::OnExpanding, this);
        Bind(wxEVT_TREE_ITEM_ACTIVATED, &SNFileTree::OnActivate, this);
        Bind(wxEVT_TREE_KEY_DOWN, &SNFileTree::OnKeyDown, this);
        Bind(wxEVT_TREE_ITEM_MENU, &SNFileTree::OnRightClick, this);
        Bind(wxEVT_TREE_BEGIN_DRAG, &SNFileTree::OnTreeDragBegin, this);
        Bind(wxEVT_TREE_END_DRAG, &SNFileTree::OnTreeDragEnd, this);
        Bind(wxEVT_TREE_END_LABEL_EDIT, &SNFileTree::OnLabelEdit, this);
        Bind(wxEVT_TREE_SEL_CHANGING, &SNFileTree::OnSelect, this);

        dir_menu_ = new wxMenu();
        dir_menu_->Append(SNIMenuItem_Run, "Run");
        dir_menu_->Append(SNIMenuItem_Rename, "Rename");
        dir_menu_->Append(SNIMenuItem_Refresh, "Refresh");
        dir_menu_->Append(SNIMenuItem_CreateDirectory, "Create Directory");
        dir_menu_->Append(SNIMenuItem_Import, "Import...");
        dir_menu_->Append(SNIMenuItem_Export, "Export...");

        Bind(wxEVT_COMMAND_MENU_SELECTED, &SNFileTree::OnContextMenuSelected, this);

        Bind(wxEVT_ENTER_WINDOW, &SNFileTree::MouseEnter, this);
        Bind(wxEVT_LEAVE_WINDOW, &SNFileTree::MouseLeave, this);

        parent->DragAcceptFiles(true);
        parent->Connect(wxEVT_DROP_FILES, wxDropFilesEventHandler(SNFileTree::OnDropFiles2), NULL, this);


    }
    void OnDropFiles2(wxDropFilesEvent& event)
    {
        wxArrayString files(event.GetNumberOfFiles(), event.GetFiles());
        OnDropFiles(event.GetPosition().x, event.GetPosition().y, files);
    }
    ~SNFileTree()
    {
    }

    void setUri(const std::string& uri);

    std::filesystem::path constructPath(wxTreeItemId folder, wxTreeItemId root = wxTreeItemId());

    bool isPlaceHolder(wxTreeItemId id);

    void refreshFolder(wxTreeItemId folderId);

    void requestBoot(wxTreeItemId fileId);

    bool requestDelete(wxTreeItemId fileId, bool top = true);

    // todo: we should handle this via the delete event
    void fixupEmptyFolder(wxTreeItemId folderId);

    void requestDeleteOfSelection();

    wxTreeItemId showNameUnder(wxTreeItemId folder, const std::string name);

    void createDirectory(wxTreeItemId fileId, std::string directory_name);

    wxTreeItemId parentDirIfNotDir(wxTreeItemId id);

    bool moveToFolder(wxTreeItemId from, wxTreeItemId to);

    bool rename(wxTreeItemId file, const std::string& name);
private:

    void OnSelect(wxTreeEvent& event);

    void MouseEnter(wxMouseEvent& evt);

    void MouseLeave(wxMouseEvent& evt);

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
        int c =
#ifdef WIN32
            strcmpi
#else
            strcasecmp
#endif
            (GetItemText(a).c_str(), GetItemText(b).c_str());
        if (c)
        {
            return c;
        }

        return wxTreeCtrl::OnCompareItems(a, b);
    }
    void OnRightClick(wxTreeEvent& event)
    {
        bool has_children = HasChildren(event.GetItem());
        
        dir_menu_->Enable(SNIMenuItem_Run, !has_children);
        
        PopupMenu(dir_menu_);

    }
public:
    wxTreeItemId GetItemUnderMouse()
    {
        int mask = wxTREE_HITTEST_ONITEMBUTTON | wxTREE_HITTEST_ONITEMICON | wxTREE_HITTEST_ONITEMLABEL | wxTREE_HITTEST_ONITEMRIGHT;
        int flags = 0;
        wxPoint point = ScreenToClient(wxGetMousePosition());
        wxTreeItemId hit = HitTest(point, flags);
        return hit;
    }
private:
    

    void RecursiveChildren(wxTreeItemId item, std::vector<wxTreeItemId>* out, bool allow_parents, bool refresh = true)
    {

        if (refresh && ItemHasChildren(item))
        {
            wxTreeItemIdValue cookie;
            wxTreeItemId childId = GetFirstChild(item, cookie);
            if (GetItemText(childId) == "...")
            {
                refreshFolder(item);
            }
        }
        ;       wxTreeItemIdValue cookie;
        wxTreeItemId childId = GetFirstChild(item, cookie);
        while (childId.IsOk())
        {
            if (allow_parents || !ItemHasChildren(childId))
            {
                out->push_back(childId);
            }
            RecursiveChildren(childId, out, allow_parents);
            childId = GetNextChild(childId, cookie);
        }
    }

    std::vector<wxTreeItemId> ReduceTree(const std::vector<wxTreeItemId>& items)
    {
        std::unordered_set<wxTreeItemId::Type> reduced_items;

        // First gather up all the things that _should_ be removed because they have a parent
        for (wxTreeItemId item : items)
        {
            std::vector<wxTreeItemId> children;
            RecursiveChildren(item, &children, true);
            for (wxTreeItemId child : children)
            {
                reduced_items.insert(child.GetID());
            }
        }

        // now reduce
        std::vector<wxTreeItemId> out;
        for (wxTreeItemId item : items)
        {
            if (reduced_items.count(item.GetID()) == 0)
            {
                out.push_back(item);
            }
        }
        return out;
    }

    void StartDrop()
    {

#ifdef WIN_DROP_SOURCE
        std::filesystem::path temp_folder_name = std::tmpnam(nullptr);
        std::filesystem::create_directory(temp_folder_name);

        std::vector<std::string> device_filenames;
        std::vector<std::wstring> local_filenames_w;

        for (const auto& treeItem : start_drag_items_)
        {
            std::vector<wxTreeItemId> items;
            if (ItemHasChildren(treeItem))
            {
                RecursiveChildren(treeItem, &items, false);
            }
            else
            {
                items.push_back(treeItem);
            }
            for (const wxTreeItemId& item : items)
            {
                device_filenames.push_back(constructPath(item).generic_string());
                local_filenames_w.push_back(constructPath(item, treeItem).generic_wstring());
            }

        }



        UINT uFileCount = device_filenames.size();

        // The CFSTR_FILEDESCRIPTOR format expects a 
        // FILEGROUPDESCRIPTOR structure followed by an
        // array of FILEDESCRIPTOR structures, one for
        // each file being dropped
        UINT uBuffSize = sizeof(FILEGROUPDESCRIPTOR) +
            (uFileCount - 1) * sizeof(FILEDESCRIPTOR);
        HGLOBAL hFileDescriptor = GlobalAlloc(
            GHND | GMEM_SHARE, uBuffSize);


        if (hFileDescriptor)
        {
            FILEGROUPDESCRIPTOR* pGroupDescriptor =
                (FILEGROUPDESCRIPTOR*)GlobalLock(hFileDescriptor);
            if (pGroupDescriptor)
            {
                // Need a pointer to the FILEDESCRIPTOR array
                FILEDESCRIPTOR* pFileDescriptorArray =
                    (FILEDESCRIPTOR*)((LPBYTE)pGroupDescriptor + sizeof(UINT));
                pGroupDescriptor->cItems = uFileCount;

                //POSITION pos = m_fileList.GetFirstSelectedItemPosition();
                int index = 0;
                //m_DataSrc.m_Files.RemoveAll();
                for (int i = 0; i < uFileCount; i++)
                {
                    //int nSelItem = m_fileList.GetNextSelectedItem(pos);
                    ZeroMemory(&pFileDescriptorArray[index],
                        sizeof(FILEDESCRIPTOR));
                    lstrcpy(pFileDescriptorArray[index].cFileName, (local_filenames_w[i]).c_str());
                    //m_DataSrc.m_Files.Add(
                   //     pFileDescriptorArray[index].cFileName);
                    pFileDescriptorArray[index].dwFlags =
                        FD_ATTRIBUTES;
                    pFileDescriptorArray[index].dwFileAttributes =
                        FILE_ATTRIBUTE_NORMAL;

                    index++;
                }
            }
            else
            {
                GlobalFree(hFileDescriptor);
            }
        }
        GlobalUnlock(hFileDescriptor);

        // For the CFSTR_FILEDESCRIPTOR format, we use
        // CacheGlobalData since we make that data available 
        // immediately
        FORMATETC etcDescriptor = {
            (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR),
            NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        SNIAfxDropSource* dataSource = new SNIAfxDropSource(
            std::move(device_filenames),
            [this](const std::string& path)
            {
                return CommunicateWithDevice("Fetching " + path, [this, &path]() -> auto
                    {
                        return sni_->getFile(uri_, path);
                    });
            }
        );
        dataSource->CacheGlobalData(RegisterClipboardFormat(
            CFSTR_FILEDESCRIPTOR), hFileDescriptor, &etcDescriptor);

        // For CFSTR_FILECONTENTS, we use DelayRenderFileData
        // as this data will have to come from a non-physical
        // device, like an FTP site, an add-on device, or an archive
        FORMATETC etcContents = {
            (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILECONTENTS),
            NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL | TYMED_ISTREAM | TYMED_FILE };
        dataSource->DelayRenderFileData(RegisterClipboardFormat(
            CFSTR_FILECONTENTS), &etcContents);

        SNWinDropFeedback* feedback = new SNWinDropFeedback(this);

        DROPEFFECT dwEffect = dataSource->DoDragDrop(
            DROPEFFECT_COPY, 0, feedback);

        // Free memory in case of failure
        if (dwEffect == DROPEFFECT_NONE)
        {
            GlobalFree(hFileDescriptor);
        }

        // Gross - send raw window message to tell the tree view that we aren't dragging anymore
        if (!feedback->returned_to_window_)
        {
            start_drag_items_.clear();
            MSWWindowProc(WM_RBUTTONUP, 0, 0);
        }
        dataSource->InternalRelease();
        feedback->InternalRelease();
#endif
    }

    bool isAncestor(wxTreeItemId maybeAncestor, wxTreeItemId check);

    bool is_dragging_ = false;
    void OnTreeDragBegin(wxTreeEvent& event);
    void OnTreeDragEnd(wxTreeEvent& event);

    void OnExpanding(wxTreeEvent& event);

    void OnActivate(wxTreeEvent& event);

    void OnContextMenuSelected(wxCommandEvent& event);

    void OnKeyDown(wxTreeEvent& event);

    void OnLabelEdit(wxTreeEvent& event);

    void PathToFiles(const std::filesystem::path& path, std::vector<std::pair<std::filesystem::path, std::filesystem::path>>* out)
    {
        if (std::filesystem::is_directory(path))
        {
            for (auto const& child : std::filesystem::recursive_directory_iterator{ path })
            {
                if (std::filesystem::is_regular_file(child))
                {
                    out->push_back({ path, child });
                }
            }
        }
        else if (std::filesystem::is_regular_file(path))
        {
            out->push_back({ path, path });
        }
    }

    void MkDirP(std::filesystem::path dir)
    {
        std::filesystem::path parent = dir.parent_path();
        if (parent != dir)
        {
            MkDirP(dir.parent_path());
        }
        sni_->makeDirectory(uri_, dir.generic_string());
    }

    void ImportFilesTo(wxTreeItemId item, const wxArrayString& filenames_in)
    {
        item = parentDirIfNotDir(item);
        std::filesystem::path device_folder = constructPath(item);

        UnselectAll();

        std::vector<std::filesystem::path> successful_files;


        std::vector<std::pair<std::filesystem::path, std::filesystem::path>> filenames;
        for (const wxString& file : filenames_in)
        {
            std::filesystem::path path(file.ToStdWstring());
            PathToFiles(path, &filenames);
        }

        for (auto& pair : filenames)
        {
            auto [dir, file] = pair;

            std::filesystem::path relative_path = device_folder / std::filesystem::relative(file, dir.parent_path()).parent_path();
            MkDirP(relative_path);
            
            auto result = sni_->putFile(uri_, file,relative_path);
            if (result.has_value())
            {
                successful_files.push_back(*result);
            }
        }

        // todo: wrong for folder imports, potentially
        refreshFolder(item);

        // Can only showNameUnder() after a refresh, or else the tree won't be built yet
        for (const auto& file : successful_files)
        {
            wxTreeItemId childId = showNameUnder(item, file.filename().generic_string());
            if (childId.IsOk())
            {
                SelectItem(childId);
            }
        }
    }

    virtual bool OnDropFiles(wxCoord 	x,
        wxCoord 	y,
        const wxArrayString& filenames
    ) override;

    SNIConnection* sni_ = nullptr;
    std::string uri_;
    wxMenu* dir_menu_;

    //SNIFileDataObject* fileDataObject;

    //wxTreeItemId start_drag_item_;
    std::vector<wxTreeItemId> start_drag_items_;
    bool in_tree_drag_ = false;

};


