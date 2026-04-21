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
* DevicesTest17
*
* Purpose:
*   Two-disk simultaneous load test.  Floods both disk request queues
*   concurrently with 8 processes per disk (16 total), verifying that the
*   two DiskDriver instances operate independently without corrupting each
*   other's queues, semaphores, or process unblock logic.
*
* Design:
*   8 children target disk0 across tracks { 0, 3, 6, 9, 12, 15, 4, 8 }.
*   8 children target disk1 across tracks { 2, 5, 8, 11, 14, 1, 7, 10 }.
*   All 16 are spawned before any Wait() is called, maximising queue
*   pressure on both drivers simultaneously.
*
*   The test does not verify data content -- it verifies that all 16
*   operations complete successfully and that no process is left blocked.
*
* Flow:
*   1. Spawn 8 disk0 children (priority 3).
*   2. Spawn 8 disk1 children (priority 3).
*   3. Wait() x16.
*   4. Print "Test complete" and exit.
*
* Expected output:
*   DevicesTest17: started
*   DevicesTest17-Child<1-16>: started                      (x16, interleaved)
*   DevicesTest17-Child<1-8>:  DiskWrite (disk0) track XX   (x8)
*   DevicesTest17-Child<9-16>: DiskWrite (disk1) track XX   (x8)
*   DevicesTest17: Test complete
*
* Pass criteria:
*   - All 16 DiskWrite calls complete with status == 0.
*   - All 16 Wait() calls return successfully with no hang.
*   - No mixing of disk0/disk1 queue state.
*
* Covers:
*   - diskRequestQueue[0] and diskRequestQueue[1] active simultaneously
*   - diskSemaphores[0] and diskSemaphores[1] independent signalling
*   - diskQueueMutex[0] and diskQueueMutex[1] independent locking
*   - devicesProcs[] table shared across both disk request paths without
*     collision (procPosition = pid % MAXPROC is disk-agnostic)
*   - 16 processes simultaneously blocked on their per-process waitSem
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    char nameBuffer[512];
    int childId = 0;
    int kidPid;
    int i, id, result;

    int disk0Tracks[] = { 0, 3, 6, 9, 12, 15, 4, 8 };
    int disk1Tracks[] = { 2, 5, 8, 11, 14,  1, 7, 10 };

    console_output(FALSE, "\n%s: started\n", testName);

    /* Spawn 8 writers to disk0. */
    for (i = 0; i < 8; ++i)
    {
        testCases[i].disk        = 0;
        testCases[i].platter     = 0;
        testCases[i].read        = FALSE;
        testCases[i].sectorCount = 1;
        testCases[i].track       = disk0Tracks[i];
        testCases[i].sectorStart = disk0Tracks[i] % 8;

        CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[i], 1, 0);
        Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 3, &kidPid);
    }

    /* Spawn 8 writers to disk1. */
    for (i = 0; i < 8; ++i)
    {
        testCases[i + 8].disk        = 1;
        testCases[i + 8].platter     = 0;
        testCases[i + 8].read        = FALSE;
        testCases[i + 8].sectorCount = 1;
        testCases[i + 8].track       = disk1Tracks[i];
        testCases[i + 8].sectorStart = disk1Tracks[i] % 8;

        CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[i + 8], 1, 0);
        Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 3, &kidPid);
    }

    for (i = 0; i < 16; i++)
    {
        result = Wait(&kidPid, &id);
    }

    console_output(FALSE, "%s: Test complete\n", testName);
    Exit(0);

    return 0;
}
