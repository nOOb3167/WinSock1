#ifndef _NET_STUFF_H_
#define _NET_STUFF_H_

#include <cstdint>

#include <vector>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <memory>

#include <winsock2.h>
#include <ws2tcpip.h>

class WinsockWrap {
    WSADATA wsd;
public:
    WinsockWrap();
    ~WinsockWrap();
};

namespace NetData {
    using namespace std;

    class NetExc : ::std::exception {};
    class NetFailureExc : NetExc {};
    class NetDisconnectExc : NetFailureExc {};
    class NetBlockExc : NetExc {};

    typedef uint32_t Stamp;

    Stamp EmptyStamp();

    string Uint32ToString(uint32_t x);

    struct PackCont {
        size_t fragNo;
        size_t partNo;

        PackCont();
        PackCont(size_t fragNo, size_t partNo);
    };

    struct PackContR {
        size_t fragNo;
        size_t partNo;
        bool inIn;

        PackContR();
        PackContR(size_t fragNo, size_t partNo, bool inIn);
    };

    class Fragment {
    public:
        Stamp stamp;
        string data;

        Fragment(const Stamp &s, const string &d);

        string SplitFragPrefix(size_t partNo) const;
        string SplitFragSuffix(size_t partNo) const;

        static void ErasePrefixTo(deque<Fragment> *deq, const PackCont &pc);
        static void CopySuffixFrom(const deque<Fragment> &deq, const PackCont &pc, deque<Fragment> *out);
    };

    /* FIXME: Is this even used? */
    class PrimitiveBase {
    public:
        virtual void WriteU(deque<Fragment>* w) = 0;
        virtual void ReadU(deque<Fragment>* w) = 0;
    };
};

/* No Win* Nix*, Just ifdef the whole thing, only have interfaces. */
/* NetNat and PollFdType allowed to use system-stuff. */
namespace NetNative {
    using namespace std;

    class NetFailureErrExc : NetData::NetFailureExc {
    private:
        int e;
        mutable string s;

    public:
        NetFailureErrExc();
        virtual const char * what() const;
    };

    class PollFdType {
    private:
        PollFdType(SOCKET s);
    public:
        SOCKET s;

        friend class NetFuncs;
    };

    class PrimitiveListening {
    public:
        PollFdType pfd;

        PrimitiveListening();
        virtual ~PrimitiveListening();

        vector<PollFdType> Accept() const;
    };

    class NetFuncs {
    public:
        bool ErrorWouldBlock();
        void PollFdTypeRead(const PollFdType &pfd, deque<NetData::Fragment>* w);

        PollFdType MakePollFdType(SOCKET s);
    };

    /* FIXME: Is this even used? */
    class PrimitiveSock : public NetData::PrimitiveBase {
    public:
        PollFdType s;

        virtual void WriteU(deque<NetData::Fragment>* w);
        virtual void ReadU(deque<NetData::Fragment>* w);
    };

    class MessSockSlave {
    private:
        vector<pollfd> pfds;

    public:
        void RebuildPollFrom(const vector<PollFdType> &p);
        void ReadyForPoll();
        vector<bool> PerformPoll();
    };

};

namespace NetStuff {
    using namespace std;

    using namespace NetData;
    using namespace NetNative;

    /* FIXME: Is this even used? */
    class PrimitiveMemonly : public PrimitiveBase {
    public:
        deque<Fragment> write;
        deque<Fragment> read;

        PrimitiveMemonly(const char **strs);

        void WriteU(deque<Fragment>* w);
        void ReadU(deque<Fragment>* w);

        static vector<PrimitiveMemonly> MakePrims(const char **strs);
    };

    /* FIXME: Is this even used? */
    class PrimitiveZombie : public PrimitiveBase {
    public:
        void WriteU(deque<Fragment>* w);
        void ReadU(deque<Fragment>* w);
    };

    class ConToken {
    public:
        uint32_t id;
        ConToken(uint32_t id);
    };

    struct ConTokenLess : std::binary_function<ConToken, ConToken, bool> {
        bool operator() (const ConToken &lhs, const ConToken &rhs) const;
    };

    class ConTokenGen {
        int maxTokens;
        set<ConToken, ConTokenLess> toks;
    public:
        ConTokenGen();

        ConToken GetToken();

        void ReturnToken(ConToken tok);
    };

