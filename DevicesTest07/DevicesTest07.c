#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include "THREADSLib.h"
#include "Scheduler.h"
#include "Messaging.h"
#include "SystemCalls.h"
#include "TestCommon.h"
#include "libuser.h"

TestDiskParameters testCases[4][7];

/*********************************************************************************
*
* DevicesTest07
*
* Purpose:
*   Concurrent multi-process disk arm scheduling test.  Four child processes
*   are spawned simultaneously, each issuing a sequence of 7 single-sector
*   writes to disk0 across varying tracks.  The interleaved request stream
*   exercises the arm-scheduling algorithm (SSF by default) under realistic
*   concurrent load.
*
* Track patterns per child process:
*   Child 1:  { 0, 3, 0, 5, 0, 5, 0 }
*   Child 2:  { 5, 4, 1, 6, 1, 6, 1 }
*   Child 3:  { 9, 9, 2, 7, 2, 7, 2 }
*   Child 4:  { 2, 9, 3, 8, 3, 8, 3 }
*
*   Each child also derives its sectorStart as (track % 6), giving a
*   non-trivial sector spread across the requests.
*
* Flow:
*   1. Build a 7-entry TestDiskParameters array for each of the 4 children.
*   2. Spawn all 4 children at priority 3 with sleepTime = 0 (immediate).
*   3. Parent calls Wait() four times (output suppressed to reveal the
*      seek pattern in the DiskDriver trace).
*   4. Parent prints "Test Complete" and exits.
*
* Expected output:
*   DevicesTest07: started
*   DevicesTest07-Child<n>: started                     (x4, order varies)
*   DevicesTest07-Child<n>: DiskWrite (disk0) - platter 0, track XX, ...  (x28 total)
*   DevicesTest07: Test Complete
*
* Pass criteria:
*   - All 28 DiskWrite calls complete with status == 0.
*   - All four Wait() calls return successfully.
*   - The seek sequence visible in debug output should reflect SSF
*     (nearest-track-first) ordering rather than FCFS.
*
* Covers:
*   - Multiple concurrent processes sharing the same disk request queue
*   - SSF (Shortest Seek First) arm scheduling with simultaneous requests
*   - TList ordering and GetNextRequestingProc under a live mixed queue
*   - Priority-3 children contending on a single disk
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
    int i, id, result;
    int trackPatterns[][7] = 
    { 
        {0,3,0,5,0,5,0 },
        {5,4,1,6,1,6,1 },
        {9,9,2,7,2,7,2 },
        {2,9,3,8,3,8,3 }
    };


    /* Initialize a set of 4 write/read patterns.  Each is given to a separate process. */
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 7; ++j)
        {
            testCases[i][j].disk = 0;
            testCases[i][j].platter = 0;
            testCases[i][j].read = FALSE;
            testCases[i][j].sectorCount = 1;
            testCases[i][j].track = trackPatterns[i][j];
            testCases[i][j].sectorStart = trackPatterns[i][j] % 6;
        }
    }

    /* Just output a message and exit. */
    console_output(FALSE, "\t%s: started\n", testName);


    for (i = 0; i < 4; i++)
    {
        /* 0 sleep time ... */
        optionSeparator = CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, testCases[i], 7, 0);
        Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 3, &kidPid);
        optionSeparator[0] = '\0';
    }

    /* no output to see the pattern */
    for (i = 0; i < 4; i++)
    {
        result = Wait(&kidPid, &id);
    }

    console_output(FALSE, "%s:\tTest Complete\n", testName);

    Exit(0);

    return 0;
}

