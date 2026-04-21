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

/*********************************************************************************
*
* DevicesTest21
*
* Purpose:
*   Invalid device name error-path test for DiskWrite, DiskRead, and DiskInfo.
*   Verifies that all three disk system calls correctly reject an unrecognised
*   device name and return -1 without blocking or crashing.
*
* Test cases (all use device name "disk9" which does not exist):
*
*   Case 1 -- DiskWrite with invalid device:
*     DiskWrite("disk9", buf, 0, 0, 0, 1, &status)
*     Expected: return value == -1, no block.
*
*   Case 2 -- DiskRead with invalid device:
*     DiskRead("disk9", buf, 0, 0, 0, 1, &status)
*     Expected: return value == -1, no block.
*
*   Case 3 -- DiskInfo with invalid device:
*     DiskInfo("disk9", &ss, &sc, &tc, &pc)
*     Expected: return value == -1, no block.
*
*   Case 4 -- Valid operation after errors (sanity check):
*     DiskWrite("disk0", buf, 0, 5, 0, 1, &status)
*     Expected: return value == 0, status == 0.
*     Confirms the driver is unaffected by the prior error cases.
*
* Flow:
*   1. Attempt DiskWrite to "disk9", print PASSED/FAILED.
*   2. Attempt DiskRead  from "disk9", print PASSED/FAILED.
*   3. Attempt DiskInfo  for  "disk9", print PASSED/FAILED.
*   4. Perform valid DiskWrite to disk0 as a sanity check.
*   5. Exit.
*
* Expected output:
*   DevicesTest21: Started
*   DevicesTest21: DiskWrite invalid device test -- PASSED
*   DevicesTest21: DiskRead  invalid device test -- PASSED
*   DevicesTest21: DiskInfo  invalid device test -- PASSED
*   DevicesTest21: DiskWrite valid device sanity check -- PASSED  (status = 0)
*
* Pass criteria:
*   - Cases 1-3 all return -1 immediately (no blocking, no crash).
*   - Case 4 returns 0 with status == 0.
*
* Implementation note -- two bugs to avoid in sys_diskinfo:
*
*   Bug 1: device_handle() returns uint32_t (per THREADSLib.h). It returns 0
*   for an unknown device and 1 or 2 for valid disks. The guard must be:
*
*       if (deviceHandle <= 0)   // CORRECT: catches 0 = not found
*
*   NOT:
*
*       if (deviceHandle < 0)    // WRONG: 0 slips through, unit becomes -1,
*                                //        function reads diskInfo[-1] and
*                                //        returns 0 instead of -1.
*
*   The same latent bug exists in sys_disk_io but the secondary parameter-range
*   check (track >= diskInfo[-1].tracks) accidentally catches it there.
*   Both functions should use <= 0 for correctness.
*
*   Bug 2: Calling device_handle() twice in sys_diskinfo. The first call may
*   initialise internal state for the unknown device such that the second call
*   returns a valid-looking handle. Call it exactly once, same as sys_disk_io.
*
* Covers:
*   - device_handle() returning 0 for unknown device name
*   - Early-return (-1) path in sys_disk_io before request is queued
*   - Early-return (-1) path in sys_diskinfo (requires <= 0 and single call)
*   - Driver remains operational after a series of invalid requests
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    unsigned char buf[THREADS_DISK_SECTOR_SIZE];
    int sectorSize, sectorCount, trackCount, platterCount;
    int status;
    int result;

    srand(system_clock());
    memset(buf, rand() % 0x100, sizeof(buf));

    console_output(FALSE, "\n%s: Started\n", testName);

    /* Case 1: DiskWrite to non-existent device */
    result = DiskWrite("disk9", buf, 0, 0, 0, 1, &status);
    if (result == -1)
        console_output(FALSE, "%s: DiskWrite invalid device test -- PASSED\n", testName);
    else
        console_output(FALSE, "%s: DiskWrite invalid device test -- FAILED (returned %d)\n", testName, result);

    /* Case 2: DiskRead from non-existent device */
    result = DiskRead("disk9", buf, 0, 0, 0, 1, &status);
    if (result == -1)
        console_output(FALSE, "%s: DiskRead  invalid device test -- PASSED\n", testName);
    else
        console_output(FALSE, "%s: DiskRead  invalid device test -- FAILED (returned %d)\n", testName, result);

    /* Case 3: DiskInfo for non-existent device */
    result = DiskInfo("disk9", &sectorSize, &sectorCount, &trackCount, &platterCount);
    if (result == -1)
        console_output(FALSE, "%s: DiskInfo  invalid device test -- PASSED\n", testName);
    else
        console_output(FALSE, "%s: DiskInfo  invalid device test -- FAILED (returned %d)\n", testName, result);

    /* Case 4: valid write to disk0 -- confirm driver is still functional */
    result = DiskWrite("disk0", buf, 0, 5, 0, 1, &status);
    if (result == 0 && status == 0)
        console_output(FALSE, "%s: DiskWrite valid device sanity check -- PASSED  (status = %d)\n", testName, status);
    else
        console_output(FALSE, "%s: DiskWrite valid device sanity check -- FAILED (result %d, status %d)\n", testName, result, status);

    Exit(0);

    return 0;
}