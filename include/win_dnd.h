#pragma once
#ifdef WIN32
#define WIN_DROP_SOURCE 
#endif
#ifdef WIN_DROP_SOURCE
#include <afxole.h>         // MFC OLE classes

#include <shlobj.h>
#include <ole2.h>


#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif


#include "wx/evtloop.h"

#include <optional>
// A dummy CWnd pointing to a wxWindow's HWND
class CDummyWindow : public CWnd
{
public:
    CDummyWindow(HWND hWnd);
    ~CDummyWindow(void);
};



struct SNIAfxDropSource : COleDataSource
{
    DECLARE_DYNAMIC(SNIAfxDropSource)
    // todo stop drag

    using GetFileHandler = std::function<
        std::optional<std::vector<uint8_t>>(const std::string&)
    >;
    SNIAfxDropSource(std::vector<std::string> files,
        GetFileHandler get_file_handler) 
        :
            get_file_handler(get_file_handler),
            files(files)
    {

    }

    BOOL OnRenderFileData(
        LPFORMATETC lpFormatEtc, CFile* pFile)
    {
        // We only need to handle CFSTR_FILECONTENTS
        if (lpFormatEtc->cfFormat ==
            RegisterClipboardFormat(CFSTR_FILECONTENTS))
        {
            if (lpFormatEtc->tymed & (TYMED_FILE | TYMED_ISTREAM))
            {
                auto bytes = get_file_handler(files[lpFormatEtc->lindex]);
                if (bytes)
                {
                    pFile->Write(&*bytes->begin(), bytes->size());
                    // Need to return TRUE to indicate success to Explorer
                    return TRUE;
                }
            }
        }
        return COleDataSource::OnRenderFileData(
            lpFormatEtc, pFile);
    }
    std::vector<std::string> files;
    GetFileHandler get_file_handler;

};
class SNFileTree;
struct SNWinDropFeedback : COleDropSource
{
    SNWinDropFeedback(SNFileTree* tree) : tree(tree)
    {

    }

    virtual SCODE QueryContinueDrag(
        BOOL bEscapePressed,
        DWORD dwKeyState);

    SNFileTree* tree;
    bool returned_to_window_ = false;

};


#endif