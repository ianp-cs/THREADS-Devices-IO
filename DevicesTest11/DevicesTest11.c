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
* DevicesTest11
*
* Purpose:
*   Mixed read/write arm scheduling test with 16 concurrent child processes.
*   Alternating children issue reads (even i) and writes (odd i) to disk0,
*   covering a spread of tracks.  Tests that the driver handles interleaved
*   read and write requests in the queue without data corruption or deadlock.
*
* Track pattern (index i drives child i+1):
*   { 11, 12, 4, 5, 0, 9, 4, 12, 9, 2, 10, 0, 9, 13, 14, 13 }
*
*   read flag   = (i % 2)  -- child 1 reads, child 2 writes, alternating
*   sectorStart = (track % 4)
*
* Flow:
*   1. Spawn 16 children at priority 2 with sleepTime = 0.
*   2. Parent calls Wait() 16 times (output suppressed).
*   3. Parent prints "Test complete" and exits.
*
* Expected output:
*   DevicesTest11: started
*   DevicesTest11-Child<n>: started                              (x16)
*   DevicesTest11-Child<n>: DiskRead/DiskWrite (disk0) - ...    (x16)
*   DevicesTest11: Test complete
*
* Pass criteria:
*   - All 16 disk operations complete with status == 0.
*   - All 16 Wait() calls return successfully.
*   - No deadlock between concurrent readers and writers.
*
* Note:
*   Because writes precede reads for most tracks but spawn order is not
*   guaranteed, read operations may return uninitialized data from the
*   disk image.  The test does not validate buffer contents -- it only
*   verifies that operations complete successfully.
*
* Covers:
*   - Mixed DISK_READ / DISK_WRITE operations in the same request queue
*   - SSF scheduling across a 0-14 track range with duplicates
*     (tracks 0, 4, 9, 12, 13 appear twice)
*   - Correct DISK_READ input_data vs DISK_WRITE output_data pointer
*     assignment in DiskTrackIO
*   - 16 processes all blocked on waitSem simultaneously
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
    int trackPatterns[] = { 11,12,4,5,0,9,4,12,9,2,10,0,9,13,14,13 };

    /* Just output a message and exit. */
    console_output(FALSE, "\n%s: started\n", testName);

    /* Mix of reads and writes. */
    for (int i = 0; i < 16; ++i)
    {
        testCases[i].disk = 0;
        testCases[i].platter = 0;
        testCases[i].read = i % 2;
        testCases[i].sectorCount = 1;
        testCases[i].track = trackPatterns[i % 16];
        testCases[i].sectorStart = trackPatterns[i % 16] % 4;

        /* 0 sleep time ... */
        optionSeparator = CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &testCases[i], 1, 0);
        Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 2, &kidPid);
        optionSeparator[0] = '\0';
        strncpy(childNames[kidPid], nameBuffer, 256);
    }



    /* wait with no output to show the results. */
    for (i = 0; i < 16; i++)
    {
        result = Wait(&kidPid, &id);
    }
    console_output(FALSE, "%s: Test complete\n", testName, result, kidPid, id);
    Exit(0);

    return 0;
}