    class PackContIt : public ::std::iterator<::std::input_iterator_tag, Fragment> {
    public:
        /* Should be CopyConstructible, Assignable */
        const deque<Fragment> *fst, *snd;
        PackContR cont;
        mutable int canary, canary_limit; /* FIXME: Cheese */

        PackContIt(const deque<Fragment> &fst, const deque<Fragment> &snd);
        void AdvancePart();
        void AdvanceFrag();
        void AdvanceToPart(size_t w);
        const string & CurFragData() const;
        size_t CurPart() const;
        bool EndFragP() const;
        bool SameFragP(const PackContIt &rhs) const;

        static void GetFromTo(const PackContIt &from, const PackContIt &to, string *accum);
    };

    class MessSock {
    public:
        typedef struct { ConToken tok; deque<Fragment> in; } StagedRead_t;
        typedef struct { ConToken tok; bool graceful; } StagedDisc_t;

        struct Staged_t {
            shared_ptr<vector<StagedRead_t> > r;
            shared_ptr<vector<StagedDisc_t> > d;
            Staged_t();
        };

    private:
        struct CtData {
            PollFdType pfd;
            deque<Fragment> in;
            deque<Fragment> out;
            bool knownClosed;
            CtData(PollFdType pfd);
        };

        ConTokenGen tokenGen;
        map<ConToken, CtData, ConTokenLess> cons;
        uint32_t numCons;

        MessSockSlave aux;

        vector<PollFdType> GetPollFds();
        void UpdateCons();
        void AddConsMulti(const vector<PollFdType> &pfds, const vector<ConToken> toks, const vector<CtData> cts);

    public:
        MessSock();

        void AcceptedConsMulti(const vector<PollFdType> &pfds);
        vector<ConToken> GetConTokens() const;
        Staged_t StagedRead();
    };

    class MessMemonly {
        struct CtData {
            PrimitiveMemonly pmo;
            CtData(PrimitiveMemonly pmo);
        };

        ConTokenGen tokenGen;
        map<ConToken, CtData, ConTokenLess> cons;
        uint32_t numCons;

    public:
        MessMemonly();

        void AcceptedConsMulti(const vector<PrimitiveMemonly> &mems);
        vector<ConToken> GetConTokens() const;
        MessSock::Staged_t StagedRead();
    };

    namespace PackNlDelEx {
        bool ReadyPacketPos(PackContIt *fpos);
        bool GetPacket(PackContIt *pos, string *out);
    };

    enum class PipeType {
        Packet
    };

    struct PostProcess {
        virtual void Process();
    };

    class PipeI;
    class PipeR;

    class PipeI {
    public:
        virtual PipeR * RemakeForRead(vector<shared_ptr<PostProcess> > *pp, const MessSock::StagedRead_t &sr) = 0;
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
        const MessSock::StagedRead_t *sr;
        PostProcessFragmentWrite(shared_ptr<deque<Fragment> > deq, const MessSock::StagedRead_t &sr);
        virtual void Process();
    };

    struct PostProcessCullPrefixAndMerge : PostProcess {
        PackContR cont;
        shared_ptr<deque<Fragment> > in;
        const deque<Fragment> *extra;
        PostProcessCullPrefixAndMerge(shared_ptr<deque<Fragment> > in, const deque<Fragment> &extra, const PackContR cont);
        virtual void Process();
    };

    struct PostProcessPackWrite : PostProcess {
        shared_ptr<deque<string> > dest;
        shared_ptr<deque<string> > src;
        PostProcessPackWrite(shared_ptr<deque<string> > dest, shared_ptr<deque<string> > src);
        virtual void Process();
    };

    class PipePacket : public PipeR {
    public:
        shared_ptr<deque<Fragment> > in;
        shared_ptr<deque<Fragment> > out;

        shared_ptr<deque<string> > inPack;

        PipePacket();

        virtual PipePacket * RemakeForRead(vector<shared_ptr<PostProcess> > *pp, const MessSock::StagedRead_t &sr);
    };

    class PipeMaker {
    private:
        static shared_ptr<PipePacket> MakePacketR();
    public:
        static shared_ptr<Pipe> MakePacket();

        static shared_ptr<PipePacket> CastPacket(shared_ptr<PipeR> w);
    };

    class PipeSet {
    public:
        map<ConToken, shared_ptr<Pipe>, ConTokenLess> pipes;

        void MergePacketed(const vector<ConToken> &toks);
        void RemakeForRead(const vector<MessSock::StagedRead_t> &sockReads);
    };

};

#endif /* _NET_STUFF_H_ */
