#include "sn_file_tree.h"


void SNFileTree::MouseEnter(wxMouseEvent& evt)
{

}

void SNFileTree::MouseLeave(wxMouseEvent& evt)
{
    if (is_dragging_)
    {
        StartDrop();
    }
}
void SNFileTree::OnTreeDragBegin(wxTreeEvent& event)
{
    is_dragging_ = true;

    event.Allow();
    start_drag_items_.clear();
    wxArrayTreeItemIds ids;
    GetSelections(ids);
    for (auto id : ids)
    {
        start_drag_items_.push_back(id);
    }
    start_drag_items_ = ReduceTree(start_drag_items_);

}
void SNFileTree::OnTreeDragEnd(wxTreeEvent& event)
{
    is_dragging_ = false;
    wxTreeItemId to = event.GetItem();
    if (to.IsOk())
    {
        std::vector<std::string> file_names;
        for (wxTreeItemId from : start_drag_items_)
        {

            if (from.IsOk())
            {
                file_names.push_back(GetItemText(from).ToStdString());

                if (!isAncestor(from, to))
                {
                    moveToFolder(from, to);
                }
            }
        }
        UnselectAll();

        // Wait until we are done messing with the directory tree to mess with selection
        for (std::string file_name : file_names)
        {
            std::filesystem::path to_path = constructPath(to) / file_name;
            wxTreeItemId childId = showNameUnder(to, file_name);
            if (childId.IsOk())
            {
                SelectItem(childId);
            }
        }
    }
}

void SNFileTree::OnExpanding(wxTreeEvent& event)
{
    refreshFolder(event.GetItem());
}

void SNFileTree::OnActivate(wxTreeEvent& event)
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
void SNFileTree::OnContextMenuSelected(wxCommandEvent& event)
{
    static const wxChar* FILETYPES = _T(
        "Super Nintendo ROMs (*.sfc, *.smc)|*.sfc;*.smc|"
        "All files (*.*)|*.*"
    );
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
    case SNIMenuItem_Import:
    {
        wxFileDialog openFileDialog(this, _("Select File to Import"), "", "", FILETYPES, wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

        if (openFileDialog.ShowModal() == wxID_CANCEL)
        {
            return;
        }
        wxArrayString filenames;
        openFileDialog.GetPaths(filenames);
        ImportFilesTo(GetFocusedItem(), filenames);
    }
    break;
    case SNIMenuItem_Export:
    {
        auto selected_item = GetFocusedItem();
        bool has_children = ItemHasChildren(selected_item);
        wxFileDialog openFileDialog(this, _("Export to..."), "", GetItemText(selected_item), has_children ? L"All Files (*.*)|*.*" : FILETYPES, wxFD_SAVE);

        if (openFileDialog.ShowModal() == wxID_CANCEL)
        {
            return;
        }

        if (has_children)
        {
            std::vector<wxTreeItemId> items;
            RecursiveChildren(selected_item, &items, false);


            std::filesystem::path local_root = openFileDialog.GetPath().ToStdWstring();

            for (const wxTreeItemId& item : items)
            {
                std::filesystem::path device_path = constructPath(item);
                // Make sure to strip off whatever the existing folder is in favor of the new saved name
                std::filesystem::path a = constructPath(item);
                std::filesystem::path b = constructPath(selected_item);
                std::filesystem::path c = std::filesystem::relative(a, b);
                std::filesystem::path path = local_root / c;
                std::filesystem::create_directories(path.parent_path());
                sni_->getFile(uri_, device_path, path, true);
            }
        }
        else
        {
            sni_->getFile(uri_, constructPath(selected_item), openFileDialog.GetPath().ToStdWstring(), true);
        }
    }
    break;
    case SNIMenuItem_Delete:
        requestDeleteOfSelection();
        break;
    }
}

void SNFileTree::OnKeyDown(wxTreeEvent& event)
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

void SNFileTree::OnSelect(wxTreeEvent& event)
{

}

void SNFileTree::OnLabelEdit(wxTreeEvent& event)
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

wxTreeItemId SNFileTree::HitTestFromGlobal(wxPoint point, int mask) const
{
    int flags = 0;
    point -= GetPosition();
    wxTreeItemId hit = HitTest(point, flags);

    if ((flags & mask) && hit.IsOk())
    {
        return hit;
    }
    return {};
}

bool SNFileTree::OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames)
{
    wxTreeItemId hit = HitTestFromGlobal({ x, y });

    if (hit.IsOk())
    {
        ImportFilesTo(hit, filenames);
        return true;
    }

    return false;
}

// TODO: could store name of item instead
void SNFileTree::SetItemDropHighlightHack(wxTreeItemId root)
{
    wxTreeItemIdValue cookie;
    wxTreeItemId item = GetFirstChild(root, cookie);

    while (item.IsOk())
    {
        if (ItemHasChildren(item))
        {
            SetItemDropHighlightHack(item);
            SetItemDropHighlight(item, false);
        }
        item = GetNextChild(root, cookie);
    }
}

wxDragResult SNFileTree::OnDragOver(wxCoord 	x,
    wxCoord 	y,
    wxDragResult 	defResult
) 
{
    wxTreeItemId hit = HitTestFromGlobal({ x, y });

    if (last_dnd_highlight_.IsOk())
    {
        SetItemDropHighlight(last_dnd_highlight_, false);
    }
    else
    {
#if !_WIN32
        // Linux has a bug where the highlight goes invalid, no idea why
        SetItemDropHighlightHack(GetRootItem());
#endif
    }
    last_dnd_highlight_ = hit;
    if (hit.IsOk())
    {
        SetItemDropHighlight(hit);
    }

    return defResult;
}