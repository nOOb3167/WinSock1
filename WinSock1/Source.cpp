#include <cassert>
#include <cstdio>

#include <exception>
#include <string>
#include <memory>
#include <vector>
#include <deque>
#include <algorithm>
#include <iterator>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define MEHTHROW(s) (::std::runtime_error((s ## " " ## __FILE__ ## " ") + ::std::to_string(__LINE__)))

#define MAGIC_READ_SIZE 1024

using namespace std;

/////////////////////////////////////////////
#if 0

namespace Socket
{

typedef uint32_t Stamp;

Stamp  EmptyStamp()
{
	return 0xBBAACCFF;
}

bool ErrorWouldBlock()
{
	int e = WSAGetLastError();
	return e == WSAEWOULDBLOCK || e == WSAEINTR || e == WSAEINPROGRESS;
}

class FrameState
{

};

class Fragment
{
public:
	Stamp stamp;
	string data;
};

class Primitive
{
public:
	// Should be deque
	deque<Fragment> in;
	deque<Fragment> out;
private:
	unique_ptr<SOCKET> rawsock;
public:
	Primitive(unique_ptr<SOCKET> s)
	{
		rawsock = move(s);
	}

	~Primitive()
	{
		closesocket(*rawsock);
	}

	void NetworkActivity()
	{
		// Writes
		{
			bool blocked = false;

			while (!(blocked || out.empty())) {
				Fragment fragment = out.front();

				int nsent = send(*rawsock, fragment.data.c_str(), fragment.data.size(), 0);

				if (nsent != SOCKET_ERROR)
				{
					Fragment newfrag(fragment);
					newfrag.data = newfrag.data.substr(nsent, newfrag.data.npos);

					out.pop_front();

					if (!newfrag.data.empty())
						out.push_front(newfrag);
				}
				else if (ErrorWouldBlock())
					blocked = true;
				else
					throw MEHTHROW("Send");
			}
		}

		// Reads
		{
			bool blocked = false;

			while (!(blocked || out.empty())) {
				char *incoming_c = new char[MAGIC_READ_SIZE];

				int nread = recv(*rawsock, incoming_c, MAGIC_READ_SIZE, 0);

				string incoming;
				if (nread != SOCKET_ERROR && nread != 0)
					incoming = string(incoming, incoming + nread);
				delete[] incoming_c;

				if (nread == 0)
					assert (false && "Closed normally - Not handled TODO");
				if (nread != SOCKET_ERROR)
				{
					// TODO: Empty stamp
					Fragment newfrag;
					newfrag.stamp = EmptyStamp();
					newfrag.data = incoming;

					in.push_back(newfrag);
				}
				else if (ErrorWouldBlock())
					blocked = true;
				else
					throw MEHTHROW("Send");
			}
		}
	}
};

class Cursor
{
	Stamp stamp;
};

class Managed
{
	Primitive prim;
	vector<Cursor> cursor;
};

class PrimitiveListening
{
	SOCKET sock;

public:

	PrimitiveListening()
	{
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

			freeaddrinfo(res);

			sock = listen_sock;

		} catch (exception &) {
			if (res) freeaddrinfo(res);
			if (listen_sock != INVALID_SOCKET) closesocket(listen_sock);
			throw;
		}
	}

	~PrimitiveListening()
	{
		closesocket(sock);
	}

	vector<shared_ptr<Primitive>> Accept()
	{
		vector<unique_ptr<SOCKET>> w;

		bool blocked = false;

		while (!blocked) {
			SOCKET s = accept(sock, nullptr, nullptr);

			if (s != INVALID_SOCKET)
				w.push_back(unique_ptr<SOCKET>(new SOCKET(s)));
			else if (ErrorWouldBlock())
				blocked = true;
			else
				throw MEHTHROW("Accept");
		}

		vector<shared_ptr<Primitive>> r;

		transform(w.begin(), w.end(), back_inserter(r), [](unique_ptr<SOCKET> &p) {
			return make_shared<Primitive>(move(p));
		});

		return r;
	}
};

class PrimitiveManager
{
	PrimitiveListening list;
	vector<shared_ptr<Primitive>> prim;

public:

	PrimitiveManager()
	{
	}

	void FrameActivity()
	{
		vector<shared_ptr<Primitive>> w = list.Accept();

		vector<shared_ptr<Primitive>> merged;
		merged.insert(merged.end(), prim.begin(), prim.end());
		merged.insert(merged.end(), w.begin(), w.end());

		for (auto i : merged) {

		}
	}

private:

	vector<shared_ptr<Primitive>> Accept()
	{
	}

};

};

#endif
/////////////////////////////////////////////

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

namespace S2 {

	typedef uint32_t Stamp;

	#define POLL_VAL_DUMMY (-1)

	class PollFdType { public:
		SOCKET socketfd;
		static PollFdType Make(SOCKET s) { PollFdType w; w.socketfd = s; return w; }
		static void ExtractInto(const PollFdType& fd, pollfd *pfd) { pfd->fd = fd.socketfd; }
		static bool IsDummy(SOCKET s) { return s == POLL_VAL_DUMMY; }
	};

