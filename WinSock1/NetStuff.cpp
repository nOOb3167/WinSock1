#include <cassert>
#include <cstdint>

#include <vector>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <memory>

#include <loginc.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <NetStuff.h>

namespace NetData {
	Stamp EmptyStamp() {
		return 0xBBAACCFF;
	};

	Fragment::Fragment(const Stamp &stamp, const string &data) : stamp(stamp), data(data) {}

	string Fragment::SplitFragPrefix(size_t partNo) const {
		return data.substr(0, partNo);
	}

	string Fragment::SplitFragSuffix(size_t partNo) const {
		return data.substr(partNo, string::npos);
	}

	void Fragment::ErasePrefixTo(deque<Fragment> *deq, const PackCont &pc) {
		/* Erase [0, fragNo) */
		deq->erase(deq->begin(), deq->begin() + pc.fragNo);
		/* Split a partial */
		if (pc.partNo != 0) {
			Fragment f(deq->at(pc.fragNo).stamp, move(deq->at(pc.fragNo).SplitFragSuffix(pc.partNo)));
			deq->pop_front();
			deq->push_front(move(f)); /* FIXME: Redundant if f.data is empty? */
		}
	}
	void Fragment::CopySuffixFrom(const deque<Fragment> &deq, const PackCont &pc, deque<Fragment> *out) {
		if (deq.size() == pc.fragNo) return;

		/* Copy first frag with correct splitting */
		out->push_back(Fragment(deq.at(pc.fragNo).stamp, move(deq.at(pc.fragNo).SplitFragSuffix(pc.partNo))));

		/* Copy remaining fully */
		copy(deq.begin() + (pc.fragNo + 1), deq.end(), back_inserter(*out));
	}

	PackCont::PackCont() : fragNo(0), partNo(0) {}

	PackCont::PackCont(size_t fragNo, size_t partNo) : fragNo(fragNo), partNo(partNo) {}

	PackContR::PackContR() : fragNo(0), partNo(0), inIn(true) {}

	PackContR::PackContR(size_t fragNo, size_t partNo, bool inIn) : fragNo(fragNo), partNo(partNo), inIn(inIn) {}
};

namespace NetNative {
	NetFailureErrExc::NetFailureErrExc() { e = WSAGetLastError(); }
	const char * NetFailureErrExc::what() const {
		switch (e) { case WSAEINVAL: return "WSAEINVAL"; case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK"; case WSAEINTR: return "WSAEINTR"; case WSAEINPROGRESS: return "WSAEINPROGRESS"; default: return (s = string("WS32ERR ").append(Uint32ToString((uint32_t)e))).c_str(); };
	};

	PrimitiveListening::PrimitiveListening() {
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

			pfd = GNetNat.MakePollFdType(listen_sock);

		} catch (exception &) {
			if (res) freeaddrinfo(res);
			if (listen_sock != INVALID_SOCKET) closesocket(listen_sock);
			throw;
		}
	}

	PrimitiveListening::~PrimitiveListening()
	{
		closesocket(pfd.s);
	}

	vector<PollFdType> PrimitiveListening::Accept() const
	{
		vector<PollFdType> ret;

		try {
			for (;;) {
				SOCKET s = accept(pfd.s, nullptr, nullptr);
				if (s != INVALID_SOCKET) ret.push_back(GNetNat.MakePollFdType(SOCKET(s)));
				if (s == INVALID_SOCKET)
					if (GNetNat.ErrorWouldBlock()) throw NetData::NetBlockExc();
					else						   throw NetData::NetFailureExc();
			}
		} catch (NetData::NetBlockExc &e) {}

		return ret;
	}

	bool NetFuncs::ErrorWouldBlock() {
		int e = WSAGetLastError();
		return e == WSAEWOULDBLOCK || e == WSAEINTR || e == WSAEINPROGRESS;
	}

	void NetFuncs::PollFdTypeRead(const PollFdType &pfd, deque<NetData::Fragment>* w) {
		char buf[MAGIC_READ_SIZE];

		for (;;) {
			int r = recv(pfd.s, buf, MAGIC_READ_SIZE, 0);
			if (r == 0)                throw NetData::NetDisconnectExc();
			if (r == SOCKET_ERROR)
				if (ErrorWouldBlock()) throw NetData::NetBlockExc();
				else                   throw NetFailureErrExc();

				/* FIXME: EmptyStamp */
				w->push_back(NetData::Fragment(NetData::EmptyStamp(), string(buf, r)));
		}
	};

