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
* DevicesTest06
*
* Purpose:
*   Track-boundary wrap test.  Writes three sectors starting at the last
*   sector of track 4 (sector 15), which forces the driver to wrap to track 5
*   for the remaining two sectors.  Reads the same span back and verifies
*   the full 3-sector payload is intact.
*
* Flow:
*   1. Fill three 512-byte regions of outBuffer with distinct random values.
*   2. DiskWrite("disk0", outBuffer, platter=0, track=4, sector=15, count=3).
*      - Sector 15 is the last sector on a track (0-indexed, 16 sectors/track).
*      - After writing sector 15, the driver must increment to track 5,
*        reset to sector 0, and write the remaining 2 sectors there.
*   3. DiskRead ("disk0", inBuffer,  platter=0, track=4, sector=15, count=3).
*      - Same wrap path on the read side.
*   4. memcmp the full 1536-byte buffers; print match/mismatch.
*   5. Exit.
*
* Expected output:
*   DevicesTest06:  Started
*   DevicesTest06:  Writing data to 3 disk sectors, wrapping to next track
*   DevicesTest06:  DiskWrite returned status = 0
*   DevicesTest06:  Reading data from 3 disk sectors, wrapping to next track
*   DevicesTest06:  DiskRead returned status = 0
*   DevicesTest06:  In and Out buffers match!
*
* Pass criteria:
*   - Both operations return 0 with status == 0.
*   - Buffer comparison passes across the track boundary.
*
* Covers:
*   - Track-wrap logic in DiskTrackIO
*     (currentSector >= THREADS_DISK_SECTOR_COUNT branch)
*   - currentTrack increment and sector reset to 0 mid-request
*   - Re-queuing of a partially-complete request (TListRemoveNode /
*     TListAddNodeInOrder) in the DiskDriver loop
*   - DISK_SEEK issued for the second track after the wrap
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

    console_output(FALSE, "%s:\tWriting data to 3 disk sectors, wrapping to next track\n", testName);
    DiskWrite("disk0", outBuffer, 0, 4, 15, 3, &status);
    console_output(FALSE, "%s:\tDiskWrite returned status = %d\n", testName, status);

    console_output(FALSE, "%s:\tReading data from 3 disk sectors, wrapping to next track\n", testName);
    DiskRead("disk0", inBuffer, 0, 4, 15, 3, &status);
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
