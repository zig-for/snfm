#pragma once
#ifdef WIN32
#define WIN_DROP_SOURCE 
#endif
#ifdef WIN_DROP_SOURCE
#include <afx.h>
#include <afxole.h>         // MFC OLE classes

#include <shlobj.h>
#include <ole2.h>
#include <afxadv.h>


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
    //DECLARE_DYNAMIC(SNIAfxDropSource)
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
#if 0
    BOOL OnRenderData(LPFORMATETC lpFormatEtc, LPSTGMEDIUM
        lpStgMedium)
    {
        BOOL bReturn = FALSE;
        if (lpFormatEtc->cfFormat ==
            RegisterClipboardFormat(CFSTR_FILECONTENTS))
        {
            HRESULT hr = S_FALSE;
            //Create an instance of CIStreamImpl which implements IStream
           // CIStreamImpl* pStreamImpl = new CIStreamImpl();
            //hr = pStreamImpl->QueryInterface(IID_IStream, (void FAR *
             //   FAR*) & pStreamImpl);
            if (SUCCEEDED(hr))
            {
                lpStgMedium->tymed = TYMED_ISTREAM;
                lpStgMedium->pstm = pStreamImpl;
                lpStgMedium->pUnkForRelease = NULL;
                bReturn = TRUE;//Set the return value
            }
            else
                pStreamImpl->Release();
        }
        /*
        else
            if (lpFormatEtc->cfFormat == g_cfFileGroupDescriptor)
            {
                lpStgMedium->tymed = TYMED_HGLOBAL;
                lpStgMedium->hGlobal = CreateFileGroupDescriptor();
                bReturn = TRUE; //Set the return value
            }*/
        return bReturn;
    }
#endif
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
                    assert(pFile->IsKindOf(RUNTIME_CLASS(CSharedFile)));
                    CSharedFile* pSharedFile = static_cast<CSharedFile*>(pFile);
                    HGLOBAL h = GlobalAlloc(0, bytes->size());
                    pSharedFile->SetHandle(h, 0);
                    pSharedFile->SetLength(0);
                    pSharedFile->SeekToBegin();
                    pSharedFile->Write(&*bytes->begin(), bytes->size());
                    
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