	PollFdType NetFuncs::MakePollFdType(SOCKET s) {
		return PollFdType(s);
	}

	void MessSockSlave::ReadyForPoll() {
		for (size_t i = 0; i < pfds.size(); i++) {
			pfds[i].events = POLLIN | POLLOUT;
			pfds[i].revents = 0;
		}
	};

	vector<bool> MessSockSlave::PerformPoll() {
		vector<bool> ret;

		int r = WSAPoll(pfds.data(), pfds.size(), 0);
		if (r == SOCKET_ERROR) 
			throw NetFailureErrExc();

		for (size_t i = 0; i < pfds.size(); i++) {
			if (pfds[i].revents & (POLLIN | POLLOUT)) ret.push_back(1);
			else ret.push_back(0);
		}

		return ret;
	}

	void MessSockSlave::RebuildPollFrom(const vector<PollFdType> &p) {
		pfds.resize(p.size());

		for (size_t i = 0; i < p.size(); i++) {
			/* FIXME: Some kind of PollFdType::ExtractInto */
			pfds[i].fd = p[i].s;
		}

		ReadyForPoll();
	}

};

namespace NetStuff {

	ConToken::ConToken(uint32_t id) : id(id) {}

	bool ConTokenLess::operator() (const ConToken &lhs, const ConToken &rhs) const {
		return lhs.id < rhs.id;
	}

	ConTokenGen::ConTokenGen() : maxTokens(100) { for (int i = 0; i < maxTokens; i++) toks.insert(ConToken(i)); }

	ConToken ConTokenGen::GetToken() {
		if (!toks.size()) throw exception("Out of tokens");
		ConToken w = *toks.begin();
		toks.erase(toks.begin());
		return w;
	}

	void ConTokenGen::ReturnToken(ConToken tok) {
		toks.insert(tok);
	}

	/* NOTE: 'cont(0, 0, bool(fst.size()))' skips an empty 'fst' */
	PackContIt::PackContIt(const deque<Fragment> &fst, const deque<Fragment> &snd) : fst(&fst), snd(&snd), cont(0, 0, bool(fst.size())), canary(0), canary_limit(1000) {}

	void PackContIt:: AdvancePart() {
		const deque<Fragment> &curr = cont.inIn ? *fst : *snd;
		if ((++cont.partNo) >= CurFragData().size())
			AdvanceFrag();
	}

	void PackContIt::AdvanceToPart(size_t w) {
		const deque<Fragment> &curr = cont.inIn ? *fst : *snd;
		cont.partNo = ZZMIN(w, cont.fragNo >= curr.size() ? 0 : curr[cont.fragNo].data.size());
	}

	void PackContIt::AdvanceFrag() {
		const deque<Fragment> &curr = cont.inIn ? *fst : *snd;
		if (cont.fragNo > curr.size()) assert(0);

		if (cont.fragNo == curr.size() && cont.inIn) cont = PackContR(0, 0, false);
		else                                         cont.fragNo++;

		AdvanceToPart(0);
	}

	const string & PackContIt::CurFragData() const {
		const deque<Fragment> &curr = cont.inIn ? *fst : *snd;
		return curr.at(cont.fragNo).data;
	}

	size_t PackContIt::CurPart() const { return cont.partNo; }

	bool PackContIt::EndFragP() const {
		if (canary++ > canary_limit) assert(0);
		const deque<Fragment> &curr = cont.inIn ? *fst : *snd;
		return !cont.inIn && cont.fragNo == curr.size();
	}

	bool PackContIt::SameFragP(const PackContIt &rhs) const {
		if (canary++ > canary_limit) assert(0);
		return cont.inIn == rhs.cont.inIn && cont.fragNo == rhs.cont.fragNo;
	}

