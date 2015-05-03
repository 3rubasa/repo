// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

//#define WIN32 1
#include "ST_sync.h"
#include "STAsync.h"
#include "MTTPCSync.h"

#include <map>

int main()
{
//    STSync server;
//
//    if (server.Init())
//        server.Start();

//    STAsync async_server;
//
//    if (async_server.Init())
//        async_server.Start();


    MTTPCSync MTTPCSync_server;

    if (MTTPCSync_server.Init())
        MTTPCSync_server.Start();

	return 0;
}
