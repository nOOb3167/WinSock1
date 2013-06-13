#include "stdafx.h"
#include "CppUnitTest.h"

#include <memory>
#include <stdexcept>

#include <NetStuff/NetStuff.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std;

namespace NetStuffTest {

    //shared_ptr<WinsockWrap> ww;

    TEST_MODULE_INITIALIZE(ModInit) {
        //NetData::PackCont z = NetData::PackCont(0, 123);
        NetData::PackCont z;
    }

	TEST_CLASS(UnitTest1) {
	public:
		
		TEST_METHOD(TestMethod1) {
			Assert::AreEqual(1, 1);
		}

	};
}