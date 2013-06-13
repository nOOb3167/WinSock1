#ifndef _LOGINC_H_
#define _LOGINC_H_

#pragma warning( push )
#pragma warning( disable : 4251 )

#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

#pragma warning( pop )

void LogincInit();

#endif /* _LOGINC_H_ */
