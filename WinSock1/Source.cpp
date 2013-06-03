#include <cassert>
#include <cstdio>

#include <exception>
#include <string>
#include <memory>
#include <vector>
#include <deque>
#include <algorithm>
#include <iterator>
#include <numeric> /* accumulate */

#include <loginc.h>

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

			for (auto &i : merged) {

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
	WinsockWrap() {
		int r = 0;
		try {
			if (r = WSAStartup(MAKEWORD(2, 2), &wsd) || LOBYTE(wsd.wVersion) != 2 || HIBYTE(wsd.wVersion) != 2)
				throw exception("Initializing Winsock2");
		} catch (exception &) {
			if (!r) WSACleanup();
			throw;
		}
	}

	~WinsockWrap() {
		WSACleanup();
	}
};

namespace S2 {

	typedef uint32_t Stamp;

#define POLL_VAL_DUMMY (-1)

	class PollFdType {
	public:
		SOCKET socketfd;
		static PollFdType Make(SOCKET s) { PollFdType w; w.socketfd = s; return w; }
		static void ExtractInto(const PollFdType& fd, pollfd *pfd) { pfd->fd = fd.socketfd; }
		static bool IsDummy(SOCKET s) { return s == POLL_VAL_DUMMY; }
	};

	class NetExc : ::std::exception {};
	class NetFailureExc : NetExc {};
	class NetDisconnectExc : NetFailureExc {};
	class NetBlockExc : NetExc {};

	bool ErrorWouldBlock() {
		int e = WSAGetLastError();
		return e == WSAEWOULDBLOCK || e == WSAEINTR || e == WSAEINPROGRESS;
	}

	Stamp  EmptyStamp() {
		return 0xBBAACCFF;
	}

	class Fragment {
	public:
		Stamp stamp;
		string data;
		Fragment(const Stamp &s, const string &d) : stamp(s), data(d) {}
	};

	class PrimitiveBase {
	public:
		virtual void WriteU(deque<Fragment>* w) = 0;
		virtual void ReadU(deque<Fragment>* w) = 0;
		virtual PollFdType GetPollFd() { return PollFdType::Make(POLL_VAL_DUMMY); }
	};

	class PrimitiveMemonly : public PrimitiveBase {
	public:
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

	class PrimitiveSock : public PrimitiveBase {
	public:
		PollFdType s;

		PrimitiveSock(PollFdType s) : s(s) {}

		virtual void WriteU(deque<Fragment>* w) { assert(0); }
		virtual void ReadU(deque<Fragment>* w) {
			char buf[MAGIC_READ_SIZE];

			for (;;) {
				int r = recv(s.socketfd, buf, MAGIC_READ_SIZE, 0);
				if (r == 0)                throw NetDisconnectExc();
				if (r == SOCKET_ERROR)
					if (ErrorWouldBlock()) throw NetBlockExc();
					else                   throw NetFailureExc();

					w->push_back(Fragment(EmptyStamp(), string(buf, r)));

					LOG(INFO) << "Read " << string(buf, r);
			}
		}
		virtual PollFdType GetPollFd() { return s; }	
	};

	class PrimitiveZombie : public PrimitiveBase {
	public:
		void WriteU(deque<Fragment>* w) { throw NetBlockExc(); }
		void ReadU(deque<Fragment>* w) { throw NetBlockExc(); }
	};

	class ManagedBase {
	public:
		deque<Fragment> in, out;
	};

	class ManagedSock : public ManagedBase { 
	public:
		bool knownClosed;

		ManagedSock() : knownClosed(false) {}

		bool IsKnownClosed() { return knownClosed; }
		void SetKnownClosed() { knownClosed = true; }
	};

	class Poller { 
	public:
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

	class ManagedGroup {

		struct StagedReadEntry { size_t idx; weak_ptr<ManagedSock> ms; deque<Fragment> rd; };
		struct StagedDisconnectEntry { size_t idx; weak_ptr<ManagedSock> ms; bool gracefulp; };
		typedef vector<StagedReadEntry> StagedRead_t;
		typedef vector<StagedDisconnectEntry> StagedDisconnect_t;
		typedef struct { shared_ptr<StagedRead_t> r; shared_ptr<StagedDisconnect_t> d; } Staged_t;

	private:
		vector<weak_ptr<PrimitiveSock> > m_sockp;
		vector<weak_ptr<ManagedSock> > m_sockm;
		Poller m_poller;

