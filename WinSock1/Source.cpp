#include <cassert>
#include <cstdio>

#include <exception>
#include <string>
#include <memory>
#include <vector>
#include <deque>
#include <set>
#include <map>
#include <algorithm>
#include <iterator>
#include <numeric> /* accumulate */
#include <sstream>

#include <loginc.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma warning(once : 4101 4800)

#define MEHTHROW(s) (::std::runtime_error((s ## " " ## __FILE__ ## " ") + ::std::to_string(__LINE__)))

#define MAGIC_READ_SIZE 1024
#define PACKET_PART_SIZE_LEN 4

using namespace std;

string Uint32ToString(uint32_t x) { std::stringstream ss; ss << x; std::string str; ss >> str; return str; }

/* template<typename T> void PtrCond(T *p, T v) { if (p) *p = v; } */
#define PTR_COND(p,v) do { auto _f_ = (p); if (_f_) { *_f_ = (v); } } while(0)
#define ZZMAX(a,b) (((a) > (b)) ? (a) : (b))
#define ZZMIN(a,b) (((a) < (b)) ? (a) : (b))

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

	class NetExc : ::std::exception {};
	class NetFailureExc : NetExc {};
	class NetDisconnectExc : NetFailureExc {};
	class NetBlockExc : NetExc {};
	class NetFailureErrExc : NetFailureExc {
	public:
		int e;
		mutable string s;
		NetFailureErrExc() { e = WSAGetLastError(); }
		virtual const char * what() const {
			switch (e) { case WSAEINVAL: return "WSAEINVAL"; case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK"; case WSAEINTR: return "WSAEINTR"; case WSAEINPROGRESS: return "WSAEINPROGRESS"; default: return (s = string("WS32ERR ").append(Uint32ToString((uint32_t)e))).c_str(); };
		};
	};

	bool ErrorWouldBlock() {
		int e = WSAGetLastError();
		return e == WSAEWOULDBLOCK || e == WSAEINTR || e == WSAEINPROGRESS;
	}

	Stamp  EmptyStamp() {
		return 0xBBAACCFF;
	}

	struct PackCont {
		size_t fragNo; size_t partNo;
		PackCont() : fragNo(0), partNo(0) {}
		PackCont(size_t fragNo, size_t partNo) : fragNo(fragNo), partNo(partNo) {}
	};

	struct PackContR {
		size_t fragNo; size_t partNo; bool inIn;
		PackContR() : fragNo(0), partNo(0), inIn(true) {}
		PackContR(size_t fragNo, size_t partNo, bool inIn) : fragNo(fragNo), partNo(partNo), inIn(inIn) {}
	};

	class Fragment {
	public:
		Stamp stamp;
		string data;
		Fragment(const Stamp &s, const string &d) : stamp(s), data(d) {}
	};

	string SplitFragPrefix(const Fragment &frag, size_t partNo) {
		return frag.data.substr(0, partNo);
	}

	string SplitFragSuffix(const Fragment &frag, size_t partNo) {
		return frag.data.substr(partNo, string::npos);
	}

	void ErasePrefixTo(deque<Fragment> *deq, const PackCont &pc) {
		/* Erase [0, fragNo) */
		deq->erase(deq->begin(), deq->begin() + pc.fragNo);
		/* Split a partial */
		if (pc.partNo != 0) {
			Fragment f(deq->at(pc.fragNo).stamp, move(SplitFragSuffix(deq->at(pc.fragNo), pc.partNo)));
			deq->pop_front();
			deq->push_front(move(f)); /* FIXME: Redundant if f.data is empty? */
		}
	}

	void CopySuffixFrom(const deque<Fragment> &deq, const PackCont &pc, deque<Fragment> *out) {
		if (deq.size() == pc.fragNo) return;

		/* Copy first frag with correct splitting */
		out->push_back(Fragment(deq.at(pc.fragNo).stamp, move(SplitFragSuffix(deq.at(pc.fragNo), pc.partNo))));

		/* Copy remaining fully */
		copy(deq.begin() + (pc.fragNo + 1), deq.end(), back_inserter(*out));
	}

	class PollFdType {
	public:
		SOCKET socketfd;
		static PollFdType Make(SOCKET s) { PollFdType w; w.socketfd = s; return w; }
		static void ExtractInto(const PollFdType& fd, pollfd *pfd) { pfd->fd = fd.socketfd; }
		static bool IsDummy(SOCKET s) { return s == POLL_VAL_DUMMY; }
		static void Read(const PollFdType &pfd, deque<Fragment>* w) {
			char buf[MAGIC_READ_SIZE];

			for (;;) {
				int r = recv(pfd.socketfd, buf, MAGIC_READ_SIZE, 0);
				if (r == 0)                throw NetDisconnectExc();
				if (r == SOCKET_ERROR)
					if (ErrorWouldBlock()) throw NetBlockExc();
					else                   throw NetFailureExc();

					w->push_back(Fragment(EmptyStamp(), string(buf, r)));

					LOG(INFO) << "Raw Read " << string(buf, r);
			}
		}
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

		vector<PollFdType> Accept2() const
		{
			vector<PollFdType> ret;

			try {
				for (;;) {
					SOCKET s = accept(sock, nullptr, nullptr);
					if (s != INVALID_SOCKET) ret.push_back(PollFdType::Make(SOCKET(s)));
					if (s == INVALID_SOCKET)
						if (ErrorWouldBlock()) throw NetBlockExc();
						else                   throw NetFailureExc();
				}
			} catch (NetBlockExc &e) {}

			return ret;
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

namespace Messy {
	using namespace S2;

	const uint32_t gMaxTokens = 100;

	class ConToken {
	public:
		uint32_t id;
		ConToken(uint32_t id) : id(id) {}
	};

	struct ConTokenLess : std::binary_function<ConToken, ConToken, bool> {
		bool operator() (const ConToken &lhs, const ConToken &rhs) const {
			return lhs.id < rhs.id;
		}
	};

	class ConTokenGen {
		set<ConToken, ConTokenLess> toks;
	public:
		ConTokenGen() { for (int i = 0; i < gMaxTokens; i++) toks.insert(ConToken(i)); }

		ConToken GetToken() {
			if (!toks.size()) throw exception("Out of tokens");
			ConToken w = *toks.begin();
			toks.erase(toks.begin());
			return w;
		}

		void ReturnToken(ConToken tok) {
			toks.insert(tok);
		}
	};

	class MessSock {
		struct CtData {
			PollFdType pfd;
			deque<Fragment> in;
			deque<Fragment> out;
			bool knownClosed;
			CtData(PollFdType pfd) : pfd(pfd), in(), out(), knownClosed(false) {}
		};

		ConTokenGen tokenGen;
		map<ConToken, CtData, ConTokenLess> cons;
		mutable vector<pollfd> pfds;

		uint32_t numCons;

		void RebuildPoll() const {
			pfds.resize(cons.size());
			size_t idx = 0;
			for (auto &i : cons) PollFdType::ExtractInto(i.second.pfd, &pfds.data()[idx++]);
		}

		void UpdateCons() {
			numCons = cons.size();
			RebuildPoll();
		};

		void ReadyForPoll() const {
			for (size_t i = 0; i < pfds.size(); i++) {
				pfds[i].events = POLLIN | POLLOUT;
				pfds[i].revents = 0;
			}
		};

		void AddConsMulti(const vector<PollFdType>& pfds, const vector<ConToken> toks, const vector<CtData> cts) {
			size_t i;

			try {
				for (i = 0; i < pfds.size(); i++)
					if (!cons.insert(make_pair(toks[i], cts[i])).second) throw exception();
			} catch (exception &e) {
				LOG(INFO) << "Insertion failure in AddConsMulti (Attempt to insert existing ConToken?)";

				for (size_t j = 0; j < i; i++)
					if (cons.erase(toks[i]) != 1) throw exception("Abort");

				throw;
			}
		}

	public:

		MessSock() : numCons(0) {}

		void AcceptedConsMulti(const vector<PollFdType>& pfds) {
			if (!pfds.size()) return;

			vector<ConToken> newToks;
			vector<CtData> newCts;
			try {
				for (auto &i : pfds) newToks.push_back(tokenGen.GetToken());
				for (auto &i : pfds) newCts.push_back(i);
				AddConsMulti(pfds, newToks, newCts);
			} catch (exception &e) {
				for (auto &i : newToks) tokenGen.ReturnToken(i);
				throw;
			}

			UpdateCons();
		}

		vector<ConToken> GetConTokens() const {
			vector<ConToken> ret;
			for (auto &i : cons) ret.push_back(i.first);
			return ret;
		}

		typedef struct { ConToken tok; deque<Fragment> in; } StagedRead_t;
		typedef struct { ConToken tok; bool graceful; } StagedDisc_t;

		struct Staged_t {
			shared_ptr<vector<StagedRead_t> > r;
			shared_ptr<vector<StagedDisc_t> > d;
			Staged_t() : r(make_shared<vector<StagedRead_t> >()), d(make_shared<vector<StagedDisc_t> >()) {}
		};

		Staged_t StagedRead() const {
			/**
			* FIXME: If disconnects are not processed, 'numCons' will not match real.
			*        I suspect a WSAPoll with all sockets dead is invalid.
			*        (See conditions for a WSAEINVAL return on MSDN)
			*/
			Staged_t ret;

			if (!numCons) return ret;

			RebuildPoll(); /* FIXME: Redundant RebuildPoll just to be sure. Call RebuildPoll only on changes to cons.. */
			ReadyForPoll();

			int r = WSAPoll(pfds.data(), pfds.size(), 0);
			if (r == SOCKET_ERROR) 
				throw NetFailureErrExc();
			if (r > 0) {
				size_t idx = 0;
				for (auto it = cons.begin(); it != cons.end(); it++) {
					if (!(pfds[idx++].revents & (POLLIN | POLLOUT))) continue;

					deque<Fragment> w;

					try {
						PollFdType::Read(it->second.pfd, &w);
					} catch (NetBlockExc &e) {
						/* Nothing */
					} catch (NetDisconnectExc &e) {
						StagedDisc_t mgde = { it->first, true };
						ret.d->push_back(mgde);
					} catch (NetFailureExc &e) {
						StagedDisc_t mgde = { it->first, false };
						ret.d->push_back(mgde);
					}

					/* Might have read something even if a disconnect or failure occurred */
					if (!w.empty()) {
						StagedRead_t mgre = { it->first, w };
						ret.r->push_back(mgre);
					}
				}
			}

			return ret;
		}

	};

};

namespace S3 {
	using namespace S2;

	struct PackContIt : public ::std::iterator<::std::input_iterator_tag, Fragment> {
		/* Should be CopyConstructible, Assignable */
		const deque<Fragment> *fst, *snd;
		PackContR cont;
		mutable int canary, canary_limit; /* FIXME: Cheese */
		/* NOTE: 'cont(0, 0, bool(fst.size()))' skips an empty 'fst' */
		PackContIt(const deque<Fragment> &fst, const deque<Fragment> &snd) : fst(&fst), snd(&snd), cont(0, 0, bool(fst.size())), canary(0), canary_limit(1000) {}
		void AdvancePart() {
			const deque<Fragment> &curr = cont.inIn ? *fst : *snd;
			if ((++cont.partNo) >= CurFragData().size())
				AdvanceFrag();
		}
		void AdvanceToPart(size_t w) {
			const deque<Fragment> &curr = cont.inIn ? *fst : *snd;
			cont.partNo = ZZMIN(w, cont.fragNo >= curr.size() ? 0 : curr[cont.fragNo].data.size());
		}
		void AdvanceFrag() {
			const deque<Fragment> &curr = cont.inIn ? *fst : *snd;
			if (cont.fragNo > curr.size()) assert(0);

			if (cont.fragNo == curr.size() && cont.inIn) cont = PackContR(0, 0, false);
			else                                         cont.fragNo++;

			AdvanceToPart(0);
		}
		const string & CurFragData() const {
			const deque<Fragment> &curr = cont.inIn ? *fst : *snd;
			return curr.at(cont.fragNo).data;
		}
		size_t CurPart() const { return cont.partNo; }
		bool EndFragP() const {
			if (canary++ > canary_limit) assert(0);
			const deque<Fragment> &curr = cont.inIn ? *fst : *snd;
			return !cont.inIn && cont.fragNo == curr.size();
		}
		bool SameFragP(const PackContIt &rhs) const {
			if (canary++ > canary_limit) assert(0);
			return cont.inIn == rhs.cont.inIn && cont.fragNo == rhs.cont.fragNo;
		}
	};

	namespace PackLenDel {
		static void GetN(const deque<Fragment> &que, string *accum, size_t *remaining) {
			for (size_t i = 0; i < que.size() && *remaining > 0; i++) {
				size_t have = que[i].data.length();
				size_t getting = have < *remaining ? have : *remaining;
				accum->append(que[i].data.substr(0, getting));
				*remaining -= getting;
			}
		}

		static void GetNTwin(const deque<Fragment> &in, const deque<Fragment> &extra, string *accum, size_t *remaining) {
			GetN(in, accum, remaining);
			if (remaining) GetN(extra, accum, remaining);
		}

		static bool GetSize(const deque<Fragment> &in, const deque<Fragment> &extra, uint32_t *out) {
			union { char c[PACKET_PART_SIZE_LEN]; uint32_t u; } d;

			size_t remaining = sizeof d.c;
			string data;

			GetNTwin(in, extra, &data, &remaining);

			if (remaining) {
				PTR_COND(out, (uint32_t)0);
				return false;
			} else {
				memcpy(d.c, data.data(), sizeof d.c);
				PTR_COND(out, (uint32_t)ntohl(d.u));
				return true;
			}
		}

		static bool GetPacket(const deque<Fragment> &in, const deque<Fragment> &extra, string *out) {
			uint32_t sz;
			string data;

			if (!GetSize(in, extra, &sz)) {
				PTR_COND(out, string());
				return false;
			} else {
				uint32_t szPlusHdr = sz + PACKET_PART_SIZE_LEN;
				GetNTwin(in, extra, &data, &szPlusHdr);
				if (&szPlusHdr) {
					PTR_COND(out, string());
					return false;
				} else {
					PTR_COND(out, data.substr(PACKET_PART_SIZE_LEN, string::npos));
					return true;
				}
			}
		}
	};

	namespace PackNlDelEx {

		static bool ReadyPacketPos(PackContIt *fpos) {
			/* Update iterator only on success */
			PackContIt pos(*fpos);

			for (; !pos.EndFragP(); pos.AdvanceFrag()) {
				size_t posn = pos.CurFragData().find('\n', pos.CurPart());
				/* size_t posr = pos.CurFragData().find('\r', pos.CurPart()); */
				/* size_t msgEndPos = posn > 0 && posn-1 == posr ? posr : posn; */

				if (posn != string::npos) {
					pos.AdvanceToPart(posn);
					pos.AdvancePart();

					*fpos = pos;
					return true;
				}
			}

			return false;
		}

		static void GetFromTo(const PackContIt &from, const PackContIt &to, string *accum) {
			PackContIt it = from;

			/* it.CurPart() is from.CurPart() the first time, and 0 (From it.AdvanceFrag()) afterwards */
			for (; !it.SameFragP(to); it.AdvanceFrag())
					accum->append(it.CurFragData().substr(it.CurPart(), string::npos));

			if (!it.EndFragP())
				accum->append(it.CurFragData().substr(0, to.CurPart()));
			}
		}

		static bool GetPacket(PackContIt *pos, string *out) {
			PackContIt start = *pos;

			if (!PackNlDelEx::ReadyPacketPos(pos))
				return false;

			PackNlDelEx::GetFromTo(start, *pos, out);
			return true;
	};

	namespace PackNlDel {
		static bool AuxFromQueue(const deque<Fragment> &in, string *accum, PackCont *cont) {
			for (size_t i = cont->fragNo; i < in.size(); i++) {
				const string &dat = in[i].data;

				if (!dat.find('\n', cont->partNo)) {
					accum->append(dat.substr(cont->partNo, string::npos));
					cont->fragNo++;
					continue;
				} else {
					size_t pos = dat.find('\n', cont->partNo);
					size_t msgEndPos = (pos > cont->partNo && dat[pos - 1] == '\r') ? pos-- : pos;
					accum->append(dat.substr(cont->partNo, msgEndPos));
					cont->partNo = pos + 1; /* FIXME: Probably want the +1 (Leave pointing to first after match, or OOB(Onepast)) */
					return true;
				}
			}

			return false;
		}

		static bool GetPacket(const deque<Fragment> &in, const deque<Fragment> &extra, const PackCont &cont, string *out, PackContR *contOut) {
			bool ok1 = false, ok2 = false;
			string data;

			PackCont c;

			{
				c = cont;
				ok1 = AuxFromQueue(in, &data, &c);
			}

			if (!ok1) {
				c = PackCont(0, 0);
				ok2 = AuxFromQueue(extra, &data, &c);
			}

			if (ok1 || ok2) {
				/* A query succeeded. 'inIn' if first succeeded. */
				PTR_COND(out, move(data));
				PTR_COND(contOut, PackContR(c.fragNo, c.partNo, ok1));
				return true;
			} else {
				/* Both queries failed. Currently in extra (inIn = false). */
				PTR_COND(out, string());
				PTR_COND(contOut, PackContR(c.fragNo, c.partNo, false));
				return false;
			}
		}
	};
};

namespace S3 {

	using namespace S2;
	//using namespace PackLenDel;
	//using namespace PackNlDel;
	using namespace PackNlDelEx;

	enum class PipeType {
		Packet
	};

	struct PostProcess {
		virtual void Process() const { LOG(ERROR) << "Empty PostProcess step"; throw exception(); };
		void operator () (void) const { return Process(); };
	};

	class PipeI;
	class PipeR;

	class PipeI {
	public:
		virtual PipeR * RemakeForRead(vector<shared_ptr<PostProcess> > *pp, const Messy::MessSock::StagedRead_t &sr) = 0;
	};

	class PipeR : public PipeI {
	public:
		PipeType pt;
	};

	class Pipe {
	public:
		shared_ptr<PipeR> pr;
	};

	struct PostProcessFragmentWrite : PostProcess {
		shared_ptr<deque<Fragment> > deq;
		const Messy::MessSock::StagedRead_t &sr;
		PostProcessFragmentWrite(shared_ptr<deque<Fragment> > deq, const Messy::MessSock::StagedRead_t &sr) : deq(deq), sr(sr) {}
		virtual void Process() const {
			for (auto &i : sr.in) deq->push_back(i);
		}
	};

	struct PostProcessCullPrefixAndMerge : PostProcess {
		const PackContR cont;
		shared_ptr<deque<Fragment> > in;
		const deque<Fragment> &extra;
		PostProcessCullPrefixAndMerge(shared_ptr<deque<Fragment> > in, const deque<Fragment> &extra, const PackContR cont) : in(in), extra(extra), cont(cont) {}
		virtual void Process() const {
			if (cont.inIn) {
				ErasePrefixTo(&(*in), PackCont(cont.fragNo, cont.partNo));
				CopySuffixFrom(extra, PackCont(0, 0), &(*in));
			} else {
				in->clear();
				CopySuffixFrom(extra, PackCont(cont.fragNo, cont.partNo), &(*in));
			}
		}
	};

	struct PostProcessPackWrite : PostProcess {
		shared_ptr<deque<string> > dest;
		shared_ptr<deque<string> > src;
		PostProcessPackWrite(shared_ptr<deque<string> > dest, shared_ptr<deque<string> > src) : dest(dest), src(src) {}
		virtual void Process() const { for (auto &i : *src) dest->push_back(i); }
	};

	class PipePacket : public PipeR {
	public:
		shared_ptr<deque<Fragment> > in;
		shared_ptr<deque<Fragment> > out;

		shared_ptr<deque<string> > inPack;

		PipePacket() :
			in(make_shared<deque<Fragment> >()),
			out(make_shared<deque<Fragment> >()),
			inPack(make_shared<deque<string> >()) {}

		virtual PipePacket * RemakeForRead(vector<shared_ptr<PostProcess> > *pp, const Messy::MessSock::StagedRead_t &sr) {
			shared_ptr<deque<string> > inP = make_shared<deque<string> >();

			/* Check for completed packets, leave iterator past last completed packet */
			PackContIt cont(*in, sr.in);

			string data;
			while (GetPacket(&cont, &data)) {
				inP->push_back(move(data));
				data = string();
			}

			const PackContR finalCont = cont.cont;

			/* FIXME: CopyConstructed */
			PipePacket *ret = new PipePacket(*this);

			pp->push_back(make_shared<PostProcessCullPrefixAndMerge>(ret->in, sr.in, finalCont));
			pp->push_back(make_shared<PostProcessPackWrite>(ret->inPack, inP));

			return ret;
		}
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
		static shared_ptr<Pipe> MakePacket() {
			auto p = make_shared<Pipe>();
			auto r = PipeMaker::MakePacketR();
			p->pr = r;
			return p;
		}

		static shared_ptr<PipePacket> MakePacketR() {
			shared_ptr<PipePacket> pl = make_shared<PipePacket>();
			pl->pt = PipeType::Packet;
			return pl;
		}
	};

};

int main2() {
	namespace S = S2;
	namespace M = Messy;

	WinsockWrap ww;

	{
		const auto pl = make_shared<S::PrimitiveListening>();
		auto m = make_shared<M::MessSock>();

		map<M::ConToken, shared_ptr<S3::Pipe>, M::ConTokenLess> pipes;

		for (;;) {
			static shared_ptr<S3::Pipe> pp = nullptr;

			vector<S::PollFdType> svec = pl->Accept2();
			m->AcceptedConsMulti(svec);

			{
				set<M::ConToken, M::ConTokenLess> ptoks, mtoks;
				for (auto &i : pipes) ptoks.insert(i.first);
				for (auto &i : m->GetConTokens()) mtoks.insert(i);

				vector<M::ConToken> toCreate;
				set_difference(mtoks.begin(), mtoks.end(), ptoks.begin(), ptoks.end(), back_inserter(toCreate), M::ConTokenLess());

				for (auto &i : toCreate) assert(pipes.find(i) == pipes.end());
				for (auto &i : toCreate) pipes[i] = S3::PipeMaker::MakePacket();
				for (auto &i : toCreate) LOG(INFO) << "Creating " << i.id << " " << m->GetConTokens().size();
			}

			const auto sg = m->StagedRead();

			{
				vector<shared_ptr<S3::PostProcess> > pc;

				for (auto &i : *sg.r) {
					if (pipes.find(i.tok) == pipes.end()) { LOG(ERROR) << "Read of inexistant " << i.tok.id; continue; }
					pipes[i.tok]->pr->RemakeForRead(&pc, i);
				}

				for (auto &i : pc) i->Process();

				for (auto &i : pipes) {
					S3::PipePacket *p3p = dynamic_cast<S3::PipePacket *>(&*(i.second->pr));
					if (!p3p->inPack->size()) continue;
					LOG(INFO) << "PackRead " << i.first.id << " : " << p3p->inPack->size();
					for (auto &i : *p3p->inPack) LOG(INFO) << "  " << i.c_str();
				}
			}
		}
	}

	return EXIT_SUCCESS;

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

int main() {
	FLAGS_logtostderr = 1;
	google::InitGoogleLogging("WinSock1");

	LOG(INFO) << "Hello";

	try {
		return main2();
	} catch (S2::NetFailureErrExc &e) {
		LOG(INFO) << "Exception " << e.what();
		throw;
	}
}
