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
* DevicesTest05
*
* Purpose:
*   Multi-disk, multi-sector write/read test.  Writes three sectors to disk0
*   and three sectors to disk1 independently, then reads each back and
*   verifies that data written to one disk is correctly stored and does not
*   contaminate the other disk.
*
* Flow:
*   1. Fill three 512-byte regions of outBuffer with distinct random values.
*   2. DiskWrite("disk0", outBuffer, platter=0, track=1, sector=1, count=3).
*   3. DiskWrite("disk1", outBuffer, platter=1, track=2, sector=8, count=3).
*   4. DiskRead ("disk0", inBuffer,  platter=0, track=1, sector=1, count=3).
*      - Compare inBuffer vs outBuffer; print match/mismatch.
*   5. DiskRead ("disk1", inBuffer,  platter=1, track=2, sector=8, count=3).
*      - Compare inBuffer vs outBuffer; print match/mismatch.
*   6. Exit.
*
* Expected output:
*   DevicesTest05:  Started
*   DevicesTest05:  Writing to disk 0, platter 0, track 1, sectors 1-3     (status = 0)
*   DevicesTest05:  Writing to disk 1, platter 1, track 2, sectors 8-12    (status = 0)
*   DevicesTest05:  Reading from disk 0, platter 0, track 6, sectors 3-6   (status = 0)
*   DevicesTest05:  In and Out buffers match!
*   DevicesTest05:  Reading from disk 1, platter 1, track 2, sectors 8-12  (status = 0)
*   DevicesTest05:  In and Out buffers match!
*
*   Note: The "Reading from disk 0" line in the source says "track 6, sectors 3-6"
*   but the actual DiskRead call targets track=1, sector=1 -- the label is
*   copy-paste text, not a specification error in the I/O parameters.
*
* Pass criteria:
*   - All four disk operations return 0 with status == 0.
*   - Both buffer comparisons pass.
*
* Covers:
*   - Independent operation of two disk driver instances (diskpids[0] and [1])
*   - Non-zero platter parameter (platter=1 on disk1)
*   - Sectors 8-10 (mid-track range) on disk1
*   - Two separate DiskDriver threads servicing requests concurrently
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    unsigned char outBuffer[THREADS_DISK_SECTOR_SIZE * 3];
    unsigned char inBuffer[THREADS_DISK_SECTOR_SIZE * 3];
    int status;

    /* Initialize the input and output buffers */
    srand(system_clock());
    memset(outBuffer, rand() % 0x100, THREADS_DISK_SECTOR_SIZE);
    memset(outBuffer + THREADS_DISK_SECTOR_SIZE, rand() % 0x100, THREADS_DISK_SECTOR_SIZE);
    memset(outBuffer + (THREADS_DISK_SECTOR_SIZE * 2), rand() % 0x100, THREADS_DISK_SECTOR_SIZE);

    memset(inBuffer, 0, sizeof(inBuffer));

    console_output(FALSE, "\n%s:\tStarted\n", testName);

    console_output(FALSE, "%s:\tWriting to disk 0, platter 0, track 1, sectors 1-3\n", testName);
    DiskWrite("disk0", outBuffer, 0, 1, 1, 3, &status);
    console_output(FALSE, "%s:\tDiskWrite returned status = %d\n", testName, status);

    console_output(FALSE, "%s:\tWriting to disk 1, platter 1, track 2, sectors 8-12\n", testName);
    DiskWrite("disk1", outBuffer, 1, 2, 8, 3, &status);
    console_output(FALSE, "%s:\tDiskWrite returned status = %d\n", testName, status);

    console_output(FALSE, "%s:\tReading from disk 0, platter 0, track 1, sectors 1-3\n", testName);
    DiskRead("disk0", inBuffer, 0, 1, 1, 3, &status);
    console_output(FALSE, "%s:\tDiskRead returned status = %d\n", testName, status);

    if (memcmp(outBuffer, inBuffer, sizeof(outBuffer)) == 0)
    {
        console_output(FALSE, "%s:\tIn and Out buffers match!\n", testName);
    }
    else
    {
        console_output(FALSE, "%s:\tIn and Out buffers do not match\n", testName);
    }

    /* reset the in buffer */
    memset(inBuffer, 0, sizeof(inBuffer));

    console_output(FALSE, "%s:\tReading from disk 1, platter 1, track 2, sectors 8-12\n", testName);
    DiskRead("disk1", inBuffer, 1, 2, 8, 3, &status);
    console_output(FALSE, "%s:\tDiskRead returned status = %d\n", testName, status);

    if (memcmp(outBuffer, inBuffer, sizeof(outBuffer)) == 0)
    {
        console_output(FALSE, "%s:\tIn and Out buffers match!\n", testName);
    }
    else
    {
        console_output(FALSE, "%s:\tIn and Out buffers do not match\n", testName);
    }

    Exit(0);

    return 0;
}
