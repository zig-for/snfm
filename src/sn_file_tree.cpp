#include "sn_file_tree.h"

wxIMPLEMENT_DYNAMIC_CLASS(SNFileTree, wxTreeCtrl);
void SNFileTree::setUri(const std::string& uri)
{
    if (uri == uri_)
    {
        if (!uri_.empty())
        {
            refreshFolder(GetRootItem());
        }
        return;
    }
    uri_ = uri;
    DeleteAllItems();

    if (!uri_.empty())
    {
        wxTreeItemId rootId = AddRoot("/");

        SetItemImage(rootId, 0);
        SetItemImage(rootId, 1, wxTreeItemIcon_Expanded);


        refreshFolder(rootId);

        Expand(rootId);
    }

}

std::filesystem::path SNFileTree::constructPath(wxTreeItemId folder, wxTreeItemId rootItem)
{
    if (!rootItem.IsOk())
    {
        rootItem = GetRootItem();
    }
    wxTreeItemId parent = GetItemParent(folder);

    if (folder == rootItem || !parent.IsOk())
    {
        return std::filesystem::path(GetItemText(rootItem).ToStdString(), std::filesystem::path::generic_format);
    }
    return constructPath(parent, rootItem) / GetItemText(folder).ToStdString();
}

bool SNFileTree::isPlaceHolder(wxTreeItemId id)
{
    return GetItemText(id) == "..." || GetItemText(id) == "(empty)";
}

void SNFileTree::refreshFolder(wxTreeItemId folderId)
{
    wxTreeItemId parentId = GetItemParent(folderId);
    if (parentId.IsOk() && !HasChildren(folderId))
    {
        return refreshFolder(parentId);
    }

    std::unordered_map<wxString, wxTreeItemId> nodes;


    wxTreeItemIdValue cookie;
    wxTreeItemId childId = GetFirstChild(folderId, cookie);
    if (childId.IsOk() && isPlaceHolder(childId))
    {
        DeleteChildren(folderId);
        childId.Unset();
    }


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
void SNFileTree::requestBoot(wxTreeItemId fileId)
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

bool SNFileTree::requestDelete(wxTreeItemId fileId, bool top /*= true*/)
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
        while (childId.IsOk())
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

void SNFileTree::fixupEmptyFolder(wxTreeItemId folderId)
{
    if (!ItemHasChildren(folderId))
    {
        AppendItem(folderId, "(empty)");
    }
}

void SNFileTree::requestDeleteOfSelection()
{
    wxArrayTreeItemIds items;
    GetSelections(items);
    for (wxTreeItemId item : items)
    {
        requestDelete(item);
    }
}

wxTreeItemId SNFileTree::showNameUnder(wxTreeItemId folder, const std::string name)
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

void SNFileTree::createDirectory(wxTreeItemId fileId, std::string directory_name)
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

wxTreeItemId SNFileTree::parentDirIfNotDir(wxTreeItemId id)
{
    if (!ItemHasChildren(id))
    {
        return GetItemParent(id);
    }
    return id;
}

bool SNFileTree::moveToFolder(wxTreeItemId from, wxTreeItemId to)
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

bool SNFileTree::rename(wxTreeItemId file, const std::string& name)
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

bool SNFileTree::isAncestor(wxTreeItemId maybeAncestor, wxTreeItemId check)
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