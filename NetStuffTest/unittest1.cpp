#include "stdafx.h"
#include "CppUnitTest.h"

#include <memory>
#include <NetStuff/NetStuff.h>
#include <NetStuff/loginc.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std;
using namespace NetData;
using namespace NetStuff;
using namespace NetNative;

namespace UnitTest1 {

    shared_ptr<WinsockWrap> ww;

    TEST_MODULE_INITIALIZE(Hello1) {
        LogincInit();
        ww = make_shared<WinsockWrap>();
    };

    TEST_CLASS(UnitTest1)
    {
    public:

        TEST_METHOD(TestMethod1) {
            Assert::IsTrue(1 == 1);
        }

        TEST_METHOD(MsgSplit) {
            /* Used to crash due to OOB in PackContIt::FragAdvance in case of rollover. */

            const char *pmss[] = {
                "ccc", "dddd\n", 0,
                0,
            };
            vector<PrimitiveMemonly> pms = PrimitiveMemonly::MakePrims(pmss);

            auto m = make_shared<MessMemonly>();
            m->AcceptedConsMulti(pms);

            auto ps = make_shared<PipeSet>();

            for (size_t i = 0; i < 3; i++) {

                ps->MergePacketed(m->GetConTokens());

                const auto sg = m->StagedRead();

                ps->RemakeForRead(*sg.r);
            }
        };

        TEST_METHOD(MsgBasic) {

            const char *pmss[] = {
                "aaa\n", "bbbb", 0,
                "ccc", "dddd\n", "eeeee\n", 0,
                0,
            };
            vector<PrimitiveMemonly> pms = PrimitiveMemonly::MakePrims(pmss);

            auto m = make_shared<MessMemonly>();
            m->AcceptedConsMulti(pms);

            auto ps = make_shared<PipeSet>();

            for (size_t i = 0; i < 3; i++) {

                ps->MergePacketed(m->GetConTokens());

                const auto sg = m->StagedRead();

                ps->RemakeForRead(*sg.r);
            }

            Assert::IsTrue(m->GetConTokens().size() == 2 && ps->pipes.size() == 2);
            Assert::IsTrue(m->GetConTokens()[0].id == 0 && m->GetConTokens()[1].id == 1);

            {
                auto w = PipeMaker::CastPacket(ps->pipes[m->GetConTokens()[0]]->pr);
                Assert::IsTrue(w->inPack->size() == 1);
            }
            {
                auto w = PipeMaker::CastPacket(ps->pipes[m->GetConTokens()[1]]->pr);
                Assert::IsTrue(w->inPack->size() == 2);
            }
        };

    };
}