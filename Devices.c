#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <THREADSLib.h>
#include <Messaging.h>
#include <Scheduler.h>
#include <TList.h>
#include <libuser.h>
#include <SystemCalls.h>
#include <Devices.h>

/* Set the disk arm scheduling algorithm.
 * See Devices.h for available constants (DISK_ARM_ALG_FCFS, DISK_ARM_ALG_SSTF, etc.).
 * You must implement FCFS and SSTF. Change this value to test each algorithm.
 * Submissions will be assessed with DISK_ARM_ALG_FCFS and DISK_ARM_ALG_SSTF. */
#define DISK_ARM_ALG   DISK_ARM_ALG_FCFS

static int ClockDriver(char*);
static int DiskDriver(char*);

typedef struct devices_proc
{
    struct devices_proc* pNext;
    struct devices_proc* pPrev;
    int pid;
} DevicesProcess;

typedef struct
{
    int tracks;
    int platters;
    char deviceName[THREADS_MAX_DEVICE_NAME];
} DiskInformation;

static DevicesProcess devicesProcs[MAXPROC];
static DiskInformation diskInfo[THREADS_MAX_DISKS];

static inline void checkKernelMode(const char* functionName);
extern int DevicesEntryPoint(char*);

int SystemCallsEntryPoint(char* arg)
{
    char    buf[25];
    char    name[128];
    int     i;
    int     clockPID = 0;
    int     diskPids[THREADS_MAX_DISKS];
    int     status;

    checkKernelMode(__func__);

    /* Assign system call handlers */

    /* Initialize the process table */
    for (int i = 0; i < MAXPROC; ++i)
    {
    }

    /* Create and start the clock driver */
    clockPID = k_spawn("Clock driver", ClockDriver, NULL, THREADS_MIN_STACK_SIZE, HIGHEST_PRIORITY);
    if (clockPID < 0)
    {
        console_output(TRUE, "start3(): Can't create clock driver\n");
        stop(1);
    }

    /* Create the disk drivers */
    for (i = 0; i < THREADS_MAX_DISKS; i++)
    {
        sprintf(buf, "%d", i);
        sprintf(name, "DiskDriver%d", i);
        diskPids[i] = k_spawn(name, DiskDriver, buf, THREADS_MIN_STACK_SIZE * 4, HIGHEST_PRIORITY);
        if (diskPids[i] < 0)
        {
            console_output(TRUE, "start3(): Can't create disk driver %d\n", i);
            stop(1);
        }
    }

    /* Create first user-level process and wait for it to finish */
    sys_spawn("DevicesEntryPoint", DevicesEntryPoint, NULL, 8 * THREADS_MIN_STACK_SIZE, 3);
    sys_wait(&status);

    return 0;
}


static int ClockDriver(char* arg)
{
    int result;
    int status;

    set_psr(get_psr() | PSR_INTERRUPTS);

    while (!signaled())
    {
        result = wait_device("clock", &status);
        if (result != 0)
        {
            return 0;
        }

        /* Compute the current time and wake up any processes whose time has come */
    }
    return 0;
}


static int DiskDriver(char* arg)
{
    int unit = atoi(arg);
    int currentTrack = 0;
    device_control_block_t devRequest;

    set_psr(get_psr() | PSR_INTERRUPTS);

    /* Read the disk info */

    /* Operating loop */
    while (!signaled())
    {
    }
    return 0;
}


struct psr_bits {
    unsigned int cur_int_enable : 1;
    unsigned int cur_mode : 1;
    unsigned int prev_int_enable : 1;
    unsigned int prev_mode : 1;
    unsigned int unused : 28;
};

union psr_values {
    struct psr_bits bits;
    unsigned int integer_part;
};

/*****************************************************************************
   Name - checkKernelMode
   Purpose - Checks the PSR for kernel mode and stops if in user mode
   Parameters -
   Returns -
   Side Effects - Will stop if not in kernel mode
****************************************************************************/
static inline void checkKernelMode(const char* functionName)
{
    union psr_values psrValue;

    psrValue.integer_part = get_psr();
    if (psrValue.bits.cur_mode == 0)
    {
        console_output(FALSE, "Kernel mode expected, but function called in user mode.\n");
        stop(1);
    }
}