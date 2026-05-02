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

/* STRUCTURES */
typedef struct devices_proc
{
    struct devices_proc* pNextSleeper;
    struct devices_proc* pPrevSleeper;
    int pid;
    int semID;
    int wakeupTime;
    bool kill;
} DevicesProcess;

typedef struct
{
    int tracks;
    int platters;
    char deviceName[THREADS_MAX_DEVICE_NAME];
} DiskInformation;

enum reqType { WRITE, READ, INFO };

typedef struct disk_request
{
    struct disk_request* nextReq;
    struct disk_request* prevReq;
    int pid;
    enum reqType type; 
    char* deviceName;
    int sectorSize;
    int sectorCount; 
    int trackCount; 
    int platterCount;
    void* dataBuffer; 
    int platter; 
    int track; 
    int firstSector; 
    int sectors;
    int result;
    int status;
} DiskRequest;

/* MUTEXES */
static int sleep_mutex;                   // Mutex for sleeping processes queue
static int disk_mutex[THREADS_MAX_DISKS]; // Mutex for disk requests queues

/* FUNCTIONS */
static int ClockDriver(char*);
static int DiskDriver(char*);
int sys_disk_info(DiskRequest* req);
int sys_disk_read(DiskRequest* req, int diskNum, int* currentTrack, int* currentSector);
int sys_disk_write(DiskRequest* req, int diskNum, int* currentTrack, int* currentSector);
int sys_sleep(int* seconds);
static void sysCall4(system_call_arguments_t* args);
static void pushSleepQueue(DevicesProcess* target);
static DevicesProcess* popSleepQueue();
static void pushDiskReq(int index, DiskRequest* head, DiskRequest* target);
static void pushDiskReqHelperFCFS(int index, DiskRequest* head, DiskRequest* target);
static DiskRequest* popDiskReq(int index, DiskRequest* head);
static DiskRequest* popDiskReqHelperFCFS(int index, DiskRequest* head);
static void initDiskInfo(int diskNum);
static int moveDiskArm(char* deviceName, int target);
static inline void checkKernelMode(const char* functionName);

extern int DevicesEntryPoint(char*);

/* CONSTANTS */
static DevicesProcess devicesProcs[MAXPROC];
static DevicesProcess* sleepQueueHead;
static DiskRequest* diskRequestHead[THREADS_MAX_DISKS];
static DiskInformation diskInfo[THREADS_MAX_DISKS];


int SystemCallsEntryPoint(char* arg)
{
    char    buf[25];
    char    name[128];
    int     i;
    int     clockPID = 0;
    int     diskPids[THREADS_MAX_DISKS];
    int     status;
    int     returnPid;

    checkKernelMode(__func__);

    /* Assign system call handlers */
    systemCallVector[SYS_SLEEP] = sysCall4;
    systemCallVector[SYS_DISKREAD] = sysCall4;
    systemCallVector[SYS_DISKWRITE] = sysCall4;
    systemCallVector[SYS_DISKINFO] = sysCall4;

    /* Initialize the process table */
    for (int i = 0; i < MAXPROC; ++i)
    {
        devicesProcs[i].pid = -1;
        devicesProcs[i].semID = -1;
        devicesProcs[i].wakeupTime = -1;
        devicesProcs[i].kill = false;
        devicesProcs[i].pNextSleeper = NULL;
        devicesProcs[i].pPrevSleeper = NULL;
    }
    
    // Create Mutex Lock
    sleep_mutex = k_semcreate(1);

    /* Create and start the clock driver */
    clockPID = k_spawn("Clock driver", ClockDriver, NULL, THREADS_MIN_STACK_SIZE, HIGHEST_PRIORITY);
    if (clockPID < 0)
    {
        console_output(TRUE, "start3(): Can't create clock driver\n");
        stop(1);
    }

    devicesProcs[clockPID % MAXPROC].pid = clockPID;

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
        
        devicesProcs[diskPids[i] % MAXPROC].pid = diskPids[i];
        devicesProcs[diskPids[i] % MAXPROC].semID = k_semcreate(0);
        disk_mutex[i] = k_semcreate(1);
    }

    /* Create first user-level process and wait for it to finish */
    sys_spawn("DevicesEntryPoint", DevicesEntryPoint, NULL, 8 * THREADS_MIN_STACK_SIZE, 3);

    // End running processes
    returnPid = k_wait(&status); // Disk driver 0,      pid 6
    k_semfree(devicesProcs[diskPids[0]].semID);
    returnPid = k_wait(&status); // Disk driver 1,      pid 7
    k_semfree(devicesProcs[diskPids[1]].semID);

    returnPid = k_wait(&status); // DevicesEntryPoint,  pid 8

    k_kill(clockPID, SIG_TERM);
    returnPid = k_wait(&status); // Clock driver,       pid 5
    k_semfree(devicesProcs[clockPID % MAXPROC].semID);

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
        int currTime = system_clock();

        while (sleepQueueHead != NULL && currTime > sleepQueueHead->wakeupTime)
        {
            DevicesProcess* currProc = popSleepQueue();
            k_semv(currProc->semID);
        }
    }
    return 0;
}


