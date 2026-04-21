#include <stdio.h>
#include <stdlib.h>
#include "THREADSLib.h"
#include "Scheduler.h"
#include "Messaging.h"
#include "SystemCalls.h"
#include "TestCommon.h"
#include "libuser.h"

/*********************************************************************************
*
* DevicesTest08
*
* Purpose:
*   Boundary / error-handling test for DiskWrite.  Verifies that the driver
*   correctly rejects three categories of invalid parameters and returns a
*   non-zero error code in each case.  No valid I/O is performed.
*
* Test cases (all targeting disk0):
*
*   Case 1 -- Invalid sector:
*     DiskWrite("disk0", buf, platter=0, track=5, sector=17, count=1)
*     Sector 17 exceeds THREADS_DISK_SECTOR_COUNT (16, valid range 0-15).
*     Expected: return value != 0  -->  "DiskWrite sector error test PASSED"
*
*   Case 2 -- Invalid track:
*     DiskWrite("disk0", buf, platter=0, track=258, sector=0, count=1)
*     Track 258 exceeds the disk's track count (typically < 258 for test images).
*     Expected: return value != 0  -->  "DiskWrite track error test PASSED"
*
*   Case 3 -- Invalid platter:
*     DiskWrite("disk0", buf, platter=4, track=25, sector=0, count=1)
*     Platter 4 exceeds the disk's platter count (THREADS_DISK_MAX_PLATTERS = 3,
*     valid range 0-2 for a 3-platter disk).
*     Expected: return value != 0  -->  "DiskWrite track error test PASSED"
*     (Note: the output label says "track error" for this case -- that is
*     a copy-paste issue in the source, not a specification error.)
*
* Flow:
*   1. Fill outBuffer with a random byte value.
*   2. Attempt each invalid write and print PASSED/FAILED based on return value.
*   3. Exit.
*
* Expected output:
*   DevicesTest08:  Started
*   DevicesTest08:  Writing to an invalid sector
*   DevicesTest08:  DiskWrite sector error test PASSED
*   DevicesTest08:  Writing to an invalid track
*   DevicesTest08:  DiskWrite track error test PASSED
*   DevicesTest08:  Writing to an invalid platter
*   DevicesTest08:  DiskWrite track error test PASSED
*
* Pass criteria:
*   All three DiskWrite calls return a non-zero error code.
*
* Covers:
*   - Parameter validation in sys_disk_io (track, sector, platter range checks)
*   - Correct early-return path (-1) before the request is queued
*   - No crash or hang when invalid parameters are supplied
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    unsigned char outBuffer[THREADS_DISK_SECTOR_SIZE];
    int status;
    int result;

    srand(system_clock());
    memset(outBuffer, rand() % 0x100, sizeof(outBuffer));

    /* Test cases based on < 258 tracks. */

    console_output(FALSE, "\n%s:\tStarted\n", testName);

    console_output(FALSE, "%s:\tWriting to an invalid sector \n", testName);
    if ((result = DiskWrite("disk0", outBuffer, 0, 5, 17, 1, &status)) == 0)
    {
        console_output(FALSE, "%s:\tDiskWrite sector error test FAILED\n", testName);
    }
    else
    {
        console_output(FALSE, "%s:\tDiskWrite sector error test PASSED\n", testName);
    }

    console_output(FALSE, "%s:\tWriting to an invalid track \n", testName);
    if ((result = DiskWrite("disk0", outBuffer, 0, 258, 0, 1, &status)) == 0)
    {
        console_output(FALSE, "%s:\tDiskWrite track error test FAILED\n", testName);
    }
    else
    {
        console_output(FALSE, "%s:\tDiskWrite track error test PASSED\n", testName);
    }

    console_output(FALSE, "%s:\tWriting to an invalid platter \n", testName);
    if ((result = DiskWrite("disk0", outBuffer, 4, 25, 0, 1, &status)) == 0)
    {
        console_output(FALSE, "%s:\tDiskWrite platter error test FAILED\n", testName);
    }
    else
    {
        console_output(FALSE, "%s:\tDiskWrite platter error test PASSED\n", testName);
    }


    Exit(0);

    return 0;
}
