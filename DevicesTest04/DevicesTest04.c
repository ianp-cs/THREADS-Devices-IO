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
* DevicesTest04
*
* Purpose:
*   Multi-sector write/read round-trip on a single track of disk0.  Writes
*   three contiguous sectors on the same track and reads them back, verifying
*   that all three sector payloads are preserved correctly.  Also calls
*   DiskInfo to confirm geometry before performing I/O.
*
* Flow:
*   1. Fill three 512-byte regions of outBuffer with distinct random byte
*      values (one random value per sector).
*   2. Call DiskInfo("disk0") and print geometry.
*   3. DiskWrite("disk0", outBuffer, platter=0, track=6, sector=3, count=3)
*      -- writes sectors 3, 4, and 5 of track 6.
*   4. DiskRead ("disk0", inBuffer,  platter=0, track=6, sector=3, count=3).
*   5. memcmp the full 1536-byte buffers; print match/mismatch.
*   6. Exit.
*
* Expected output:
*   DevicesTest04:  Started
*   DevicesTest04:  Calling DiskInfo for disk0
*   DevicesTest04:  DiskInfo returned sectorSize 512, ...
*   DevicesTest04:  Writing to disk 0, platter 0, track 6, sectors 3-6
*   DevicesTest04:  DiskWrite returned status = 0
*   DevicesTest04:  Reading from disk 0, platter 0, track 6, sectors 3-6
*   DevicesTest04:  DiskRead returned status = 0
*   DevicesTest04:  In and Out buffers match!
*
* Pass criteria:
*   - DiskWrite and DiskRead both return 0 with status == 0.
*   - Full 3-sector buffer is identical after the round-trip.
*
* Covers:
*   - Multi-sector I/O within a single track (DiskTrackIO loop)
*   - sectorCount > 1 path in DiskTrackIO
*   - Correct buffer offset advancement (sectorsProcessed * SECTOR_SIZE)
*   - No spurious track wrap (sectors 3-5 stay within track 6's 16 sectors)
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    int sectorSize, platterCount, sectorCount, trackCount;
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

    /* Get the disk information for disk 0 */
    console_output(FALSE, "%s:\tCalling DiskInfo for disk0\n", testName);
    if (DiskInfo("disk0", &sectorSize, &sectorCount, &trackCount, &platterCount) == 0)
    {
        console_output(FALSE, "%s:\tDiskInfo returned sectorSize %d, sectorCount %d, trackCount %d, platterCount %d\n", testName, sectorSize, sectorCount, trackCount, platterCount);
    }
    else
    {
        console_output(FALSE, "%s:\tDiskInfo failed\n", testName);
    }

    console_output(FALSE, "%s:\tWriting to disk 0, platter 0, track 6, sectors 3-6\n", testName);
    DiskWrite("disk0", outBuffer, 0, 6, 3, 3, &status);
    console_output(FALSE, "%s:\tDiskWrite returned status = %d\n", testName, status);

    console_output(FALSE, "%s:\tReading from disk 0, platter 0, track 6, sectors 3-6\n", testName);
    DiskRead("disk0", inBuffer, 0, 6, 3, 3, &status);
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
