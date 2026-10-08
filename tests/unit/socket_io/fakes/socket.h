// fake socket.h: the Socket / Address / SocketException API socket_io.cxx uses.
// Behaviour is scripted per test through fake_ctl (fake_ctl.h).
#pragma once
#include <string>
#include <exception>
#include <atomic>
class SocketException : public std::exception {
public:
	SocketException(int err_ = 0, const std::string &m = "fake socket error") : err(err_), msg(m) {}
	~SocketException() throw() {}
	const char *what(void) const throw() { return msg.c_str(); }
	int error(void) const { return err; }
private:
	int err; std::string msg;
};
class Address {
public:
	Address(const char *host = "", int port = 0, const char *proto = "tcp");
	Address(const char *host, const char *port_name, const char *proto = "tcp");
	~Address();
	std::string node, service;
};
class Socket {
public:
	Socket(const Address &addr);
	Socket(int fd = -1);
	~Socket();
	void close(void);
	void connect(const Address &addr);
	void connect(void);
	size_t send(const std::string);
	size_t recv(std::string &buf);
	void set_nonblocking(bool v = true);
	void set_timeout(double t);
	int fd(void);
	// test-only
	std::atomic<int> sockfd;
	long canary;             // read after a blocking recv(): ASan sees use-after-free
};