	class NetExc : ::std::exception {};
	class NetFailureExc : NetExc {};
	class NetBlockExc : NetExc {};

	bool ErrorWouldBlock() {
		int e = WSAGetLastError();
		return e == WSAEWOULDBLOCK || e == WSAEINTR || e == WSAEINPROGRESS;
	}

	class Fragment { public:
		Stamp stamp;
		string data;
	};

	class FragmentTransfer { public:
		deque<Fragment> write;
		deque<Fragment> read;
	};

	class PrimitiveBase { public:
		virtual void WriteU(deque<Fragment>* w) = 0;
		virtual void ReadU(deque<Fragment>* w) = 0;
		virtual PollFdType GetPollFd() { return PollFdType::Make(POLL_VAL_DUMMY); }
	};

	class PrimitiveMemonly : public PrimitiveBase { public:
		deque<Fragment> write;
		deque<Fragment> read;
		void WriteU(deque<Fragment>* w) {
			write.insert(write.end(), w->begin(), w->end());
			w->clear();
		}
		void ReadU(deque<Fragment>* w) {
			if (read.empty()) return;
			w->push_back(read.front());
			read.pop_front();
		}
	};

	class PrimitiveSock : public PrimitiveBase { public:
		PollFdType s;
		PrimitiveSock(PollFdType s) : s(s) {}

		virtual void WriteU(deque<Fragment>* w) { assert(0); }
		virtual void ReadU(deque<Fragment>* w) { assert(0); }
		virtual PollFdType GetPollFd() { return s; }	
	};

	class PrimitiveZombie : public PrimitiveBase { public:
		void WriteU(deque<Fragment>* w) { throw NetBlockExc(); }
		void ReadU(deque<Fragment>* w) { throw NetBlockExc(); }
	};

	class ManagedBase { public:
		deque<Fragment> in, out;
	};

	class Managed { public:
		shared_ptr<PrimitiveBase> prim;
		deque<Fragment> in, out;
		void Transfer() {
			try { prim->ReadU(&in); } catch (NetBlockExc &e) {}
			try { prim->WriteU(&out); } catch (NetBlockExc &e) {}
		}
	};

	class ManagedSock : public ManagedBase { public:
	};

	class Poller { public:
		vector<pollfd> pfd;

		size_t Size() {
			return pfd.size();
		}

		void PushBack(shared_ptr<PrimitiveSock> ps) {
			assert(!PollFdType::IsDummy(ps->GetPollFd().socketfd));
			pfd.push_back(pollfd());
			PollFdType::ExtractInto(ps->GetPollFd(), &pfd.data()[pfd.size() - 1]);
		};

		void ReadyForPoll() {
			for (size_t i = 0; i < pfd.size(); i++) {
				pfd[i].events = POLLIN | POLLOUT;
				pfd[i].revents = 0;
			}
		};

	};

	typedef vector<pair<weak_ptr<ManagedSock>, deque<Fragment>> > ManagedGroupStagedRead_t;

	void StagedReadApply(const ManagedGroupStagedRead_t& srt) {
		for (size_t i = 0; i < srt.size(); i++) {
			shared_ptr<ManagedSock> w = srt[i].first.lock(); assert(w);
			w->in.insert(w->in.end(), srt[i].second.begin(), srt[i].second.end());
		}
	};

	class ManagedGroup { public:
		vector<weak_ptr<PrimitiveSock> > m_sockp;
		vector<weak_ptr<ManagedSock> > m_sockm;
		Poller m_poller;
		
		vector<shared_ptr<Managed> > m_rest;

		shared_ptr<ManagedSock> RegSock(shared_ptr<PrimitiveSock> ps) {
			shared_ptr<ManagedSock> w = make_shared<ManagedSock>();
			m_sockp.push_back(ps);
			m_sockm.push_back(w);
			m_poller.PushBack(ps);
			return w;
		}

		void AssurePfd() {
			assert(m_sockp.size() == m_sockm.size() == m_poller.Size());
			// Sticking inactive Pfds into the Poller function is legitimate, not needed
			// FIXME: Should probably be pruning expired-weakptr entries though
			for (size_t i = 0; i < m_sockp.size(); i++) {
			}
		};

		shared_ptr<ManagedGroupStagedRead_t> StagedRead() {
			shared_ptr<ManagedGroupStagedRead_t> ret = make_shared<ManagedGroupStagedRead_t>();

			AssurePfd();
			m_poller.ReadyForPoll();

			int r = WSAPoll(m_poller.pfd.data(), m_poller.Size(), 0);
			if (r == SOCKET_ERROR) throw NetFailureExc();
			if (r > 0) {
				for (size_t i = 0; i < m_poller.Size(); i++) {
					if (m_poller.pfd[i].revents & (POLLIN | POLLOUT)) {
						shared_ptr<PrimitiveSock> t = m_sockp[i].lock(); assert(t);
						deque<Fragment> w;
						try { t->ReadU(&w); } catch (NetBlockExc &e) {};

						/* Push a dummy deque first, then swap ReadU's data in */
						ret->push_back(make_pair(m_sockm[i], deque<Fragment>()));
						ret->at(ret->size() - 1).second.swap(w);
					}
				}
			}

			return ret;
		}

