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

void SNFileTree::OnTreeDragEnd(wxTreeEvent& event)
{
    is_dragging_ = false;
    UnselectAll();
    wxTreeItemId to = event.GetItem();
    if (to.IsOk())
    {
        for (wxTreeItemId from : start_drag_items_)
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

bool SNFileTree::OnDropFiles(wxCoord 	x,
    wxCoord 	y,
    const wxArrayString& filenames
)
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

        UnselectAll();

        std::vector<std::filesystem::path> successful_files;

        for (const wxString& file : filenames)
        {
            auto result = sni_->putFile(uri_, file.ToStdString(), device_path);
            if (result.has_value())
            {
                successful_files.push_back(*result);
            }
        }

        refreshFolder(hit);

        // Can only showNameUnder() after a refresh, or else the tree won't be built yet
        for (const auto& file : successful_files)
        {
            wxTreeItemId childId = showNameUnder(hit, file.filename().generic_string());
            if (childId.IsOk())
            {
                SelectItem(childId);
            }
        }

        return true;
    }

    return false;
}