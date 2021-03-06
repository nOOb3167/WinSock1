#include <stdafx.h>

#include <cassert>
#include <cstdint>

#include <algorithm>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <memory>

#include <loginc.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <NetStuff.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma warning(once : 4101 4800)

#define MEHTHROW(s) (::std::runtime_error((s ## " " ## __FILE__ ## " ") + ::std::to_string(__LINE__)))

#define MAGIC_READ_SIZE 1024
#define PACKET_PART_SIZE_LEN 4

/* template<typename T> void PtrCond(T *p, T v) { if (p) *p = v; } */
#define PTR_COND(p,v) do { auto _f_ = (p); if (_f_) { *_f_ = (v); } } while(0)
#define ZZMAX(a,b) (((a) > (b)) ? (a) : (b))
#define ZZMIN(a,b) (((a) < (b)) ? (a) : (b))

using namespace std;

WinsockWrap::WinsockWrap() {
    int r = WSAStartup(MAKEWORD(2, 2), &wsd);
    unsigned int l = LOBYTE(wsd.wVersion);
    unsigned int h = HIBYTE(wsd.wVersion);
    if (r) WSACleanup();
    if (l != 2 || h != 2) assert(0);
}

WinsockWrap::~WinsockWrap() {
    WSACleanup();
}

namespace NetNative {
    NetFuncs GNetNat;
};

namespace NetData {
    Stamp EmptyStamp() {
        return 0xBBAACCFF;
    };

    string Uint32ToString(uint32_t x) {
        std::stringstream ss;
        ss << x;
        std::string str;
        ss >> str;
        return str;
    }

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
        switch (e) { case WSAEINVAL: return "WSAEINVAL"; case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK"; case WSAEINTR: return "WSAEINTR"; case WSAEINPROGRESS: return "WSAEINPROGRESS"; default: return (s = string("WS32ERR ").append(NetData::Uint32ToString((uint32_t)e))).c_str(); };
    };

    PollFdType::PollFdType(SOCKET s) : s(s) {}