		vector<shared_ptr<Managed> > m;

		void TransferAll() { for (auto w : m) w->Transfer(); }

		void Transfer() {
			unique_ptr<pollfd[]> pfd(new pollfd[m.size()]);

			for (size_t i = 0; i < m.size(); i++) {
				PollFdType::ExtractInto(m[i]->prim->GetPollFd(), &pfd[i]);
				pfd[i].events = POLLIN | POLLOUT;
				pfd[i].revents = 0;
			}

			bool allDummy = true;
			for (size_t i = 0; i < m.size(); i++) if (!PollFdType::IsDummy(pfd[i].fd)) allDummy = false;

			int r;
			if (allDummy) r = 1; else r = WSAPoll(&pfd[0], m.size(), 0);
			if (r == SOCKET_ERROR) throw NetFailureExc();
			if (r > 0) {
				for (size_t i = 0; i < m.size(); i++) {
					if (PollFdType::IsDummy(pfd[i].fd) || (pfd[i].revents & (POLLIN | POLLOUT)))
						try { m[i]->Transfer(); } catch (NetBlockExc &e) {};
				}
			}
		}
	};

	shared_ptr<ManagedSock> MakeMgdSkt(shared_ptr<ManagedGroup> mg, shared_ptr<PrimitiveSock> ps) {
		return mg->RegSock(ps);
	}

	class PrimitiveListening { public:
		SOCKET sock;

		PrimitiveListening() {
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

				freeaddrinfo(res);

				sock = listen_sock;

			} catch (exception &) {
				if (res) freeaddrinfo(res);
				if (listen_sock != INVALID_SOCKET) closesocket(listen_sock);
				throw;
			}
		}

		~PrimitiveListening()
		{
			closesocket(sock);
		}

		vector<shared_ptr<PrimitiveSock>> Accept()
		{
			vector<SOCKET> w;

			bool blocked = false;

			while (!blocked) {
				SOCKET s = accept(sock, nullptr, nullptr);

				if (s != INVALID_SOCKET)
					w.push_back(SOCKET(s));
				else if (ErrorWouldBlock())
					blocked = true;
				else
					throw exception("Accept");
			}

			vector<shared_ptr<PrimitiveSock>> r;

			transform(w.begin(), w.end(), back_inserter(r), [](SOCKET &p) {
				return make_shared<PrimitiveSock>(PollFdType::Make(p));
			});

			return r;
		}
	};

};

namespace S3 {

	using namespace S2;

	enum class PipeType {
		Packet
	};

	class Pipe { public:
		PipeType pt;
	};

	class PipePacket : public Pipe {
	public:
		shared_ptr<PrimitiveBase> pb;
		shared_ptr<ManagedBase> mgd;
	};

	//Only worthwile member of Prim is GetPollFD?
	//MakeMgdSkt(MgdGroup, Prim) -> MgdGroup.Reg(Prim)
	//MakeMgdMemonly() -> Nothing
	//If going for a cursor design, mgd is probably it
	//Maybe: No call downs into Prim, MgdGroup injects into Mgd queue OR stages, to be applied
	//  Stages, bond by WeakPtr
	//Mgd multiple groups: SockBased, EveryFrameCallbackBased

	class PipeMaker { public:
	static shared_ptr<PipePacket> MakePacket(shared_ptr<PrimitiveBase> pb, shared_ptr<ManagedBase> mgd) {
		shared_ptr<PipePacket> pl = make_shared<PipePacket>();
		pl->pt = PipeType::Packet;
		pl->pb = pb;
		pl->mgd = mgd;
		return pl;
	}
	};

};

int main()
{
	WinsockWrap ww;

	namespace S = S2;

	shared_ptr<S::Managed> mm(new S::Managed);
	auto w1 = make_shared<S::PrimitiveMemonly>();
	for (int i = 0; i < 5; i++) w1->read.push_back(S::Fragment());
	mm->prim = w1; 
	shared_ptr<S::Managed> mz(new S::Managed); mz->prim = make_shared<S::PrimitiveZombie>();

	S::ManagedGroup g; g.m.push_back(mm); g.m.push_back(mz);

	for (int i = 0; i < 5; i++) g.Transfer();

	{
		auto pl = make_shared<S::PrimitiveListening>();
		auto mg = make_shared<S::ManagedGroup>();

		vector<shared_ptr<S::PrimitiveSock> > svec;
		while(!(svec = pl->Accept()).size()) {}

		auto ms = S::MakeMgdSkt(mg, svec.at(0)); 

		while(1) mg->StagedRead();
	}

	return EXIT_SUCCESS;
}
