// fake rigpanel.h: the four widgets socket_io.cxx touches, as recording test
// doubles (FakeWidget, fake_ctl.h) instead of Fl_Box / Fl_Group. The variable
// names are flrig's, so socket_io.cxx compiles unchanged; no FLTK widget code
// is linked.
#pragma once
#include "fake_ctl.h"
extern FakeWidget *box_tcpip_connect;
extern FakeWidget *box_xcvr_connect;
extern FakeWidget *tcpip_menu_box;
extern FakeWidget *tcpip_box;