    PrimitiveListening::PrimitiveListening() : pfd(GNetNat.MakePollFdType(INVALID_SOCKET)) {
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

    PrimitiveMemonly::PrimitiveMemonly(const char **strs) {
        for (size_t i = 0; strs[i] != nullptr; i++)
            read.push_back(Fragment(EmptyStamp(), string(strs[i])));
    }

    vector<PrimitiveMemonly> PrimitiveMemonly::MakePrims(const char **strs) {
        vector<PrimitiveMemonly> ret;
        size_t idx = 0, i;

        while (strs[idx] != nullptr) {
            ret.push_back(PrimitiveMemonly(&strs[idx]));
            for (i = idx; strs[i] != nullptr; i++) {}
            idx = i + 1;
        }

        return ret;
    }

    void PrimitiveMemonly::WriteU(deque<Fragment>* w) {
        assert(0);
        write.insert(write.end(), w->begin(), w->end());
        w->clear();
    }

    void PrimitiveMemonly::ReadU(deque<Fragment>* w) {
        if (read.empty()) throw NetDisconnectExc();
        w->push_back(read.front());
        read.pop_front();
        throw NetBlockExc();
    }

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

    void PackContIt::AdvancePart() {
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

        size_t nFp = ZZMIN(cont.fragNo + 1, curr.size());
        if (cont.inIn && nFp == curr.size()) cont = PackContR(0, 0, false);
        else                                 cont.fragNo = nFp;

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

    MessSock::Staged_t::Staged_t() : r(make_shared<vector<StagedRead_t> >()), d(make_shared<vector<StagedDisc_t> >()) {}

    MessSock::CtData::CtData(PollFdType pfd) : pfd(pfd), in(), out(), knownClosed(false) {}

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
        MessSock::Staged_t ret;

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

    MessMemonly::CtData::CtData(PrimitiveMemonly pmo) : pmo(pmo) {}

    MessMemonly::MessMemonly() : tokenGen(), cons(), numCons(0) {};

    void MessMemonly::AcceptedConsMulti(const vector<PrimitiveMemonly> &mems) {
        for (auto &i : mems)
            if (!cons.insert(make_pair(tokenGen.GetToken(), MessMemonly::CtData(i))).second) assert(0);

        numCons = cons.size();
    }

    vector<ConToken> MessMemonly::GetConTokens() const {
        vector<ConToken> ret;

        for (auto &i : cons)
            ret.push_back(i.first);

        return ret;
    }

    MessSock::Staged_t MessMemonly::StagedRead() {
        auto r = make_shared<vector<MessSock::StagedRead_t> >();
        auto d = make_shared<vector<MessSock::StagedDisc_t> >();
        MessSock::Staged_t ret;
        ret.r = r;
        ret.d = d;

        for (auto &i : cons) {
            deque<Fragment> w;

            try {
                i.second.pmo.ReadU(&w);
            } catch (NetBlockExc &e) {
                /* Nothing */
            } catch (NetDisconnectExc &e) {
                MessSock::StagedDisc_t mgde = { i.first, true };
                ret.d->push_back(mgde);
            } catch (NetFailureExc &e) {
                MessSock::StagedDisc_t mgde = { i.first, false };
                ret.d->push_back(mgde);
            }

            if (!w.empty()) {
                MessSock::StagedRead_t mgre = { i.first, w };
                ret.r->push_back(mgre);
            }
        }

        return ret;
    }

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

        static bool GetPacket(PackContIt *pos, string *out) {
            PackContIt start = *pos;

            if (!PackNlDelEx::ReadyPacketPos(pos))
                return false;

            PackNlDelEx::GetFromTo(start, *pos, out);
            return true;
        }

    };

    void PostProcess::Process() {
        LOG(ERROR) << "Empty PostProcess step";
        throw exception("Empty PostProcess step");
    };

    PostProcessFragmentWrite::PostProcessFragmentWrite(shared_ptr<deque<Fragment> > deq, const MessSock::StagedRead_t &sr) : deq(deq), sr(&sr) {}

    void PostProcessFragmentWrite::Process() {
        for (auto &i : sr->in) deq->push_back(i);
    }

    PostProcessCullPrefixAndMerge::PostProcessCullPrefixAndMerge(shared_ptr<deque<Fragment> > in, const deque<Fragment> &extra, const PackContR cont) : in(in), extra(&extra), cont(cont) {}

    void PostProcessCullPrefixAndMerge::Process() {
        if (cont.inIn) {
            NetData::Fragment::ErasePrefixTo(&(*in), PackCont(cont.fragNo, cont.partNo));
            NetData::Fragment::CopySuffixFrom(*extra, PackCont(0, 0), &(*in));
        } else {
            in->clear();
            NetData::Fragment::CopySuffixFrom(*extra, PackCont(cont.fragNo, cont.partNo), &(*in));
        }
    }

    PostProcessPackWrite::PostProcessPackWrite(shared_ptr<deque<string> > dest, shared_ptr<deque<string> > src) : dest(dest), src(src) {}

    void PostProcessPackWrite::Process() {
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

    shared_ptr<Pipe> PipeMaker::MakePacket() {
        auto p = make_shared<Pipe>();
        auto r = PipeMaker::MakePacketR();
        p->pr = r;
        return p;
    }

    shared_ptr<PipePacket> PipeMaker::MakePacketR() {
        shared_ptr<PipePacket> pl = make_shared<PipePacket>();
        pl->pt = PipeType::Packet;
        return pl;
    }

    shared_ptr<PipePacket> PipeMaker::CastPacket(shared_ptr<PipeR> w) {
        assert(w->pt == PipeType::Packet);
        auto q = dynamic_pointer_cast<PipePacket>(w);
        if (!q) throw bad_cast();
        return q;
    }

    void PipeSet::MergePacketed(const vector<ConToken> &toks) {
        set<ConToken, ConTokenLess> ptoks, mtoks;
        for (auto &i : pipes) ptoks.insert(i.first);
        for (auto &i : toks) mtoks.insert(i);

        vector<ConToken> toCreate;
        set_difference(mtoks.begin(), mtoks.end(), ptoks.begin(), ptoks.end(), back_inserter(toCreate), ConTokenLess());

        for (auto &i : toCreate) assert(pipes.find(i) == pipes.end());
        for (auto &i : toCreate) pipes[i] = PipeMaker::MakePacket();
        for (auto &i : toCreate) LOG(INFO) << "Creating Packet Pipe " << i.id;
    }

    void PipeSet::RemakeForRead(const vector<MessSock::StagedRead_t> &sockReads) {
        vector<shared_ptr<PostProcess> > pc;

        for (auto &i : sockReads) {
            if (pipes.find(i.tok) == pipes.end()) { LOG(ERROR) << "Read of inexistant " << i.tok.id; continue; }
            pipes[i.tok]->pr->RemakeForRead(&pc, i);
        }

        for (auto &i : pc) i->Process();
    }

};
