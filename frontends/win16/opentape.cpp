#include "wx_prec.h"

#ifndef WX_PRECOMP
#include "wx.h"
#endif

#include <stdio.h>

extern "C" {
#include "opentape/backend.h"
}

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

    frame = new OpenTapeFrame(NULL, "OpenTape");
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
    fileMenu->Append(OT_MENU_EXIT, "E&xit");
    helpMenu = new wxMenu;
    helpMenu->Append(OT_MENU_ABOUT, "&About OpenTape...");
    menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(helpMenu, "&Help");
    SetMenuBar(menuBar);

    panel = new wxPanel(this, 0, 0, 520, 330);
    new wxMessage(panel, "OpenTape", 18, 18);
    new wxMessage(panel,
                  "Backup, restore, verify and catalogue for SCSI tape",
                  18, 48);

    new wxButton(panel, NULL, "Backup...", 18, 88, 105, 30);
    new wxButton(panel, NULL, "Restore...", 132, 88, 105, 30);
    new wxButton(panel, NULL, "Verify...", 246, 88, 105, 30);
    new wxButton(panel, NULL, "Catalogue...", 360, 88, 120, 30);

    new wxButton(panel, NULL, "Media...", 18, 132, 105, 30);
    new wxButton(panel, NULL, "Settings...", 132, 132, 105, 30);

    backend = ot_backend_select(0u, OT_BACKEND_AUTO);
    sprintf(status, "Detected backend: %s", ot_backend_name(backend));
    new wxMessage(panel, status, 18, 195);
    new wxMessage(panel,
                  "Bootstrap build: tape access is not implemented yet.",
                  18, 225);
}

void OpenTapeFrame::OnMenuCommand(int id)
{
    switch (id) {
    case OT_MENU_EXIT:
        delete this;
        break;
    case OT_MENU_ABOUT:
        wxMessageBox("OpenTape 0.1 bootstrap\nGPL version 3",
                     "About OpenTape", wxOK | wxCENTRE, this);
        break;
    }
}
