// fake socket_io.h: same declarations as src/include/socket_io.h
#pragma once
#include <string>
#include "threads.h"
#include "socket.h"
extern Socket *tcpip;
extern Address *remote_addr;
void connect_to_remote();
void disconnect_from_remote();
void send_to_remote(std::string cmd_string);
int  read_from_remote(std::string &str);
