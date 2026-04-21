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

TestDiskParameters writeCases[8];
TestDiskParameters readCases[8];

/*********************************************************************************
*
* DevicesTest19
*
* Purpose:
*   Data integrity stress test with racing (unsynchronized) writers and
*   readers.  Eight writer processes and eight reader processes are spawned
*   simultaneously, all targeting the same eight disk locations.  Readers and
*   writers race against each other with no barrier between them.
*
*   Because readers may execute before their paired writer completes, a
*   content mismatch is NOT necessarily a bug.  The primary pass criterion
*   is that all 16 operations COMPLETE without error (status == 0) and no
*   process hangs or deadlocks.  Any "Output match!" lines in the output are
*   a bonus indicating a reader happened to run after its writer.
*
*   The racing design stresses the queue and semaphore machinery under
*   maximum concurrency.  A hang or crash here indicates a race condition
*   in the driver's locking, queue manipulation, or process unblock logic.
*
* Write/Read assignments (all disk0, platter 0, sector 9):
*   Track  2  -- message A    Track  8  -- message B
*   Track  4  -- message B    Track 10  -- message A
*   Track  6  -- message A    Track 12  -- message B
*                              Track 14  -- message A
*                              Track 15  -- message B
*
*   Writers are spawned at priority 3; readers at priority 3 also, so the
*   scheduler may interleave them freely.
*
* Flow:
*   1. Spawn 8 writer children (priority 3) -- do NOT wait.
*   2. Spawn 8 reader children (priority 3) -- do NOT wait.
*   3. Wait() x16 for all children.
*   4. Print "Test complete" and exit.
*
* Expected output:
*   DevicesTest19: started
*   DevicesTest19-Child<1-16>: started                (x16, interleaved)
*   DevicesTest19-Child<1-8>:  DiskWrite (disk0) ...  (x8, order varies)
*   DevicesTest19-Child<9-16>: DiskRead  (disk0) ...  (x8, order varies)
*   DevicesTest19-Child<n>: DiskRead - Output match!  (0-8 times, timing dependent)
*   DevicesTest19: Test complete
*
* Pass criteria:
*   - All 16 operations complete with status == 0.
*   - All 16 Wait() calls return -- no hang or deadlock.
*   - Content mismatches are acceptable; crashes and hangs are not.
*
* Covers:
*   - Maximum concurrency: 16 processes simultaneously in the disk request path
*   - Readers and writers racing to the same disk locations
*   - Queue mutex (diskQueueMutex) correctness under high contention
*   - devicesProcs[] table collision resistance (16 pids, MAXPROC = 50)
*   - No deadlock when a reader is serviced before its paired writer
*
* See also:
*   DevicesTest18 -- same test with synchronized waves for deterministic
*                    pass/fail on data integrity.
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    char nameBuffer[512];
    int childId = 0;
    int kidPid;
    int i, id, result;

    int tracks[]   = {  2,  4,  6,  8, 10, 12, 14, 15 };
    int messages[] = {
        DEVICES_OPTION_MESSAGEA,
        DEVICES_OPTION_MESSAGEB,
        DEVICES_OPTION_MESSAGEA,
        DEVICES_OPTION_MESSAGEB,
        DEVICES_OPTION_MESSAGEA,
        DEVICES_OPTION_MESSAGEB,
        DEVICES_OPTION_MESSAGEA,
        DEVICES_OPTION_MESSAGEB
    };

    console_output(FALSE, "\n%s: started\n", testName);

    /* Spawn all 8 writers -- do NOT wait before spawning readers. */
    for (i = 0; i < 8; ++i)
    {
        writeCases[i].disk        = 0;
        writeCases[i].platter     = 0;
        writeCases[i].read        = FALSE;
        writeCases[i].sectorCount = 1;
        writeCases[i].track       = tracks[i];
        writeCases[i].sectorStart = 9;

        CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &writeCases[i], 1, messages[i]);
        Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 3, &kidPid);
    }

    /* Spawn all 8 readers immediately -- they race the writers. */
    for (i = 0; i < 8; ++i)
    {
        readCases[i].disk        = 0;
        readCases[i].platter     = 0;
        readCases[i].read        = TRUE;
        readCases[i].sectorCount = 1;
        readCases[i].track       = tracks[i];
        readCases[i].sectorStart = 9;

        CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, 0, &readCases[i], 1, messages[i]);
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
