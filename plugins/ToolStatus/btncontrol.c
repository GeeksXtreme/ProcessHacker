#include "toolstatus.h"
#include "btncontrol.h"

BOOLEAN InsertButton(
    __in HINSTANCE DllBase,
    __in HWND WindowHandle, 
    __in UINT uCmdId, 
    __in INT nSize
    )
{
    NC_CONTROL* context;
    
    context = (NC_CONTROL*)PhAllocate(sizeof(NC_CONTROL));
    memset(context, 0, sizeof(NC_CONTROL));

    context->uCmdId = uCmdId;
    context->IsButtonDown = FALSE;
    context->nButSize = nSize;
    context->DllBase = DllBase;
    context->ImageList = ImageList_Create(22, 22, ILC_COLOR32 | ILC_MASK, 0, 0);
    
    // Set the number of images.
    ImageList_SetImageCount(context->ImageList, 2);

    PhSetImageListBitmap(context->ImageList, 0, (HINSTANCE)PluginInstance->DllBase, MAKEINTRESOURCE(IDB_SEARCH1));
    PhSetImageListBitmap(context->ImageList, 1, (HINSTANCE)PluginInstance->DllBase, MAKEINTRESOURCE(IDB_SEARCH2));;

    // replace the old window procedure with our new one
    context->NCAreaWndProc = SubclassWindow(WindowHandle, InsButProc);

    // associate our button state structure with the window
    SetProp(WindowHandle, L"Context", (HANDLE)context);

    // force the edit control to update its non-client area
    SetWindowPos(
        WindowHandle,
        0, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER
        );

    return TRUE;
}

VOID GetButtonRect(
    __inout NC_CONTROL* nc,
    __in RECT* rect
    )
{
    // retrieve the coordinates of an inserted button, given the specified window rectangle. 
    rect->right -= nc->cxRightEdge;
    rect->top += nc->cyTopEdge;
    rect->bottom -= nc->cyBottomEdge;
    rect->left = rect->right - nc->nButSize;

    if (nc->cxRightEdge > nc->cxLeftEdge)
        OffsetRect(rect, nc->cxRightEdge - nc->cxLeftEdge, 0);
}   

VOID RedrawNC(
    __in HWND hwnd
    )
{
    SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_DRAWFRAME|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
}

VOID DrawInsertedButton(
    __in HWND hwnd, 
    __inout NC_CONTROL* nc, 
    __in RECT* prect
    )
{
    HDC hdc;

    hdc = GetWindowDC(hwnd);
    
    SetBkMode(hdc, TRANSPARENT);
    FillRect(hdc, prect, GetSysColorBrush(COLOR_BTNFACE));    

    if (nc->ImageList)
    {
        if (nc->IsMouseDown)
        {
            ImageList_Draw(
                nc->ImageList, 
                0, 
                hdc, 
                prect->left, 
                prect->top, 
                ILD_NORMAL | ILD_TRANSPARENT
                );
        }
        else
        {
            ImageList_Draw(
                nc->ImageList, 
                1, 
                hdc, 
                prect->left, 
                prect->top, 
                ILD_NORMAL | ILD_TRANSPARENT
                );
        }
    }
        
    ReleaseDC(hwnd, hdc);
}

