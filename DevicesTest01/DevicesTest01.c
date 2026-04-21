#define _CRT_SECURE_NO_WARNINGS
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
* DevicesTest01
*
* Purpose:
*   Stress test for concurrent sleeping processes.  Ten child processes are
*   spawned, each sleeping a different duration (1, 3, 5, 7, 9, 11, 13, 15,
*   17, and 19 seconds).  The parent waits for all ten to complete.
*
* Flow:
*   1. Spawn child 1  with sleepTime = 1 second.
*   2. Spawn child 2  with sleepTime = 3 seconds.
*      ... (increment by 2 each iteration)
*   3. Spawn child 10 with sleepTime = 19 seconds.
*   4. Parent calls Wait() ten times, printing the result of each.
*   5. Parent exits.
*
* Each child (DevicesTestDriver):
*   - Receives its sleep duration via the encoded name string.
*   - Calls SleepSeconds(n), prints elapsed milliseconds, then exits.
*
* Expected output (interleaved by actual wake order, earliest first):
*   DevicesTest01-Child1:  started
*   ...
*   DevicesTest01-Child1:  Slept ~1000 ms
*   DevicesTest01-Child2:  Slept ~3000 ms
*   ...
*   DevicesTest01: Wait returned 0, pid:<x>, status 0   (x10)
*
* Pass criteria:
*   - Children wake in ascending sleep-duration order.
*   - Each reported elapsed time is approximately equal to its assigned
*     sleep duration (within one clock tick, ~100 ms).
*   - All ten Wait() calls return successfully.
*
* Covers:
*   - Multiple concurrent sleeping processes
*   - sleepingProcesses TList ordering (OrderByWakeTime)
*   - Clock driver correctly waking the earliest-deadline process first
*   - Sleep durations that span several clock interrupts
*
*********************************************************************************/
int DevicesEntryPoint(void* pArgs)
{
    char* testName = GetTestName(__FILE__);
    char nameBuffer[512];
    int childId = 0;
    int kidPid;
    char* optionSeparator;
    int messageCount = 0;
    int sleepTime = 1;
    char childNames[MAXPROC][256];
    int i, id, result;


    /* Just output a message and exit. */
    console_output(FALSE, "\n%s: started\n", testName);

    for (i = 0; i < 10; i++) 
    {
        /* set sleep time */
        optionSeparator = CreateDevicesTestArgs(nameBuffer, sizeof(nameBuffer), testName, ++childId, sleepTime, 0, 0, 0);
        Spawn(nameBuffer, DevicesTestDriver, nameBuffer, THREADS_MIN_STACK_SIZE, 3, &kidPid);
        optionSeparator[0] = '\0';
        strncpy(childNames[kidPid], nameBuffer, 256);

        sleepTime += 2;
    }


    for (i = 0; i < 10; i++) 
    {
        console_output(FALSE, "%s: Waiting on Child\n", testName);
        result = Wait(&kidPid, &id);
        console_output(FALSE, "%s: Wait returned %d, pid:%d, status %d\n", testName, result, kidPid, id);
    }

    Exit(0);

    return 0;
}
