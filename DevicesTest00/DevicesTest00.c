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
* DevicesTest00
*
* Purpose:
*   Baseline test for the SleepSeconds system call.  A single process
*   records the system clock before and after sleeping for 10 seconds,
*   then prints the elapsed time in milliseconds.
*
* Flow:
*   1. Record start time via GetTimeofDay.
*   2. Call SleepSeconds(10).
*   3. Record end time via GetTimeofDay.
*   4. Print elapsed milliseconds and exit.
*
* Expected output:
*   DevicesTest00: Started
*   DevicesTest00:  Going to sleep for 10 seconds
*   DevicesTest00: Slept <~10000>          (value should be close to 10000 ms)
*
* Pass criteria:
*   Elapsed time is approximately 10 000 ms.  Any significant over- or
*   under-shoot indicates a bug in the clock driver's wake-up logic or
*   in the wakeTime calculation inside sleep_real().
*
* Covers:
*   - SleepSeconds / sleep_real basic correctness
*   - Clock driver tick processing and sleeping-process wake-up
*   - GetTimeofDay accuracy
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    int begin, end;
    int timeInMilliseconds;

    /* Just output a message and exit. */
    console_output(FALSE, "\n%s: Started\n", testName);

    console_output(FALSE, "%s:  Going to sleep for 10 seconds\n", testName);

    GetTimeofDay(&begin);
    SleepSeconds(10);
    GetTimeofDay(&end);
    timeInMilliseconds = (end - begin) / 1000;

    console_output(FALSE, "%s: Slept %d\n", testName, timeInMilliseconds);

    Exit(0);

    return 0;
}
