#include "win_dnd.h"
#include "sn_file_tree.h"

#ifdef WIN_DROP_SOURCE


//IMPLEMENT_DYNAMIC(SNIAfxDropSource, COleDataSource);

SCODE SNWinDropFeedback::QueryContinueDrag(
    BOOL bEscapePressed,
    DWORD dwKeyState)
{
    if (tree->IsMouseInWindow())
    {
        returned_to_window_ = true;
        return DRAGDROP_S_CANCEL;
    }
    return COleDropSource::QueryContinueDrag(bEscapePressed, dwKeyState);
}


// Dummy MFC window for specifying a valid main window to MFC, using
// a wxWidgets HWND.
CDummyWindow::CDummyWindow(HWND hWnd)
{
    Attach(hWnd);
}

// Don't let the CWnd destructor delete the HWND
CDummyWindow::~CDummyWindow()
{
    Detach();
}

CWinApp dummyMFCApp;

#endif