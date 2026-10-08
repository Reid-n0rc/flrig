// fake status.h: only the progStatus fields socket_io.cxx reads
#pragma once
#include <string>
struct status {
	std::string tcpip_addr = "127.0.0.1";
	std::string tcpip_port = "4001";
	int tcpip_reconnect_after = 1;
	int tcpip_drops_allowed = 3;
	int serloop_timing = 50;
};
extern status progStatus;
