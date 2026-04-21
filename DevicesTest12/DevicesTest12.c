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
* DevicesTest12
*
* Purpose:
*   Integration test combining concurrent sleep and disk I/O.  Sleeping
*   processes and disk-writing processes run simultaneously, verifying that
*   the clock driver and disk drivers operate independently without
*   interfering with each other.  Also tests that two separate writes to
*   the same disk location and two separate reads from a different disk
*   are handled without data corruption.
*
* Child processes spawned (9 total):
*
*   Children 1-5  (priority 2, sleep only):
*     Each sleeps for i seconds (0, 1, 2, 3, 4).  Child 1 sleeps 0 s
*     (exits immediately), child 5 sleeps 4 s.
*
*   Child 6  (priority 2, disk write):
*     Writes DEVICES_OPTION_MESSAGEA ("The cake is a lie. Disk 0 writes truth.")
*     to disk0, platter 0, track 7, sector 9.
*
*   Child 7  (priority 2, disk write):
*     Writes DEVICES_OPTION_MESSAGEB ("Interrupts don't scare me. Disk 1 ready.")
*     to disk0, platter 0, track 7, sector 9  (same location as child 6).
*     One message will overwrite the other; final content is race-dependent.
*
*   Child 8  (priority 1, disk read):
*     Reads from disk1, platter 0, track 7, sector 9 and checks for message A.
*     (disk1 was not written -- result is uninitialized data; mismatch expected.)
*
*   Child 9  (priority 1, disk read):
*     Reads from disk1, platter 0, track 7, sector 9 and checks for message B.
*     Same uninitialized-data caveat as child 8.
*
* Flow:
*   1. Spawn children 1-5 (sleep), then 6-7 (disk0 writes), then 8-9
*      (disk1 reads).
*   2. Parent calls Wait() 9 times (output suppressed on the wait loop).
*   3. Parent prints "Test complete" and exits.
*
* Expected output:
*   DevicesTest12: started
*   DevicesTest12-Child<1-9>: started           (x9, order varies)
*   DevicesTest12-Child<1-5>: Slept ~N ms       (N = 0-4000)
*   DevicesTest12-Child<6-7>: DiskWrite ...     (status = 0)
*   DevicesTest12-Child<8-9>: DiskRead  ...     (status = 0, mismatch expected)
*   DevicesTest12: Test complete
*
* Pass criteria:
*   - All 9 children exit and all 9 Wait() calls return successfully.
*   - Sleeping children report elapsed times proportional to their sleep durations.
*   - Disk writes complete without error.
*   - No deadlock or hang between clock and disk driver activity.
*
* Covers:
*   - Simultaneous clock driver and disk driver activity
*   - sleepingProcesses TList and diskRequestQueue[0/1] active concurrently
*   - Two writes racing to the same disk location (tests queue integrity,
*     not write ordering -- outcome is intentionally non-deterministic)
*   - DiskRead from a disk with no prior write (uninitialized sector)
*   - Priority differences between writers (priority 2) and readers (priority 1)
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
    char childNames[512][256];
    int i, id, result;

    /* Just output a message and exit. */
    console_output(FALSE, "\n%s: started\n", testName);


    /* Spawn 5 processes to sleep. */
    for (i = 0; i < 5; ++i)
    {

        /* i sleep time ... */
        optionSeparator = CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, i, 0, 0, 0);
        Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 2, &kidPid);
        optionSeparator[0] = '\0';
        strncpy(childNames[kidPid], nameBuffer, 256);
    }

    testCases[0].disk = 0;
    testCases[0].platter = 0;
    testCases[0].read = 0;
    testCases[0].sectorCount = 1;
    testCases[0].track = 7;
    testCases[0].sectorStart = 9;

    /* Write message A to disk 0 */
    optionSeparator = CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[0], 1, DEVICES_OPTION_MESSAGEA);
    Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 2, &kidPid);
    optionSeparator[0] = '\0';
    strncpy(childNames[kidPid], nameBuffer, 256);

    /* Write message B to disk 0 */
    optionSeparator = CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[0], 1, DEVICES_OPTION_MESSAGEB);
    Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 2, &kidPid);
    optionSeparator[0] = '\0';
    strncpy(childNames[kidPid], nameBuffer, 256);

    testCases[1].disk = 1;
    testCases[1].platter = 0;
    testCases[1].read = 1;
    testCases[1].sectorCount = 1;
    testCases[1].track = 7;
    testCases[1].sectorStart = 9;

    /* Read from disk 1 (unwritten - confirms read completes without error, no content check) */
    optionSeparator = CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[1], 1, 0);
    Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 1, &kidPid);
    optionSeparator[0] = '\0';
    strncpy(childNames[kidPid], nameBuffer, 256);

    /* Read from disk 1 (unwritten - confirms read completes without error, no content check) */
    optionSeparator = CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[1], 1, 0);
    Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 1, &kidPid);
    optionSeparator[0] = '\0';
    strncpy(childNames[kidPid], nameBuffer, 256);

    /* wait with no output to show the results. */
    for (i = 0; i < 9; i++)
    {
        result = Wait(&kidPid, &id);
    }
    console_output(FALSE, "%s: Test complete\n", testName, result, kidPid, id);
    Exit(0);

    return 0;
}
