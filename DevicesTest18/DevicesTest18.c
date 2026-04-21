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
* DevicesTest18
*
* Purpose:
*   Data integrity test under concurrent disk I/O using synchronized write
*   and read waves.  Eight concurrent writer processes write known message
*   patterns to eight distinct tracks.  After ALL writers complete, eight
*   concurrent reader processes read each location back and verify the
*   correct content is present.
*
*   The synchronized-wave design guarantees readers never run before their
*   paired writer completes, making any content mismatch a reliable indicator
*   of a data corruption bug (wrong buffer pointer, wrong track/sector
*   calculation, queue state corruption, etc.).
*
* Write assignments (all disk0, platter 0, sector 9):
*   Track  2  -- message A
*   Track  4  -- message B
*   Track  6  -- message A
*   Track  8  -- message B
*   Track 10  -- message A
*   Track 12  -- message B
*   Track 14  -- message A
*   Track 16... wait, disk0 has 128 tracks (0-127).
*   Track 15  -- message B
*
* Read assignments: same locations as writes, checking for the same message.
*
* Flow:
*   Wave 1: Spawn 8 writer children (priority 3).  Parent waits for all 8.
*   Wave 2: Spawn 8 reader children (priority 3).  Parent waits for all 8.
*   Parent prints "Test complete" and exits.
*
* Expected output:
*   DevicesTest18: started
*   DevicesTest18-Child<1-8>:  DiskWrite (disk0) track XX ...   (x8)
*   DevicesTest18-Child<9-16>: DiskRead  (disk0) track XX ...   (x8)
*   DevicesTest18-Child<9-16>: DiskRead  (disk0) - Output match!  (x8)
*   DevicesTest18: Test complete
*
* Pass criteria:
*   - All 8 DiskWrite calls complete with status == 0.
*   - All 8 DiskRead calls complete with status == 0.
*   - All 8 read children report "Output match!" -- any mismatch indicates
*     a data integrity bug.
*
* Covers:
*   - Correct buffer pointer management across 8 concurrent write requests
*   - Data survives concurrent queue activity without corruption
*   - Write-then-read integrity across distinct tracks under scheduler load
*   - Both message patterns (A and B) verified independently
*   - Synchronized wave design provides deterministic pass/fail signal
*
* See also:
*   DevicesTest19 -- same test with racing (unsynchronized) writers and readers.
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    char nameBuffer[512];
    int childId = 0;
    int kidPid;
    int i, id, result;

    /* Alternating message A/B across 8 distinct tracks. */
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

    /* Wave 1: 8 concurrent writers. */
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

    /* Wait for all writers before starting readers. */
    for (i = 0; i < 8; i++)
    {
        result = Wait(&kidPid, &id);
    }

    /* Wave 2: 8 concurrent readers checking the same locations. */
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

    for (i = 0; i < 8; i++)
    {
        result = Wait(&kidPid, &id);
    }

    console_output(FALSE, "%s: Test complete\n", testName);
    Exit(0);

    return 0;
}
