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
* DevicesTest15
*
* Purpose:
*   Elevator (SCAN) arm scheduling verification test.  Uses a track pattern
*   that spans both sides of the starting arm position, forcing the Elevator
*   algorithm to complete a full forward sweep, reverse direction, and service
*   the remaining requests on the way back.  Must be compiled with DISK_ARM_ALG
*   set to DISK_ARM_ALG_ELEVATOR in Devices.c.
*
* Design:
*   The arm is anchored at track 8 by a synchronous write (phase 1).
*   Eight children are spawned simultaneously (phase 2) targeting tracks
*   on both sides of 8:
*
*     Tracks: { 2, 4, 6, 10, 12, 14, 5, 3 }
*
*   The Elevator sweeps forward first (>= current arm), then reverses:
*
*     Elevator expected:     10 -> 12 -> 14 -> 6 -> 5 -> 4 -> 3 -> 2
*
*   Under other algorithms the output will differ:
*
*     SSTF expected:         10 -> 6  -> 5  -> 4 -> 3 -> 2 -> 12 -> 14
*     FCFS expected:         2  -> 4  -> 6  -> 10 -> 12 -> 14 -> 5 -> 3
*     OneDirection expected: 10 -> 12 -> 14 -> 2  -> 3  -> 4  -> 5  -> 6
*
*   The Elevator/OneDirection distinction is visible at the end of the sweep:
*   Elevator reverses (14 -> 6 -> ... -> 2, descending),
*   OneDirection wraps (14 -> 2 -> 3 -> ... -> 6, ascending after wrap).
*
* Flow:
*   Phase 1: Single child writes to track 8, sector 9.  Parent waits.
*   Phase 2: Spawn 8 children simultaneously at priority 5.  Parent waits
*            for all 8.
*
* Expected output (Elevator):
*   DevicesTest15: started
*   DevicesTest15-Child1:  DiskWrite (disk0) track 08 ...   (phase 1)
*   DevicesTest15-Child<2-9>: DiskWrite track order:
*     10, 12, 14, 6, 5, 4, 3, 2                (forward sweep then reverse)
*   DevicesTest15: Test complete
*
* Pass criteria:
*   - All 9 DiskWrite calls complete with status == 0.
*   - All Wait() calls return successfully.
*   - With DISK_ARM_ALG_ELEVATOR the visible seek order shows a forward
*     sweep followed by a reverse sweep, not a zigzag or wrap.
*
* Covers:
*   - Elevator forward direction initial sweep
*   - Direction reversal when no more requests exist in the forward direction
*   - Bidirectional arm state correctly maintained across requests
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

    /* Tracks on both sides of anchor (8): forward side 10,12,14 and
       reverse side 2,3,4,5,6.  Spawn order is intentionally not sorted. */
    int trackPatterns[] = { 2, 4, 6, 10, 12, 14, 5, 3 };

    console_output(FALSE, "\n%s: started\n", testName);

    /* Phase 1: anchor the arm at track 8 (synchronous). */
    testCases[0].disk        = 0;
    testCases[0].platter     = 0;
    testCases[0].read        = FALSE;
    testCases[0].sectorCount = 1;
    testCases[0].track       = 8;
    testCases[0].sectorStart = 9;

    CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[0], 1, 0);
    Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 2, &kidPid);
    Wait(&kidPid, &status);

    /* Phase 2: flood the queue with 8 concurrent requests spanning the arm. */
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
