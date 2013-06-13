#include "stdafx.h"
#include "CppUnitTest.h"

#include <Lib1/Header.h>
#include <NetStuff/NetStuff.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTest1
{		
    TEST_MODULE_INITIALIZE(Hello1) {
        1 == 1;
    };

    TEST_CLASS(UnitTest1)
    {
    public:

        TEST_METHOD(TestMethod1)
        {
            Lib1 a;
            NetData::PackCont b;
            Assert::IsTrue(a.Hello(10) == 110);
            // TODO: Your test code here
        }

    };
}