#pragma once

#define WIN_DROP_SOURCE

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
    wxWindow* parent_window_;

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

        dir_menu_ = new wxMenu();
        dir_menu_->Append(SNIMenuItem_Run, "Run");
        dir_menu_->Append(SNIMenuItem_Rename, "Rename");
        dir_menu_->Append(SNIMenuItem_Refresh, "Refresh");
        dir_menu_->Append(SNIMenuItem_CreateDirectory, "Create Directory");
        //dir_menu_->Append(SNIMenuItem_CreateDirectory, "Import file");
        //dir_menu_->Append(SNIMenuItem_CreateDirectory, "Export file");

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

    std::filesystem::path constructPath(wxTreeItemId folder);

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
        if (HasChildren(event.GetItem()))
        {
            PopupMenu(dir_menu_);
        }
        else
        {
            PopupMenu(dir_menu_);
        }
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
    void OnTreeDragBegin(wxTreeEvent& event)
    {
        is_dragging_ = true;

        event.Allow();
        GetSelections(start_drag_items_);
    }


    void StartDrop()
    {

#ifdef WIN_DROP_SOURCE
        std::filesystem::path temp_folder_name = std::tmpnam(nullptr);
        std::filesystem::create_directory(temp_folder_name);

        UINT uFileCount = start_drag_items_.Count();

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
                    lstrcpy(pFileDescriptorArray[index].cFileName, GetItemText(start_drag_items_[i]).ToStdWstring().c_str());
                    //m_DataSrc.m_Files.Add(
                   //     pFileDescriptorArray[index].cFileName);
                    pFileDescriptorArray[index].dwFlags =
                        FD_FILESIZE | FD_ATTRIBUTES;
                    pFileDescriptorArray[index].nFileSizeLow = 512;
                    pFileDescriptorArray[index].nFileSizeHigh = 0;
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
        SNIAfxDropSource* dataSource = new SNIAfxDropSource([this]
            {
                // Gross - send raw window message to tell the tree view that we aren't dragging anymore
                MSWWindowProc(WM_RBUTTONUP, 0, 0);

            });
        dataSource->CacheGlobalData(RegisterClipboardFormat(
            CFSTR_FILEDESCRIPTOR), hFileDescriptor, &etcDescriptor);

        // For CFSTR_FILECONTENTS, we use DelayRenderFileData
        // as this data will have to come from a non-physical
        // device, like an FTP site, an add-on device, or an archive
        FORMATETC etcContents = {
            (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILECONTENTS),
            NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        dataSource->DelayRenderFileData(RegisterClipboardFormat(
            CFSTR_FILECONTENTS), &etcContents);

        SNWinDropFeedback* feedback = new SNWinDropFeedback(this);

        DROPEFFECT dwEffect = dataSource->DoDragDrop(
            DROPEFFECT_COPY, 0, feedback);
        dataSource->InternalRelease();
        feedback->InternalRelease();
        // Free memory in case of failure
        if (dwEffect == DROPEFFECT_NONE)
        {
            GlobalFree(hFileDescriptor);
        }
#endif
    }

    bool isAncestor(wxTreeItemId maybeAncestor, wxTreeItemId check);
    bool is_dragging_ = false;
    void OnTreeDragEnd(wxTreeEvent& event);

    void OnExpanding(wxTreeEvent& event);

    void OnActivate(wxTreeEvent& event);

    void OnContextMenuSelected(wxCommandEvent& event);

    void OnKeyDown(wxTreeEvent& event);

    void OnLabelEdit(wxTreeEvent& event);

    virtual bool OnDropFiles(wxCoord 	x,
        wxCoord 	y,
        const wxArrayString& filenames
    ) override;

    SNIConnection* sni_ = nullptr;
    std::string uri_;
    wxMenu* dir_menu_;

    //SNIFileDataObject* fileDataObject;

    //wxTreeItemId start_drag_item_;
    wxArrayTreeItemIds start_drag_items_;
    bool in_tree_drag_ = false;

};