static int DiskDriver(char* arg)
{
    int pid = k_getpid();
    int diskNum = atoi(arg);

    set_psr(get_psr() | PSR_INTERRUPTS);

    /* Read the disk info */
    initDiskInfo(diskNum);

    /* Initialize disk arm position */
    int currentTrack = moveDiskArm(diskInfo[diskNum].deviceName, 0);
    int currentSector = 0;

    /* Operating loop */
    
    while (!signaled())
    {
        k_semp(devicesProcs[pid % MAXPROC].semID); // Block until summoned

        if (DISK_ARM_ALG == DISK_ARM_ALG_FCFS)
        {
            DiskRequest* req = popDiskReq(diskNum, diskRequestHead[diskNum]);

            switch (req->type)
            {
                case INFO:
                    req->result = sys_disk_info(req);
                    break;

                case READ:
                    req->result = sys_disk_read(req, diskNum, &currentTrack, &currentSector);
                    break;

                case WRITE:
                    req->result = sys_disk_write(req, diskNum, &currentTrack, &currentSector);
                    break;
            }

            k_semv(devicesProcs[req->pid % MAXPROC].semID); // Unblock user process that used disk
        }
    }
    
    return 0;
}


static void initDiskInfo(int diskNum)
{
    device_control_block_t devRequest;
    int result;
    int status;

    sprintf(diskInfo[diskNum].deviceName, "disk%d", diskNum);

    devRequest.command = DISK_INFO;
    result = device_control(diskInfo[diskNum].deviceName, devRequest);

    if (result != 0)
    {
        console_output(false, "initDiskInfo(): Device control failed! Closing program...\n");
        stop(1);
    }

    wait_device(diskInfo[diskNum].deviceName, &status);
    diskInfo[diskNum].tracks = status & 0xffff;
    diskInfo[diskNum].platters = status >> 16;
}


static int moveDiskArm(char* deviceName, int target)
{
    device_control_block_t devRequest;
    int result = -1;
    int status = -1;

    devRequest.command = DISK_SEEK;
    devRequest.control1 = target;
    devRequest.control2 = 0;

    result = device_control(deviceName, devRequest);

    if (result != 0)
    {
        console_output(false, "moveDiskArm(): Device control failed! Couldn't move arm...\n");
        stop(1);
    }

    wait_device(deviceName, &status);

    return target;
}


int sys_disk_info(DiskRequest* req)
{
    int index = k_getpid() - 6; // - 6 because disk pids start at 6

    req->sectorSize = THREADS_DISK_SECTOR_SIZE;
    req->sectorCount = THREADS_DISK_SECTOR_COUNT;
    req->trackCount = diskInfo[index].tracks;
    req->platterCount = diskInfo[index].platters;

    return 0;
}


int sys_disk_read(DiskRequest* req, int diskNum, int* currentTrack, int* currentSector)
{
    device_control_block_t devRequest;
    int sectorsProcessed = 0;

    // Move to initial track and sector
    *currentSector = req->firstSector;

    if (*currentTrack != req->track)
    {
        *currentTrack = moveDiskArm(req->deviceName, req->track);
    }

    for (int i = 0; i < req->sectors; i++)
    {
        // Operate on sector
        devRequest.command = DISK_READ;
        devRequest.control1 = (uint8_t)req->platter;
        devRequest.control2 = (uint8_t)*currentSector;
        devRequest.input_data = (unsigned char*)req->dataBuffer + sectorsProcessed * THREADS_DISK_SECTOR_SIZE;

        req->result = device_control(req->deviceName, devRequest);

        wait_device(req->deviceName, &req->status);

        // Move to next sector
        (*currentSector)++;
        sectorsProcessed++;

        // Move disk arm to new track if reached the end of sectors for current track
        if (*currentSector == THREADS_DISK_SECTOR_COUNT)
        {
            *currentSector = 0;
            int nextTrack = (*currentTrack + 1) % diskInfo[diskNum].tracks;
            *currentTrack = moveDiskArm(req->deviceName, nextTrack);
        }
    }
}


