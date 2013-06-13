#include "stdafx.h"
#include "CppUnitTest.h"

#include <memory>
#include <NetStuff/NetStuff.h>

using namespace std;

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTest1 {

    shared_ptr<WinsockWrap> ww;

    TEST_MODULE_INITIALIZE(Hello1) {
        ww = make_shared<WinsockWrap>();
    };

    TEST_CLASS(UnitTest1)
    {
    public:

        TEST_METHOD(TestMethod1)
        {
            Assert::IsTrue(1 == 1);
        }

    };
}