LRESULT CALLBACK InsButProc(
    __in HWND hwndDlg,
    __in UINT uMsg,
    __in WPARAM wParam,
    __in LPARAM lParam
    )
{
    NC_CONTROL* context = (NC_CONTROL*)GetProp(hwndDlg, L"Context");

    if (!context || !context->NCAreaWndProc)
        return FALSE;

    if (uMsg == WM_DESTROY)
    {
        RemoveProp(hwndDlg, L"Context");     
        PhFree(context);
        context = NULL;
    }
    
    if (!context || !context->NCAreaWndProc)
        return FALSE;

    switch (uMsg)
    {
    case WM_NCCALCSIZE:
        {
            context->prect = (RECT*)lParam;
            context->oldrect = *context->prect;

            // let the old wndproc allocate space for the borders, or any other non-client space.
            //CallWindowProc(context->NCAreaWndProc, hwndDlg, uMsg, wParam, lParam);

            // calculate what the size of each window border is,
            // we need to know where the button is going to live.
            context->cxLeftEdge = context->prect->left - context->oldrect.left; 
            context->cxRightEdge = context->oldrect.right - context->prect->right;
            context->cyTopEdge = context->prect->top - context->oldrect.top;
            context->cyBottomEdge = context->oldrect.bottom - context->prect->bottom;   

            // now we can allocate additional space by deflating the
            // rectangle even further. Our button will go on the right-hand side,
            // and will be the same width as a scrollbar button
            context->prect->right -= context->nButSize;
        }
        return FALSE;
    case WM_NCPAINT:
        {  
            // let the old window procedure draw the borders other non-client bits-and-pieces for us.
            //CallWindowProc(context->NCAreaWndProc, hwndDlg, uMsg, wParam, lParam);

            // get the screen coordinates of the window.
            GetWindowRect(hwndDlg, &context->rect);
            // adjust the coordinates so they start from 0,0
            OffsetRect(&context->rect, -context->rect.left, -context->rect.top);    
            // work out where to draw the button
            GetButtonRect(context, &context->rect);

            DrawInsertedButton(hwndDlg, context, &context->rect);
        }
        return FALSE;
    case WM_NCHITTEST:
        {
            // get the screen coordinates of the mouse
            context->pt.x = GET_X_LPARAM(lParam);
            context->pt.y = GET_Y_LPARAM(lParam);

            // get the position of the inserted button
            GetWindowRect(hwndDlg, &context->rect);
            GetButtonRect(context, &context->rect);

            // check that the mouse is within the inserted button
            if (PtInRect(&context->rect, context->pt))
                return HTBORDER;
        }
        break;
    case WM_NCLBUTTONDBLCLK:
    case WM_NCLBUTTONDOWN:
        {
            // get the screen coordinates of the mouse
            context->pt.x = GET_X_LPARAM(lParam);
            context->pt.y = GET_Y_LPARAM(lParam);

            // get the position of the inserted button
            GetWindowRect(hwndDlg, &context->rect);
            
            context->pt.x -= context->rect.left;
            context->pt.y -= context->rect.top;

            OffsetRect(&context->rect, -context->rect.left, -context->rect.top);

            GetButtonRect(context, &context->rect);

            // check that the mouse is within the inserted button
            if (PtInRect(&context->rect, context->pt))
            {
                SetCapture(hwndDlg);

                context->IsButtonDown = TRUE;
                context->IsMouseDown = TRUE;

                //redraw the non-client area to reflect the change
                DrawInsertedButton(hwndDlg, context, &context->rect);
            }
        }
        break;
    case WM_MOUSEMOVE:
        {
            if (!context->IsMouseDown)
                break;

            // get the SCREEN coordinates of the mouse
            context->pt.x = GET_X_LPARAM(lParam);
            context->pt.y = GET_Y_LPARAM(lParam);

            ClientToScreen(hwndDlg, &context->pt);

            // get the position of the inserted button
            GetWindowRect(hwndDlg, &context->rect);

            context->pt.x -= context->rect.left;
            context->pt.y -= context->rect.top;

            OffsetRect(
                &context->rect, 
                -context->rect.left, 
                -context->rect.top
                );

            GetButtonRect(context, &context->rect);

            context->oldstate = context->IsButtonDown;

            // check that the mouse is within the inserted button
            if (PtInRect(&context->rect, context->pt))
                context->IsButtonDown = TRUE;
            else
                context->IsButtonDown = FALSE;

            // redraw the non-client area to reflect the change.
            // to prevent flicker, we only redraw the button if its state has changed
            if (context->oldstate != context->IsButtonDown)
                DrawInsertedButton(hwndDlg, context, &context->rect);
        }
        break;
    case WM_LBUTTONUP:
        {
            if (!context->IsMouseDown)
                break;

            // get the SCREEN coordinates of the mouse
            context->pt.x = GET_X_LPARAM(lParam);
            context->pt.y = GET_Y_LPARAM(lParam);
            
            ClientToScreen(hwndDlg, &context->pt);

            // get the position of the inserted button
            GetWindowRect(hwndDlg, &context->rect);

            context->pt.x -= context->rect.left;
            context->pt.y -= context->rect.top;

            OffsetRect(
                &context->rect, 
                -context->rect.left, 
                -context->rect.top
                );

            GetButtonRect(context, &context->rect);
            
            // check that the mouse is within the inserted button
            if (PtInRect(&context->rect, context->pt))
            {
                PostMessage(
                    GetParent(hwndDlg), 
                    WM_COMMAND, 
                    MAKEWPARAM(context->uCmdId, BN_CLICKED), 
                    0
                    );
            }
                    
            ReleaseCapture();

            context->IsButtonDown = FALSE;
            context->IsMouseDown = FALSE;

            // redraw the non-client area to reflect the change.
            DrawInsertedButton(hwndDlg, context, &context->rect);
        }
        break;
    }

    return CallWindowProc(context->NCAreaWndProc, hwndDlg, uMsg, wParam, lParam);
}