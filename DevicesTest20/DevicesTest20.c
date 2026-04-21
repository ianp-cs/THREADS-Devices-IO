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
* DevicesTest20
*
* Purpose:
*   SleepSeconds error-path test.  Verifies that the clock driver correctly
*   rejects invalid sleep durations (0 and negative values) without blocking
*   the calling process, and that the system remains fully functional for a
*   valid sleep call afterward.
*
* Test cases:
*
*   Case 1 -- Zero seconds:
*     SleepSeconds(0)
*     Expected: return value == -1, process does not block.
*     Spec: sleep_real() returns -1 for seconds <= 0.
*
*   Case 2 -- Negative seconds:
*     SleepSeconds(-1)
*     Expected: return value == -1, process does not block.
*
*   Case 3 -- Large negative:
*     SleepSeconds(-100)
*     Expected: return value == -1, process does not block.
*
*   Case 4 -- Valid sleep (sanity check):
*     SleepSeconds(2)
*     Expected: return value == 0, process sleeps approximately 2 seconds.
*     This confirms the clock driver is still operational after the error cases.
*
* Flow:
*   1. Call SleepSeconds(0),    check return == -1, print PASSED/FAILED.
*   2. Call SleepSeconds(-1),   check return == -1, print PASSED/FAILED.
*   3. Call SleepSeconds(-100), check return == -1, print PASSED/FAILED.
*   4. Record time, call SleepSeconds(2), record time.
*      Print elapsed ms and confirm approximately 2000 ms.
*   5. Exit.
*
* Expected output:
*   DevicesTest20: Started
*   DevicesTest20: SleepSeconds(0)    returned -1 -- PASSED
*   DevicesTest20: SleepSeconds(-1)   returned -1 -- PASSED
*   DevicesTest20: SleepSeconds(-100) returned -1 -- PASSED
*   DevicesTest20: SleepSeconds(2) returned 0, slept ~2000 ms -- PASSED
*
* Pass criteria:
*   - All three invalid calls return -1 immediately (no blocking).
*   - The valid call returns 0 and the elapsed time is approximately 2000 ms.
*
* Covers:
*   - sleep_real() boundary check (seconds <= 0)
*   - Process is NOT added to sleepingProcesses for invalid input
*   - Clock driver remains functional after rejected requests
*   - Return value propagation through sysCall4 -> SleepSeconds wrapper
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    int result;
    int begin, end;
    int timeInMilliseconds;

    console_output(FALSE, "\n%s: Started\n", testName);

    /* Case 1: zero seconds */
    result = SleepSeconds(0);
    if (result == -1)
        console_output(FALSE, "%s: SleepSeconds(0)    returned -1 -- PASSED\n", testName);
    else
        console_output(FALSE, "%s: SleepSeconds(0)    returned %d  -- FAILED (expected -1)\n", testName, result);

    /* Case 2: negative one */
    result = SleepSeconds(-1);
    if (result == -1)
        console_output(FALSE, "%s: SleepSeconds(-1)   returned -1 -- PASSED\n", testName);
    else
        console_output(FALSE, "%s: SleepSeconds(-1)   returned %d  -- FAILED (expected -1)\n", testName, result);

    /* Case 3: large negative */
    result = SleepSeconds(-100);
    if (result == -1)
        console_output(FALSE, "%s: SleepSeconds(-100) returned -1 -- PASSED\n", testName);
    else
        console_output(FALSE, "%s: SleepSeconds(-100) returned %d  -- FAILED (expected -1)\n", testName, result);

    /* Case 4: valid sleep -- confirm system still works after error cases */
    GetTimeofDay(&begin);
    result = SleepSeconds(2);
    GetTimeofDay(&end);
    timeInMilliseconds = (end - begin) / 1000;

    if (result == 0 && timeInMilliseconds >= 1800 && timeInMilliseconds <= 2500)
        console_output(FALSE, "%s: SleepSeconds(2) returned 0, slept %d ms -- PASSED\n", testName, timeInMilliseconds);
    else
        console_output(FALSE, "%s: SleepSeconds(2) returned %d, slept %d ms -- FAILED\n", testName, result, timeInMilliseconds);

    Exit(0);

    return 0;
}
