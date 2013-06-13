#include <stdafx.h>

#include <cassert>

#include <loginc.h>

void LogincInit() {
    static bool already = false;
    if (already) assert(0);
    else         already = true;

    FLAGS_logtostderr = 1;
	google::InitGoogleLogging("WinSock1");
}