		size_t mSockCnt;

		//vector<shared_ptr<Managed> > m_rest;

		void AddItem(shared_ptr<PrimitiveSock> ps, shared_ptr<ManagedSock> w) {
			int stage;
			try {
				stage = 0; m_sockp.push_back(ps);
				stage = 1; m_sockm.push_back(w);
				stage = 2; m_poller.PushBack(ps);
			} catch (exception &e) {
				try { switch (stage) { case 1: m_sockp.pop_back(); case 2: m_sockm.pop_back(); } } catch (exception &e) { LOG(FATAL) << "Recovery Failure"; };
				throw;
			}
			mSockCnt++;
		}

		void RemoveItemMulti_X(StagedRead_t &sre) {
			if (!sre.size()) return;
			if (!(sre.back().idx < mSockCnt)) throw exception("Attempt to remove nonexistent"); /* FIXME: */

			assert(mSockCnt == m_sockp.size() == m_sockm.size() == m_poller.Size());

			/**
			* Algorithm:
			* With the array remidx filled as remidx0(=sre[0].idx)..remidx1(=sre[n].idx)..remidxEND(=mSockCnt)
			* [0, remidx0) ; [(remidxn)+1 remidnext) ; Are the half-open ranges that need to be copied
			* The total number of copied elements is (mSockCnt - remidxs.size()) (All elements except the +1 lumpouts)
			*/
			vector<size_t> remidxs;

			for_each(sre.begin(), sre.end(), [&remidxs](StagedReadEntry &x) { remidxs.push_back(x.idx); });
			sort(remidxs.begin(), remidxs.end(), less<size_t>());
			remidxs.push_back(mSockCnt - 1); /* The extra remidx coming from mSockCnt instead of a sre.idx */

			vector<weak_ptr<PrimitiveSock> > newp(mSockCnt - remidxs.size());
			vector<weak_ptr<ManagedSock> > newm(mSockCnt - remidxs.size());
			vector<pollfd> newl(mSockCnt - remidxs.size());

			auto itp = m_sockp.begin(), ttp = newp.begin();
			auto itm = m_sockm.begin(), ttm = newm.begin();
			auto itl = m_poller.pfd.begin(), ttl = newl.begin();

			size_t ncopy;
			/* One range per iteration */
			for (size_t curidx = 0, tgtidx = 0; tgtidx < remidxs.size(); tgtidx++) {
				/* 'ncopy' and 'itp/m/l' iterators are at 'curidx' */
				/* As 'ncopy' is 'remidxs[tgtidx]-curidx', advancing 'curidx' and the iterators by 'ncopy+1' gets to '(remidxn)+1' */
				ncopy  = remidxs[tgtidx] - curidx;
				ttp = copy_n(itp, ncopy, ttp);
				ttm = copy_n(itm, ncopy, ttm);
				ttl = copy_n(itl, ncopy, ttl);
				itp+=ncopy+1; itm+=ncopy+1; itl+=ncopy+1;
				curidx+=ncopy+1;
			}
		}

		shared_ptr<ManagedSock> RegSock(shared_ptr<PrimitiveSock> ps) {
			shared_ptr<ManagedSock> w = make_shared<ManagedSock>();
			AddItem(ps, w);
			return w;
		}

		void AssureSockCnt() { size_t a = mSockCnt; assert(a == m_sockp.size() && a == m_sockm.size() && a == m_poller.Size()); }

	public:
		ManagedGroup() : mSockCnt(0) {}

		Staged_t StagedRead() {
			AssureSockCnt();

			shared_ptr<StagedRead_t>       retr = make_shared<StagedRead_t>();
			shared_ptr<StagedDisconnect_t> retd = make_shared<StagedDisconnect_t>();
			Staged_t ret = { retr, retd };

			/* Early return. Seems that as with 'select', empty 'poll' kills winsock */
			if (m_poller.Size() == 0) return ret;

			m_poller.ReadyForPoll();

			int r = WSAPoll(m_poller.pfd.data(), m_poller.Size(), 0);
			if (r == SOCKET_ERROR) throw NetFailureExc();

			if (r > 0) {
				for (size_t i = 0; i < m_poller.Size() && m_poller.pfd[i].revents & (POLLIN | POLLOUT); i++) {
					deque<Fragment> w;

					try {
						shared_ptr<PrimitiveSock> t = m_sockp[i].lock(); assert(t); /* FIXME: Handle ghost */
						t->ReadU(&w);
					} catch (NetBlockExc &e) {
						/* Nothing */
					} catch (NetDisconnectExc &e) {
						StagedDisconnectEntry mgde = {i, m_sockm[i], true};
						retd->push_back(mgde);
					} catch (NetFailureExc &e) {
						StagedDisconnectEntry mgde = {i, m_sockm[i], false};
						retd->push_back(mgde);
					}

					/* Might have read something even if a disconnect or failure occurred */
					if (!w.empty()) {
						StagedReadEntry mgre = {i, m_sockm[i], w};
						retr->push_back(mgre);
					}
				}
			}

			return ret;
		}

