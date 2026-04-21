#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include "THREADSLib.h"
#include "Scheduler.h"
#include "Messaging.h"
#include "SystemCalls.h"
#include "TestCommon.h"
#include "libuser.h"

TestDiskParameters testCases[50];

/*********************************************************************************
*
* DevicesTest10
*
* Purpose:
*   Within-track sector ordering test.  Eight child processes each write
*   one sector to the same track (track 3) of disk0, platter 0, but to
*   different sectors.  Because all requests target the same track, no
*   arm seeks are required -- the test focuses on the sector-level
*   dispatch ordering once the arm is already positioned.
*
* Sector pattern (one sector per child, in spawn order):
*   { 5, 3, 9, 0, 7, 2, 1, 6 }
*
* Flow:
*   1. Spawn 8 children at priority 2 with sleepTime = 0 (all start
*      immediately, all target track 3).
*   2. Parent calls Wait() 8 times (output suppressed).
*   3. Parent prints "Test complete" and exits.
*
* Expected output:
*   DevicesTest10: started
*   DevicesTest10-Child<n>: started                         (x8)
*   DevicesTest10-Child<n>: DiskWrite (disk0) - track 03, sector XX, ... (x8)
*   DevicesTest10: Test complete
*
* Pass criteria:
*   - All 8 DiskWrite calls complete with status == 0.
*   - All 8 Wait() calls return successfully.
*   - No seek should occur between writes (arm stays at track 3).
*
* Covers:
*   - Multiple requests queued to the same track
*   - Sector scheduling when arm position == requested track for all requests
*   - SSF degenerates to an arbitrary order when all distances == 0;
*     verifies no crash or livelock in that degenerate case
*   - TListGetClosestNode behavior with distance-0 neighbors
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    char nameBuffer[512];
    int childId = 0;
    int kidPid;
    char* optionSeparator;
    int messageCount = 0;
    char childNames[MAXPROC][256];
    int i, id, result;
    int trackPatterns[] = { 5,3,9,0,7,2,1,6 };

    /* Just output a message and exit. */
    console_output(FALSE, "\n%s: started\n", testName);

    /* Scheduling test with one track, different sectors. */
    for (int i = 0; i < 8; ++i)
    {
        testCases[i].disk = 0;
        testCases[i].platter = 0;
        testCases[i].read = FALSE;
        testCases[i].sectorCount = 1;
        testCases[i].track = 3;
        testCases[i].sectorStart = trackPatterns[i];

        /* 0 sleep time ... */
        optionSeparator = CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[i], 1, 0);
        Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 2, &kidPid);
        optionSeparator[0] = '\0';
        strncpy(childNames[kidPid], nameBuffer, 256);
    }



    /* wait with no output to show the results. */
    for (i = 0; i < 8; i++)
    {
        result = Wait(&kidPid, &id);
    }

    console_output(FALSE, "%s: Test complete\n", testName, result, kidPid, id);
    Exit(0);

    return 0;
}
