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
* DevicesTest16
*
* Purpose:
*   OneDirection (C-SCAN) arm scheduling verification test.  Uses a track
*   pattern that forces the arm to sweep to the highest requested track and
*   then wrap back to service requests near track 0, which is the defining
*   behaviour of C-SCAN.  Must be compiled with DISK_ARM_ALG set to
*   DISK_ARM_ALG_ONE_DIRECTION in Devices.c.
*
* Design:
*   The arm is anchored at track 10 by a synchronous write (phase 1).
*   Eight children are spawned simultaneously (phase 2) with tracks that
*   include a cluster near the top of the disk and a cluster near the bottom:
*
*     Tracks: { 12, 14, 15, 1, 3, 5, 11, 13 }
*
*   OneDirection sweeps forward to the highest track then wraps to the lowest:
*
*     OneDirection expected: 11 -> 12 -> 13 -> 14 -> 15 -> 1 -> 3 -> 5
*
*   Under other algorithms the output will differ:
*
*     Elevator expected:     11 -> 12 -> 13 -> 14 -> 15 -> 5 -> 3 -> 1
*     SSTF expected:         11 -> 12 -> 13 -> 14 -> 15 -> 5 -> 3 -> 1
*                            (coincides with Elevator for this pattern)
*     FCFS expected:         12 -> 14 -> 15 -> 1 -> 3 -> 5 -> 11 -> 13
*
*   The critical OneDirection vs Elevator distinction is visible at the end:
*   OneDirection wraps and ascends (... -> 15 -> 1 -> 3 -> 5),
*   Elevator reverses and descends  (... -> 15 -> 5 -> 3 -> 1).
*
* Flow:
*   Phase 1: Single child writes to track 10, sector 9.  Parent waits.
*   Phase 2: Spawn 8 children simultaneously at priority 5.  Parent waits
*            for all 8.
*
* Expected output (OneDirection):
*   DevicesTest16: started
*   DevicesTest16-Child1:  DiskWrite (disk0) track 10 ...   (phase 1)
*   DevicesTest16-Child<2-9>: DiskWrite track order:
*     11, 12, 13, 14, 15, 1, 3, 5          (forward sweep then wrap to low)
*   DevicesTest16: Test complete
*
* Pass criteria:
*   - All 9 DiskWrite calls complete with status == 0.
*   - All Wait() calls return successfully.
*   - With DISK_ARM_ALG_ONE_DIRECTION the visible seek order shows a forward
*     sweep that wraps to the low end in ascending order (not a reversal).
*
* Covers:
*   - OneDirection forward-only sweep
*   - Wrap-around from highest requested track back to lowest
*   - Ascending order after the wrap (distinguishes from Elevator reversal)
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

    /* Tracks: cluster near top (11-15) and cluster near bottom (1,3,5).
       Spawn order is intentionally unsorted to stress the queue ordering. */
    int trackPatterns[] = { 12, 14, 15, 1, 3, 5, 11, 13 };

    console_output(FALSE, "\n%s: started\n", testName);

    /* Phase 1: anchor the arm at track 10 (synchronous). */
    testCases[0].disk        = 0;
    testCases[0].platter     = 0;
    testCases[0].read        = FALSE;
    testCases[0].sectorCount = 1;
    testCases[0].track       = 10;
    testCases[0].sectorStart = 9;

    CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[0], 1, 0);
    Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 2, &kidPid);
    Wait(&kidPid, &status);

    /* Phase 2: flood the queue with high-end and low-end requests. */
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
