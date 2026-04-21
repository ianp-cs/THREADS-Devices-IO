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
* DevicesTest13
*
* Purpose:
*   Arm-scheduling correctness test with a known starting position.
*   The disk arm is anchored at track 6 by a synchronous write before
*   any concurrent requests are queued.  Two subsequent batches of 7
*   concurrent high-priority writes are then issued, letting the
*   scheduler (SSF or Elevator) demonstrate its seek ordering from a
*   well-defined arm position.
*
* Phase 1 -- Anchor write (synchronous):
*   Single child (priority 2) writes message A to disk0, platter 0,
*   track 6, sector 9.  Parent waits for completion before continuing.
*   After this phase the disk arm is known to be at track 6.
*
* Phase 2 -- First concurrent batch (7 children, priority 5):
*   Tracks targeted: { 6, 15, 3, 4, 7, 11, 12 }
*   All 7 children write message B to the same platter/sector (0/9).
*   Parent waits for all 7 before starting phase 3.
*
*   From arm position 6, SSF expected order:
*     6 (dist 0) -> 7 (dist 1) -> 4 (dist 3) -> 3 (dist 1) ->
*     11 (dist 8) -> 12 (dist 1) -> 15 (dist 3)
*
* Phase 3 -- Second concurrent batch (7 children, priority 5):
*   Tracks targeted: { 3, 14, 4, 1, 6, 5, 9 }
*   Same write parameters as phase 2.  Arm starts wherever phase 2 left it.
*   Parent waits for all 7.
*
* Flow:
*   1. Spawn phase-1 child; Wait (synchronous).
*   2. Spawn all 7 phase-2 children; Wait x7.
*   3. Spawn all 7 phase-3 children; Wait x7.
*   4. Print "Test complete" and exit.
*
* Expected output:
*   DevicesTest13: started
*   DevicesTest13-Child1:  DiskWrite (disk0) track 06 ...  (phase 1)
*   DevicesTest13-Child<2-8>:  DiskWrite (disk0) track XX ...  (phase 2, x7)
*   DevicesTest13-Child<9-15>: DiskWrite (disk0) track XX ...  (phase 3, x7)
*   DevicesTest13: Test complete
*
* Pass criteria:
*   - All 15 DiskWrite calls complete with status == 0.
*   - All Wait() calls return successfully.
*   - With SSF, the seek sequence for phase 2 (visible in driver debug
*     output) should demonstrate nearest-track-first ordering from
*     track 6, not the arbitrary spawn order.
*
* Covers:
*   - Controlled arm position as a precondition for scheduling analysis
*   - Priority-5 children flooding the queue before the driver processes any
*   - SSF (and optionally Elevator) seek ordering with a known start track
*   - Two independent batches testing that arm state is correctly preserved
*     between phases
*   - 15 total children across three sequential synchronization points
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    char nameBuffer[512];
    int childId = 0;
    int kidPid;
    int status;
    char* optionSeparator;
    int messageCount = 0;
    int i, id, result;

    /* Just output a message and exit. */
    console_output(FALSE, "\n%s: started\n", testName);

    for (i = 0; i < 15; ++i)
    {
        testCases[i].disk = 0;
        testCases[i].platter = 0;
        testCases[i].read = 0;
        testCases[i].sectorCount = 1;
        testCases[i].track = 6;
        testCases[i].sectorStart = 9;
    }

    /* Sets the starting track to 6 */
    optionSeparator = CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[0], 1, DEVICES_OPTION_MESSAGEA);
    Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 2, &kidPid);
    Wait(&kidPid, &status);

    testCases[1].track = 6;
    testCases[2].track = 15;
    testCases[3].track = 3;
    testCases[4].track = 4;
    testCases[5].track = 7;
    testCases[6].track = 11;
    testCases[7].track = 12;

    for (i = 1; i < 8; ++i)
    {
        CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[i], 1, DEVICES_OPTION_MESSAGEB);
        Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 5, &kidPid);
    }

    /* wait with no output to show the results. */
    for (i = 0; i < 7; i++)
    {
        result = Wait(&kidPid, &id);
    }

    testCases[8].track = 3;
    testCases[9].track = 14;
    testCases[10].track = 4;
    testCases[11].track = 1;
    testCases[12].track = 6;
    testCases[13].track = 5;
    testCases[14].track = 9;

    for (i = 8; i < 15; ++i)
    {
        CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[i], 1, DEVICES_OPTION_MESSAGEB);
        Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 5, &kidPid);
    }

    /* wait with no output to show the results. */
    for (i = 0; i < 7; i++)
    {
        result = Wait(&kidPid, &id);
    }

    console_output(FALSE, "%s: Test complete\n", testName, result, kidPid, id);
    Exit(0);

    return 0;
}
