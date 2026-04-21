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
* DevicesTest02
*
* Purpose:
*   Baseline test for the DiskInfo system call.  A single process queries
*   the geometry of both simulated disks and prints the results.  No I/O
*   is performed; this is purely an information retrieval test.
*
* Flow:
*   1. Call DiskInfo("disk0", ...) and print sectorSize, sectorCount,
*      trackCount, platterCount.
*   2. Call DiskInfo("disk1", ...) and print the same fields.
*   3. Exit.
*
* Expected output:
*   DevicesTest02: Started
*   DevicesTest02:  Calling DiskInfo for disk0
*   DevicesTest02:  DiskInfo returned sectorSize 512, sectorCount 16,
*                   trackCount <N>, platterCount <P>
*   DevicesTest02:  Calling DiskInfo for disk1
*   DevicesTest02:  DiskInfo returned sectorSize 512, sectorCount 16,
*                   trackCount <N>, platterCount <P>
*
*   (N and P are determined by the disk image files present at runtime.)
*
* Pass criteria:
*   - Both DiskInfo calls return 0 (success).
*   - sectorSize == THREADS_DISK_SECTOR_SIZE (512).
*   - sectorCount == THREADS_DISK_SECTOR_COUNT (16).
*   - trackCount and platterCount reflect the actual disk image geometry
*     and are non-zero.
*
* Covers:
*   - SYS_DISKINFO / sys_diskinfo dispatch path
*   - DiskDriver initialization (diskInfo[] populated from DISK_INFO command)
*   - device_handle() name-to-unit mapping
*   - Both disk units (disk0 and disk1)
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    int sectorSize, platterCount, sectorCount, trackCount;


    console_output(FALSE, "\n%s: Started\n", testName);

    /* Get the disk information for disk 0 */
    console_output(FALSE, "%s:  Calling DiskInfo for disk0\n", testName);
    if (DiskInfo("disk0", &sectorSize, &sectorCount, &trackCount, &platterCount) == 0)
    {
        console_output(FALSE, "%s:  DiskInfo returned sectorSize %d, sectorCount %d, trackCount %d, platterCount %d\n", testName, sectorSize, sectorCount, trackCount, platterCount);
    }
    else
    {
        console_output(FALSE, "%s:  DiskInfo failed\n", testName);
    }

    /* Get the disk information for disk 1 */
    console_output(FALSE, "%s:  Calling DiskInfo for disk1\n", testName);
    if (DiskInfo("disk1", &sectorSize, &sectorCount, &trackCount, &platterCount) == 0)
    {
        console_output(FALSE, "%s:  DiskInfo returned sectorSize %d, sectorCount %d, trackCount %d, platterCount %d\n", testName, sectorSize, sectorCount, trackCount, platterCount);
    }
    else
    {
        console_output(FALSE, "%s:  DiskInfo failed\n", testName);
    }

    Exit(0);

    return 0;
}
