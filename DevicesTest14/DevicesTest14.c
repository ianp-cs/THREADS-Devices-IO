#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include "THREADSLib.h"
#include "Scheduler.h"
#include "Messaging.h"
#include "SystemCalls.h"
#include "Devices.h"
#include "TestCommon.h"
#include "libuser.h"

TestDiskParameters testCases[50];

/*********************************************************************************
*
* DevicesTest14
*
* Purpose:
*   FCFS (First Come First Serve) arm scheduling verification test.
*   Uses a zigzag track pattern designed to produce a clearly distinct seek
*   sequence under FCFS versus all other algorithms.  Must be compiled with
*   DISK_ARM_ALG set to DISK_ARM_ALG_FCFS in Devices.c.
*
* Design:
*   The disk arm is anchored at track 6 by a synchronous write (phase 1).
*   Eight children are then spawned simultaneously (phase 2) targeting tracks
*   that deliberately alternate between low and high values:
*
*     Spawn order / tracks: { 1, 15, 2, 14, 3, 13, 4, 12 }
*
*   Under FCFS the driver services requests in arrival order, producing
*   an obvious zigzag seek pattern in the output:
*
*     FCFS expected:         1 -> 15 -> 2 -> 14 -> 3 -> 13 -> 4 -> 12
*
*   Under other algorithms the output will differ noticeably:
*
*     SSTF expected:         4 -> 3 -> 2 -> 1 -> 12 -> 13 -> 14 -> 15
*     Elevator expected:     12 -> 13 -> 14 -> 15 -> 4 -> 3 -> 2 -> 1
*     OneDirection expected: 12 -> 13 -> 14 -> 15 -> 1 -> 2 -> 3 -> 4
*
* Flow:
*   Phase 1: Single child writes to track 6, sector 9.  Parent waits (arm
*            is now known to be at track 6).
*   Phase 2: Spawn 8 children simultaneously at priority 5.  Parent waits
*            for all 8.
*
* Expected output (FCFS):
*   DevicesTest14: started
*   DevicesTest14-Child1:  DiskWrite (disk0) track 06 ...   (phase 1)
*   DevicesTest14-Child<2-9>: DiskWrite track order:
*     01, 15, 02, 14, 03, 13, 04, 12                        (zigzag = FCFS)
*   DevicesTest14: Test complete
*
* Pass criteria:
*   - All 9 DiskWrite calls complete with status == 0.
*   - All Wait() calls return successfully.
*   - With DISK_ARM_ALG_FCFS the visible seek order matches spawn order.
*
* Covers:
*   - FCFS queue ordering (TListAddNode, not TListAddNodeInOrder)
*   - Requests serviced strictly in arrival order regardless of arm position
*   - Known arm starting position for deterministic output comparison
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    char nameBuffer[512];
    int childId = 0;
    int kidPid;
    int status;
    int i, id, result;

    /* Zigzag pattern: alternates low and high tracks to make FCFS visible. */
    int trackPatterns[] = { 1, 15, 2, 14, 3, 13, 4, 12 };

    console_output(FALSE, "\n%s: started\n", testName);

    /* Phase 1: anchor the arm at track 6 (synchronous). */
    testCases[0].disk        = 0;
    testCases[0].platter     = 0;
    testCases[0].read        = FALSE;
    testCases[0].sectorCount = 1;
    testCases[0].track       = 6;
    testCases[0].sectorStart = 9;

    CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[0], 1, 0);
    Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 2, &kidPid);
    Wait(&kidPid, &status);

    /* Phase 2: flood the queue with 8 concurrent zigzag requests. */
    for (i = 0; i < 8; ++i)
    {
        testCases[i + 1].disk        = 0;
        testCases[i + 1].platter     = 0;
        testCases[i + 1].read        = FALSE;
        testCases[i + 1].sectorCount = 1;
        testCases[i + 1].track       = trackPatterns[i];
        testCases[i + 1].sectorStart = 9;

        CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[i + 1], 1, 0);
        Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 5, &kidPid);
    }

    for (i = 0; i < 8; i++)
    {
        result = Wait(&kidPid, &id);
    }

    console_output(FALSE, "%s: Test complete\n", testName);
    Exit(0);

    return 0;
}