int sys_disk_write(DiskRequest* req, int diskNum, int* currentTrack, int* currentSector)
{
    device_control_block_t devRequest;
    int sectorsProcessed = 0;

    // Move to initial track and sector
    *currentSector = req->firstSector;
    
    if (*currentTrack != req->track)
    {
        *currentTrack = moveDiskArm(req->deviceName, req->track);
    }

    for (int i = 0; i < req->sectors; i++)
    {
        // Operate on sector
        devRequest.command = DISK_WRITE;
        devRequest.control1 = (uint8_t)req->platter;
        devRequest.control2 = (uint8_t)*currentSector;
        devRequest.output_data = (unsigned char*)req->dataBuffer + sectorsProcessed * THREADS_DISK_SECTOR_SIZE;

        req->result = device_control(req->deviceName, devRequest);

        wait_device(req->deviceName, &req->status);

        // Move to next sector
        (*currentSector)++;
        sectorsProcessed++;

        // Move disk arm to new track if reached the end of sectors for current track
        if (*currentSector == THREADS_DISK_SECTOR_COUNT)
        {
            *currentSector = 0;
            int nextTrack = (*currentTrack + 1) % diskInfo[diskNum].tracks;
            *currentTrack = moveDiskArm(req->deviceName, nextTrack);
        }
    }

    return 0;
}


int sys_sleep(int* seconds)
{
    int pid = k_getpid();

    if (*seconds < 0)
    {
        console_output(FALSE, "%s - Invalid number of seconds.\n", "sys_sleep");
        return -1;
    }

    devicesProcs[pid % MAXPROC].wakeupTime = ((*seconds) * 1000000) + system_clock();
    pushSleepQueue(&devicesProcs[pid % MAXPROC]);
    k_semp(devicesProcs[pid % MAXPROC].semID);

    return 0;
}


static void sysCall4(system_call_arguments_t* args)
{
    DiskRequest req;
    req.nextReq = NULL;
    req.prevReq = NULL;

    // Add to device process list
    int pid = k_getpid();

    devicesProcs[pid % MAXPROC].pid = pid;
    devicesProcs[pid % MAXPROC].semID = k_semcreate(0);

    switch (args->call_id)
    {
        case SYS_SLEEP:
            if (args->arguments[0] < 0)
            {
                console_output(false, "SYS_SLEEP(): Invalid number of seconds. Must be at least 0.\n");
                return -1;
            }

            args->arguments[3] = (intptr_t)sys_sleep(&args->arguments[0]);
            break;

        case SYS_DISKREAD:
            req.deviceName = (char*)args->arguments[0];

            req.pid = pid;
            req.type = READ;
            req.dataBuffer = args->arguments[1];
            req.platter = args->arguments[2];
            req.track = args->arguments[3];
            req.firstSector = args->arguments[4];
            req.sectors = args->arguments[5];

            for (int i = 0; i < THREADS_MAX_DISKS; i++)
            {
                if (strcmp(req.deviceName, diskInfo[i].deviceName) == 0)
                {
                    pushDiskReq(i, diskRequestHead[i], &req);
                    k_semv(devicesProcs[(i + 6) % MAXPROC].semID); // i + 6 because disk pids start at 6
                    k_semp(devicesProcs[pid % MAXPROC].semID); // Sleep until disk is finished

                    args->arguments[3] = (intptr_t)req.result;
                    args->arguments[0] = (intptr_t)req.status;
                    args->arguments[1] = (intptr_t)req.dataBuffer;

                    return;
                }
            }

            console_output(false, "SYS_DISKREAD(): Invalid device name.\n");
            return -1;

        case SYS_DISKWRITE:
            req.deviceName = (char*)args->arguments[0];

            req.pid = pid;
            req.type = WRITE;
            req.dataBuffer = args->arguments[1];
            req.platter = args->arguments[2];
            req.track = args->arguments[3];
            req.firstSector = args->arguments[4];
            req.sectors = args->arguments[5];

            for (int i = 0; i < THREADS_MAX_DISKS; i++)
            {
                if (strcmp(req.deviceName, diskInfo[i].deviceName) == 0)
                {
                    pushDiskReq(i, diskRequestHead[i], &req);
                    k_semv(devicesProcs[(i + 6) % MAXPROC].semID); // i + 6 because disk pids start at 6
                    k_semp(devicesProcs[pid % MAXPROC].semID); // Sleep until disk is finished

                    args->arguments[3] = (intptr_t)req.result;
                    args->arguments[0] = (intptr_t)req.status;

                    return;
                }
            }

            console_output(false, "SYS_DISKWRITE(): Invalid device name.\n");
            return -1;

        case SYS_DISKINFO:
            req.deviceName = (char*)args->arguments[0];

            req.pid = pid;
            req.type = INFO;

            for (int i = 0; i < THREADS_MAX_DISKS; i++)
            {
                if (strcmp(req.deviceName, diskInfo[i].deviceName) == 0)
                {
                    pushDiskReq(i, diskRequestHead[i], &req);
                    k_semv(devicesProcs[(i + 6) % MAXPROC].semID); // i + 6 because disk pids start at 6
                    k_semp(devicesProcs[pid % MAXPROC].semID); // Sleep until disk is finished

                    args->arguments[3] = (intptr_t)req.result;
                    args->arguments[0] = (intptr_t)req.sectorSize;
                    args->arguments[1] = (intptr_t)req.sectorCount;
                    args->arguments[2] = (intptr_t)req.trackCount;
                    args->arguments[4] = (intptr_t)req.platterCount;

                    return;
                }
            }

            console_output(false, "SYS_DISKINFO(): Invalid device name.\n");
            return -1;
    }
}


