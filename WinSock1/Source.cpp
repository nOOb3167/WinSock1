#include <exception>
#include <memory>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

bool SocketErrorWouldBlock()
{
	int e = WSAGetLastError();
	return e == WSAEWOULDBLOCK || e == WSAEINTR;
}

class WinsockWrap
{
	WSADATA wsd;
public:
	WinsockWrap()
	{
		int r = 0;
		try {
			if (r = WSAStartup(MAKEWORD(2, 2), &wsd) || LOBYTE(wsd.wVersion) != 2 || HIBYTE(wsd.wVersion) != 2)
				throw exception("Initializing Winsock2");
		} catch (exception &) {
			if (!r) WSACleanup();
			throw;
		}
	}

	~WinsockWrap()
	{
		WSACleanup();
	}
};

int main()
{
	WinsockWrap ww;

	struct addrinfo *res = nullptr;
	struct addrinfo hints = {0};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_PASSIVE;

	SOCKET listen_sock = INVALID_SOCKET;

	try {

	if (getaddrinfo(nullptr, "27010", &hints, &res))
		throw exception("Getaddrinfo");

	if ((listen_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == INVALID_SOCKET)
		throw exception("Socket creation");

	if (bind(listen_sock, res->ai_addr, res->ai_addrlen) == SOCKET_ERROR)
		throw exception("Socket bind");

	if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR)
		throw exception("Socket listen");

	u_long blockmode = 1;
	if (ioctlsocket(listen_sock, FIONBIO, &blockmode) != NO_ERROR)
		throw exception("Socket nonblocking mode");

	SOCKET clt_sock = INVALID_SOCKET;

	if ((clt_sock = accept(listen_sock, nullptr, nullptr)) == INVALID_SOCKET)
		if (!SocketErrorWouldBlock())
			throw exception("Socket accept");

	} catch (exception &) {
		if (res) freeaddrinfo(res);
		if (listen_sock != INVALID_SOCKET) closesocket(listen_sock);
		throw;
	}

	return EXIT_SUCCESS;
}
