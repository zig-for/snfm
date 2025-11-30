#pragma once
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#undef EXPORT

enum class SystemIcon
{
    FOLDER = 3,
    OPEN_FOLDER = 4,
    QUESTION = 23,
    RENAME = 133,
    EXPORT = 122,
    REFRESH = 238,
    EXECUTE = 261,
    RESET_TO_MENU = 253,
    RESET_GAME = 46,
};

inline wxIcon GetSystemIcon(SystemIcon index)
{
#if _WIN32

    HICON hicon;
    wxIcon icon;

    ::ExtractIconEx(L"shell32.dll", (int)index, NULL, &hicon, 1);
    icon.SetHandle(hicon);

    return icon;
#else
    return {};
#endif

}