		static void StagedReadApply(const StagedRead_t& srt) {
			for (auto &i : srt) {
				LOG(INFO) << "Processing Staged Read: " << accumulate(i.rd.begin(), i.rd.end(), string(), [](string &x, const Fragment &y){return x+=y.data;});
				shared_ptr<ManagedSock> w = i.ms.lock(); assert(w); /* FIXME: Handle ghost (Applicable?) */
				w->in.insert(w->in.end(), i.rd.begin(), i.rd.end());
			}
		}

		static void StagedDisconnectApply(const StagedDisconnect_t& sdt) {
			for (auto &i : sdt) {
				LOG(INFO) << "Processing Staged Disconnect. Graceful: " << bool(i.gracefulp);
				shared_ptr<ManagedSock> w = i.ms.lock(); assert(w); /* FIXME: Handle ghost */
				w->SetKnownClosed();
			}
		}

		void RemoveKnownClosed() {
			vector<weak_ptr<PrimitiveSock> > newp;
			vector<weak_ptr<ManagedSock> > newm;
			vector<pollfd> newl;

			for (size_t i = 0; i < mSockCnt; i++) {
				if (m_sockm[i].lock()->IsKnownClosed()) /* FIXME: Handle ghost */
					continue;
				newp.push_back(m_sockp[i]);
				newm.push_back(m_sockm[i]);
				newl.push_back(m_poller.pfd[i]);
			}

			swap(m_sockp, newp);
			swap(m_sockm, newm);
			swap(m_poller.pfd, newl);

			mSockCnt = m_poller.Size();
		};

		static shared_ptr<ManagedSock> MakeMgdSkt(shared_ptr<ManagedGroup> mg, shared_ptr<PrimitiveSock> ps) {
			return mg->RegSock(ps);
		}
	};

	class PrimitiveListening {
	public:
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
			vector<shared_ptr<PrimitiveSock>> ret;
			vector<SOCKET> w;

			try {
				for (;;) {
					SOCKET s = accept(sock, nullptr, nullptr);
					if (s != INVALID_SOCKET) w.push_back(SOCKET(s));
					if (s == INVALID_SOCKET)
						if (ErrorWouldBlock()) throw NetBlockExc();
						else                   throw NetFailureExc();
				}
			} catch (NetBlockExc &e) {}

			transform(w.begin(), w.end(), back_inserter(ret), [](SOCKET &p) { return make_shared<PrimitiveSock>(PollFdType::Make(p)); });

			return ret;
		}
	};

};

namespace S3 {

	using namespace S2;

	enum class PipeType {
		Packet
	};

	class Pipe {
	public:
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
	//
	// class Abc { Base x; }; vs class Abc { Base getX(); };

	class PipeMaker {
	public:
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
	namespace S = S2;

	FLAGS_logtostderr = 1;
	google::InitGoogleLogging("WinSock1");

	LOG(INFO) << "Hello";

	WinsockWrap ww;

	{
		auto pl = make_shared<S::PrimitiveListening>();
		auto mg = make_shared<S::ManagedGroup>();

		vector<shared_ptr<S::PrimitiveSock> > tmpPsv;
		vector<shared_ptr<S::ManagedSock> > tmpMsv;

		for (;;) {
			vector<shared_ptr<S::PrimitiveSock> > svec = pl->Accept();
			for (auto &i : svec) { tmpPsv.push_back(i); tmpMsv.push_back(S::ManagedGroup::MakeMgdSkt(mg, i)); }

			auto w = mg->StagedRead();
			S::ManagedGroup::StagedReadApply(*w.r);
			S::ManagedGroup::StagedDisconnectApply(*w.d);
			mg->RemoveKnownClosed();
		}
	}

	return EXIT_SUCCESS;
}