	void PackContIt::GetFromTo(const PackContIt &from, const PackContIt &to, string *accum) {
		PackContIt it = from;

		/* it.CurPart() is from.CurPart() the first time, and 0 (From it.AdvanceFrag()) afterwards */
		for (; !it.SameFragP(to); it.AdvanceFrag())
			accum->append(it.CurFragData().substr(it.CurPart(), string::npos));

		if (!it.EndFragP())
			accum->append(it.CurFragData().substr(0, to.CurPart()));
	}

	vector<PollFdType> MessSock::GetPollFds() {
		vector<PollFdType> curCons;
		for (auto &i : cons) curCons.push_back(i.second.pfd);

		return curCons;
	}

	void MessSock::UpdateCons() {
		numCons = cons.size();
		aux.RebuildPollFrom(GetPollFds());
	};

	void MessSock::AddConsMulti(const vector<PollFdType>& pfds, const vector<ConToken> toks, const vector<CtData> cts) {
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

	MessSock::MessSock() : numCons(0) {}

	void MessSock::AcceptedConsMulti(const vector<PollFdType>& pfds) {
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

	vector<ConToken> MessSock::GetConTokens() const {
		vector<ConToken> ret;
		for (auto &i : cons) ret.push_back(i.first);
		return ret;
	}

	MessSock::Staged_t MessSock::StagedRead() {
		Staged_t ret;

		if (!numCons) return ret;

		aux.RebuildPollFrom(GetPollFds());

		vector<bool> pollres = aux.PerformPoll();

		size_t idx = 0;

		for (auto &it : cons) {
			if (!pollres[idx++]) continue;

			deque<Fragment> w;

			try {
				GNetNat.PollFdTypeRead(it.second.pfd, &w);
			} catch (NetBlockExc &e) {
				/* Nothing */
			} catch (NetDisconnectExc &e) {
				StagedDisc_t mgde = { it.first, true };
				ret.d->push_back(mgde);
			} catch (NetFailureExc &e) {
				StagedDisc_t mgde = { it.first, false };
				ret.d->push_back(mgde);
			}

			/* Might have read something even if a disconnect or failure occurred */
			if (!w.empty()) {
				StagedRead_t mgre = { it.first, w };
				ret.r->push_back(mgre);
			}
		}

		return ret;
	};

	void PostProcess::Process() {
		LOG(ERROR) << "Empty PostProcess step";
		throw exception("Empty PostProcess step");
	};

	PostProcessFragmentWrite::PostProcessFragmentWrite(shared_ptr<deque<Fragment> > deq, const MessSock::StagedRead_t &sr) : deq(deq), sr(&sr) {}

	void PostProcessFragmentWrite::Process() const {
		for (auto &i : sr->in) deq->push_back(i);
	}

	PostProcessCullPrefixAndMerge::PostProcessCullPrefixAndMerge(shared_ptr<deque<Fragment> > in, const deque<Fragment> &extra, const PackContR cont) : in(in), extra(extra), cont(cont) {}
	void PostProcessCullPrefixAndMerge::Process() const {
		if (cont.inIn) {
			NetData::Fragment::ErasePrefixTo(&(*in), PackCont(cont.fragNo, cont.partNo));
			NetData::Fragment::CopySuffixFrom(*extra, PackCont(0, 0), &(*in));
		} else {
			in->clear();
			NetData::Fragment::CopySuffixFrom(*extra, PackCont(cont.fragNo, cont.partNo), &(*in));
		}
	}

	PostProcessPackWrite::PostProcessPackWrite(shared_ptr<deque<string> > dest, shared_ptr<deque<string> > src) : dest(dest), src(src) {}

	void PostProcessPackWrite::Process() const {
		for (auto &i : *src) dest->push_back(i);
	}

	PipePacket::PipePacket() :
		in(make_shared<deque<Fragment> >()),
		out(make_shared<deque<Fragment> >()),
		inPack(make_shared<deque<string> >()) {}

	PipePacket * PipePacket::RemakeForRead(vector<shared_ptr<PostProcess> > *pp, const MessSock::StagedRead_t &sr) {
		shared_ptr<deque<string> > inP = make_shared<deque<string> >();

		/* Check for completed packets, leave iterator past last completed packet */
		PackContIt cont(*in, sr.in);

		string data;
		while (NetStuff::PackNlDelEx::GetPacket(&cont, &data)) {
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
