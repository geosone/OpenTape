#include "wx_prec.h"

#ifndef WX_PRECOMP
#include "wx.h"
#endif

#include <stdio.h>

extern "C" {
#include "opentape/backend.h"
}
#include "opentape/messages.h"

class OpenTapeApp: public wxApp
{
public:
    wxFrame *OnInit(void);
};

class OpenTapeFrame: public wxFrame
{
public:
    OpenTapeFrame(wxFrame *parent, char *title);
    void OnMenuCommand(int id);
    Bool OnClose(void) { return TRUE; }
};

#define OT_MENU_EXIT  1
#define OT_MENU_ABOUT 2

OpenTapeApp openTapeApp;
IMPLEMENT_WXWIN_MAIN

wxFrame *OpenTapeApp::OnInit(void)
{
    OpenTapeFrame *frame;

    frame = new OpenTapeFrame(NULL, OT_MSG_APP_TITLE);
    frame->Show(TRUE);
    return frame;
}

OpenTapeFrame::OpenTapeFrame(wxFrame *parent, char *title):
    wxFrame(parent, title, 40, 30, 520, 330)
{
    enum ot_backend_kind backend;
    char status[96];
    wxMenu *fileMenu;
    wxMenu *helpMenu;
    wxMenuBar *menuBar;
    wxPanel *panel;

    fileMenu = new wxMenu;
    fileMenu->Append(OT_MENU_EXIT, OT_MSG_MENU_EXIT);
    helpMenu = new wxMenu;
    helpMenu->Append(OT_MENU_ABOUT, OT_MSG_MENU_ABOUT);
    menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, OT_MSG_MENU_FILE);
    menuBar->Append(helpMenu, OT_MSG_MENU_HELP);
    SetMenuBar(menuBar);

    panel = new wxPanel(this, 0, 0, 520, 330);
    new wxMessage(panel, OT_MSG_APP_TITLE, 18, 18);
    new wxMessage(panel, OT_MSG_TASK_SUMMARY_LONG, 18, 48);

    new wxButton(panel, NULL, OT_MSG_TASK_BACKUP, 18, 88, 140, 30);
    new wxButton(panel, NULL, OT_MSG_TASK_RESTORE, 176, 88, 140, 30);
    new wxButton(panel, NULL, OT_MSG_TASK_VERIFY, 334, 88, 140, 30);

    new wxButton(panel, NULL, OT_MSG_TASK_CATALOGUE, 18, 132, 140, 30);
    new wxButton(panel, NULL, OT_MSG_TASK_MEDIA, 176, 132, 140, 30);
    new wxButton(panel, NULL, OT_MSG_TASK_SETTINGS, 334, 132, 140, 30);

    backend = ot_backend_select(0u, OT_BACKEND_AUTO);
    sprintf(status, OT_MSG_BACKEND_FMT, ot_backend_name(backend));
    new wxMessage(panel, status, 18, 195);
    new wxMessage(panel, OT_MSG_NOT_IMPLEMENTED, 18, 225);
}

void OpenTapeFrame::OnMenuCommand(int id)
{
    switch (id) {
    case OT_MENU_EXIT:
        delete this;
        break;
    case OT_MENU_ABOUT:
        wxMessageBox(OT_MSG_ABOUT_TEXT, OT_MSG_ABOUT_TITLE,
                     wxOK | wxCENTRE, this);
        break;
    }
}