static void pushSleepQueue(DevicesProcess* target)
{
    // Set mutex
    k_semp(sleep_mutex);
    
    // Target has less wait time than the first proc in the sleepQueue or the sleepQueue is empty
    if (sleepQueueHead == NULL || target->wakeupTime < sleepQueueHead->wakeupTime)
    {
        target->pNextSleeper = sleepQueueHead;
        sleepQueueHead = target;
    }
    // Target has more wait time than the head of the sleepQueue
    else
    {
        DevicesProcess* currProc = sleepQueueHead;

        while (currProc->pNextSleeper != NULL && currProc->wakeupTime < target->wakeupTime)
        {
            currProc = currProc->pNextSleeper;
        }

        if (currProc->wakeupTime < target->wakeupTime)
        {
            target->pNextSleeper = currProc->pNextSleeper;
            target->pPrevSleeper = currProc;
            
            if (currProc->pNextSleeper != NULL)
            {
                currProc->pNextSleeper->pPrevSleeper = target;
            }
            currProc->pNextSleeper = target;
        }
        else if (currProc->wakeupTime > target->wakeupTime)
        {
            target->pNextSleeper = currProc;
            target->pPrevSleeper = currProc->pPrevSleeper;

            if (currProc->pPrevSleeper != NULL)
            {
                currProc->pPrevSleeper->pNextSleeper = target;
            }
            currProc->pPrevSleeper = target;

            // If current is the head, then target becomes the new head
            if (sleepQueueHead == currProc)
            {
                sleepQueueHead = target;
            }
        }
    }

    // Release mutex
    k_semv(sleep_mutex);
}


static DevicesProcess* popSleepQueue()
{
    // Set mutex
    k_semp(sleep_mutex);

    DevicesProcess* target = sleepQueueHead;

    sleepQueueHead = sleepQueueHead->pNextSleeper;

    if (sleepQueueHead != NULL)
    {
        sleepQueueHead->pPrevSleeper = NULL;
    }

    // Release mutex
    k_semv(sleep_mutex);

    return target;
}


static void pushDiskReq(int index, DiskRequest* head, DiskRequest* target)
{
    k_semp(disk_mutex[index]);

    if (DISK_ARM_ALG == DISK_ARM_ALG_FCFS)
    {
        pushDiskReqHelperFCFS(index, head, target);
    }

    k_semv(disk_mutex[index]);
}


static void pushDiskReqHelperFCFS(int index, DiskRequest* head, DiskRequest* target)
{
    // Disk Request queue is empty
    if (diskRequestHead[index] == NULL)
    {
        target->nextReq = diskRequestHead[index];
        diskRequestHead[index] = target;
    }
    // Disk Request queue is not empty
    else
    {
        DiskRequest* currReq = diskRequestHead[index];

        while (currReq->nextReq != NULL)
        {
            currReq = currReq->nextReq;
        }

        currReq->nextReq = target;
        target->prevReq = currReq;
    }
}


static DiskRequest* popDiskReq(int index, DiskRequest* head)
{
    k_semp(disk_mutex[index]);

    DiskRequest* target;

    if (DISK_ARM_ALG == DISK_ARM_ALG_FCFS)
    {
        target = popDiskReqHelperFCFS(index, head);
    }

    k_semv(disk_mutex[index]);

    return target;
}


static DiskRequest* popDiskReqHelperFCFS(int index, DiskRequest* head)
{
    DiskRequest* target;

    target = diskRequestHead[index];
    
    if (diskRequestHead[index] != NULL)
    {
        diskRequestHead[index] = diskRequestHead[index]->nextReq;
    }

    return target;
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