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
* DevicesTest03
*
* Purpose:
*   Baseline single-sector write/read round-trip test on disk0.  Writes one
*   randomly-filled sector to a known location and reads it back, verifying
*   that the data is identical.  Also exercises DiskInfo as a precondition.
*
* Flow:
*   1. Fill a 512-byte outBuffer with a random byte value (rand() % 0x100).
*   2. Call DiskInfo("disk0") and print the geometry.
*   3. DiskWrite("disk0", outBuffer, platter=0, track=5, sector=0, count=1).
*   4. DiskRead ("disk0", inBuffer,  platter=0, track=5, sector=0, count=1).
*   5. memcmp outBuffer vs inBuffer; print match/mismatch result.
*   6. Exit.
*
* Expected output:
*   DevicesTest03:  Started
*   DevicesTest03:  Calling DiskInfo for disk0
*   DevicesTest03:  DiskInfo returned sectorSize 512, sectorCount 16,
*                   trackCount <N>, platterCount <P>
*   DevicesTest03:  Writing to disk 0, platter 0, track 5, sector 0
*   DevicesTest03:  DiskWrite returned status = 0
*   DevicesTest03:  Reading from disk 0, platter 0, track 5, sector 0
*   DevicesTest03:  DiskRead returned status = 0
*   DevicesTest03:  In and Out buffers match!
*
* Pass criteria:
*   - Both DiskWrite and DiskRead return 0 with status == 0.
*   - outBuffer and inBuffer are identical after the read.
*
* Covers:
*   - SYS_DISKWRITE / SYS_DISKREAD single-sector happy path
*   - sys_disk_io parameter validation (valid track, sector, platter)
*   - DiskDriver DISK_SEEK + single-sector DiskTrackIO
*   - Process blocking on waitSem and unblocking on I/O completion
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    int sectorSize, platterCount, sectorCount, trackCount;
    unsigned char outBuffer[THREADS_DISK_SECTOR_SIZE];
    unsigned char inBuffer[THREADS_DISK_SECTOR_SIZE];
    int status;

    srand(system_clock());
    memset(outBuffer, rand() % 0x100, sizeof(outBuffer));

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

    console_output(FALSE, "%s:\tWriting to disk 0, platter 0, track 5, sector 0\n", testName);
    DiskWrite("disk0", outBuffer, 0, 5, 0, 1, &status);
    console_output(FALSE, "%s:\tDiskWrite returned status = %d\n", testName, status);

    console_output(FALSE, "%s:\tReading from disk 0, platter 0, track 5, sector 0\n", testName);
    DiskRead("disk0", inBuffer, 0, 5, 0, 1, &status);
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
