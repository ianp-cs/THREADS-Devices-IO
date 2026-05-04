/* ------------------------------------------------------------------------
   Devices.c
   College of Applied Science and Technology
   The University of Arizona
   CYBV 489

   Student Names:  Ian Penrose and Lindsay Wax

   ------------------------------------------------------------------------ */

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
    struct devices_proc* pNextSleeper;  // Pointer to next device in the sleep queue
    struct devices_proc* pPrevSleeper;  // Pointer to previous device in the sleep queue
    int pid;                            // Process pid
    int semID;                          // Semaphor used to sleep and wake the process
    int wakeupTime;                     // Requested sleep duration
} DevicesProcess;

typedef struct
{
    int tracks;                                 // Number of tracks on the disk
    int platters;                               // Number of platters on the disk
    char deviceName[THREADS_MAX_DEVICE_NAME];   // Name of the device, i.e. "disk0"
} DiskInformation;
    
enum reqType { WRITE, READ, INFO }; // Types of DiskRequests

typedef struct disk_request
{
    // Used by all requests
    struct disk_request* nextReq;   // Next request in the disk request queue
    struct disk_request* prevReq;   // Previous request in the disk request queue
    int pid;                        // Pid of the process that created the request
    enum reqType type;              // Type of request
    char* deviceName;               // Name of the target disk
    int result;                     // Result code return by the relevant operation's function
    int status;                     // Status code returned by wait_device() calls
    // INFO request fields
    int sectorSize;                 // Size of the target disk's sectors
    int sectorCount;                // Number of sectors on the disk
    int trackCount;                 // Number of tracks on the disk
    int platterCount;               // Number of platters on the disk
    // READ/WRITE request fields
    void* dataBuffer;               // Data to be transferred
    int platter;                    // Target platter on the disk
    int track;                      // Target starting track on the disk
    int firstSector;                // Target starting sector on the disk
    int sectors;                    // Total number of sectors to be operated on (minimum 1)
 
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
int create_info_request(system_call_arguments_t* args, DiskRequest* req);
int create_read_request(system_call_arguments_t* args, DiskRequest* req);
int create_write_request(system_call_arguments_t* args, DiskRequest* req);
static void sysCall4(system_call_arguments_t* args);
static void pushSleepQueue(DevicesProcess* target);
static DevicesProcess* popSleepQueue();
static void pushDiskReq(int index, DiskRequest* target);
static DiskRequest* popDiskReq(int index, int* currentTrack, int* currentSector);
static DiskRequest* popDiskReqHelperFCFS(int index);
static DiskRequest* popDiskReqHelperSSTF(int index, int* currentTrack, int* currentSector);
static DiskRequest* getClosestReq(int index, int* currentTrack, int* currentSector);
static void initDiskInfo(int diskNum);
static int diskSeek(char* deviceName, int target);
static void clearProcess(DevicesProcess* target);
static inline void checkKernelMode(const char* functionName);

extern int DevicesEntryPoint(char*);

/* GLOBAL VARIABLES */
static DevicesProcess devicesProcs[MAXPROC];                // List of device processes
static DevicesProcess* sleepQueueHead;                      // Queue of sleeping processes
static DiskRequest* diskRequestHead[THREADS_MAX_DISKS];     // Queue of disk requests
static DiskInformation diskInfo[THREADS_MAX_DISKS];         // List of structs containing details about each disk
static int diskInitChecker = -1;                            // Holds the id of the semaphor used to ensure disk initializations are complete

/*
 *  Routine:  SystemCallsEntryPoint
 *
 *  Description: Called by the SystemCalls module and initiates the setup for device I/O operations.
 *
 *  Arguments: char* arg -- optional arguments
 *
 *  Return Value: 0 means success
 *
 */
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
        clearProcess(&devicesProcs[i]);
        devicesProcs[i].semID = k_semcreate(0);
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
        disk_mutex[i] = k_semcreate(1);
    }

    // Make program wait until all disks spin up
    diskInitChecker = k_semcreate(0);

    for (int i = 0; i < THREADS_MAX_DISKS; i++)
    {
        k_semp(diskInitChecker); // Handshake with each disk
    }

    k_semfree(diskInitChecker);

    /* Create first user-level process and wait for it to finish */
    sys_spawn("DevicesEntryPoint", DevicesEntryPoint, NULL, 8 * THREADS_MIN_STACK_SIZE, 3);

    // End running processes
    returnPid = k_wait(&status); // DevicesEntryPoint,  pid 8

    for (int i = 0; i < MAXPROC; ++i)
    {
        k_semfree(devicesProcs[i].semID);
    }

    returnPid = k_wait(&status); // Disk driver 0,      pid 6
    returnPid = k_wait(&status); // Disk driver 1,      pid 7

    k_kill(clockPID, SIG_TERM);
    returnPid = k_wait(&status); // Clock driver,       pid 5

    return 0;
}

/*
 *  Routine: ClockDriver
 *
 *  Description: Uses the THREADS system clock to check each sleeping process and wakes them if
 *               their wait time has elapsed.
 *
 *  Arguments: char* pArg - optional arguments passed to the driver
 *
 *  Return Value: 0 means success
 *
 */
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
            return -1;
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

/*
 *  Routine: DiskDriver
 *
 *  Description: Initializes the disk specified by arg and then processes incoming disk READ, WRITE,
 *               and INFO requests.
 *
 *  Arguments: char* pArg - the index of the disk
 *
 *  Return Value: 0 means success
 *
 */
static int DiskDriver(char* arg)
{
    int pid = k_getpid();
    int diskNum = atoi(arg);

    set_psr(get_psr() | PSR_INTERRUPTS);

    /* Read the disk info */
    initDiskInfo(diskNum);

    /* Initialize disk arm position */
    int currentTrack = diskSeek(diskInfo[diskNum].deviceName, 0);
    int currentSector = 0;

    k_semv(diskInitChecker);
    /* Operating loop */
    
    while (!signaled())
    {
        k_semp(devicesProcs[pid % MAXPROC].semID); // Block until summoned

        if (diskRequestHead[diskNum] != NULL)
        {
            DiskRequest* req = popDiskReq(diskNum, &currentTrack, &currentSector);

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

            k_semv(devicesProcs[req->pid % MAXPROC].semID); // Unblock user process that sent disk request
        }
    }
    
    return 0;
}

/*
 *  Routine: initDiskInfo
 *
 *  Description: Initializes the diskInfo structs when disk drivers are being initialized.
 *
 *  Arguments: int diskNum - the index of the disk being initialized
 *
 *  Return Value: None
 *
 */
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

/*
 *  Routine: diskSeek
 *
 *  Description: Moves the arm of the disk specified by deviceName to the track specified by 
 *               target.
 *
 *  Arguments: char* deviceName - name of the disk
 *             int target - the track on the disk the arm needs to be moved to
 *
 *  Return Value: int target
 *
 */
static int diskSeek(char* deviceName, int target)
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
        console_output(false, "diskSeek(): Device control failed! Couldn't move arm...\n");
        stop(1);
    }

    wait_device(deviceName, &status);

    return target;
}

/*
 *  Routine: sys_disk_info
 *
 *  Description: Processes an INFO disk request, pulling information about the disk and placing it
 *               with the request's fields to be returned to the calling process.
 *
 *  Arguments: DiskRequest* req - the request to be processed
 *
 *  Return Value: 0 means success
 *
 */
int sys_disk_info(DiskRequest* req)
{
    int index = k_getpid() - 6; // - 6 because disk pids start at 6

    req->sectorSize = THREADS_DISK_SECTOR_SIZE;
    req->sectorCount = THREADS_DISK_SECTOR_COUNT;
    req->trackCount = diskInfo[index].tracks;
    req->platterCount = diskInfo[index].platters;

    return 0;
}

/*
 *  Routine: sys_disk_read
 *
 *  Description: Processes a READ disk request, pulling data from the disk and placing it into
 *               the request's fields.
 *
 *  Arguments: DiskRequest* req - the request to be processed
 *             int diskNum - the index of the disk being operated on
 *             int* currentTrack - the current track the disk arm is located at
 *             int* currentSector - the current sector the disk arm is located at
 *
 *  Return Value: 0 means success
 *
 */
int sys_disk_read(DiskRequest* req, int diskNum, int* currentTrack, int* currentSector)
{
    device_control_block_t devRequest;
    int sectorsProcessed = 0;

    // Move to initial track and sector
    *currentSector = req->firstSector;

    if (*currentTrack != req->track)
    {
        *currentTrack = diskSeek(req->deviceName, req->track);
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
            *currentTrack = diskSeek(req->deviceName, nextTrack);
        }
    }

    return 0;
}

/*
 *  Routine: sys_disk_write
 *
 *  Description: Processes a WRITE disk request, pulling data from the request's fields and placing
 *               it on the disk.
 *
 *  Arguments: DiskRequest* req - the request to be processed
 *             int diskNum - the index of the disk being operated on
 *             int* currentTrack - the current track the disk arm is located at
 *             int* currentSector - the current sector the disk arm is located at
 *
 *  Return Value: 0 means success
 *
 */
int sys_disk_write(DiskRequest* req, int diskNum, int* currentTrack, int* currentSector)
{
    device_control_block_t devRequest;
    int sectorsProcessed = 0;

    // Move to initial track and sector
    *currentSector = req->firstSector;
    
    if (*currentTrack != req->track)
    {
        *currentTrack = diskSeek(req->deviceName, req->track);
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
            *currentTrack = diskSeek(req->deviceName, nextTrack);
        }
    }

    return 0;
}

/*
 *  Routine: sys_sleep
 *
 *  Description: Places the current process into the sleep queue and then puts the process to sleep.
 *
 *  Arguments: int* seconds - the duration that the process will sleep
 *
 *  Return Value: 0 means success, -1 means invalid parameter
 *
 */
int sys_sleep(int* seconds)
{
    int pid = k_getpid();

    if (*seconds <= 0)
    {
        console_output(FALSE, "%s - Invalid number of seconds.\n", "sys_sleep");
        return -1;
    }

    devicesProcs[pid % MAXPROC].wakeupTime = ((*seconds) * 1000000) + system_clock();
    pushSleepQueue(&devicesProcs[pid % MAXPROC]);
    k_semp(devicesProcs[pid % MAXPROC].semID);

    return 0;
}

/*
 *  Routine: create_info_request
 *
 *  Description: Creates an INFO disk request, placing the return values in the request into the
 *               input args struct.
 *
 *  Arguments: system_call_arguments_t* args - the args needed for the request, then holds returned data
 *             DiskRequest* req - an initialized struct that facilitates data transfer between the disk 
                                  and calling process
 *
 *  Return Value: 0 means success, -1 means invalid parameter
 *
 */
int create_info_request(system_call_arguments_t* args, DiskRequest* req)
{
    req->deviceName = (char*)args->arguments[0];
    req->type = INFO;

    for (int i = 0; i < THREADS_MAX_DISKS; i++)
    {
        if (strcmp(req->deviceName, diskInfo[i].deviceName) == 0)
        {
            pushDiskReq(i, req);
            k_semv(devicesProcs[(i + 6) % MAXPROC].semID); // i + 6 because disk pids start at 6
            k_semp(devicesProcs[req->pid % MAXPROC].semID); // Sleep until disk is finished

            args->arguments[3] = (intptr_t)req->result;
            args->arguments[0] = (intptr_t)req->sectorSize;
            args->arguments[1] = (intptr_t)req->sectorCount;
            args->arguments[2] = (intptr_t)req->trackCount;
            args->arguments[4] = (intptr_t)req->platterCount;

            return 0;
        }
    }

    args->arguments[3] = (intptr_t)(-1);
    console_output(false, "SYS_DISKINFO(): Invalid device name.\n");
    return -1;
}

/*
 *  Routine: create_read_request
 *
 *  Description: Creates a READ disk request, after performing input validation on args, then moves
 *               data between the args and req structs.
 *
 *  Arguments: system_call_arguments_t* args - the args needed for the request, then holds returned data
 *             DiskRequest* req - an initialized struct that facilitates data transfer between the disk
                                  and calling process
 *
 *  Return Value: 0 means success, -1 means invalid parameter
 *
 */
int create_read_request(system_call_arguments_t* args, DiskRequest* req)
{

    req->deviceName = (char*)args->arguments[0];

    req->type = READ;
    req->dataBuffer = args->arguments[1];
    req->platter = args->arguments[2];
    req->track = args->arguments[3];
    req->firstSector = args->arguments[4];
    req->sectors = args->arguments[5];

    for (int i = 0; i < THREADS_MAX_DISKS; i++)
    {
        if (strcmp(req->deviceName, diskInfo[i].deviceName) == 0)
        {
            // Input validation
            if (req->platter < 0 || req->platter >= diskInfo[i].platters)
            {
                console_output(false, "SYS_DISKREAD(): Invalid platter.\n");
                return -1;
            }
            if (req->track < 0 || req->track >= diskInfo[i].tracks)
            {
                console_output(false, "SYS_DISKREAD(): Invalid track.\n");
                return -1;
            }
            if (req->firstSector < 0 || req->firstSector >= THREADS_DISK_SECTOR_COUNT)
            {
                console_output(false, "SYS_DISKREAD(): Invalid first sector.\n");
                return -1;
            }
            if (req->sectors < 1 || req->sectors > THREADS_DISK_SECTOR_COUNT)
            {
                console_output(false, "SYS_DISKREAD(): Invalid number of sectors.\n");
                return -1;
            }

            pushDiskReq(i, req);
            k_semv(devicesProcs[(i + 6) % MAXPROC].semID); // i + 6 because disk pids start at 6
            k_semp(devicesProcs[req->pid % MAXPROC].semID); // Sleep until disk is finished

            args->arguments[3] = (intptr_t)req->result;
            args->arguments[0] = (intptr_t)req->status;
            args->arguments[1] = (intptr_t)req->dataBuffer;

            return 0;
        }
    }

    args->arguments[3] = (intptr_t)(-1);
    console_output(false, "SYS_DISKREAD(): Invalid device name.\n");
    return -1;
}

/*
 *  Routine: create_write_request
 *
 *  Description: Creates a WRITE disk request, after performing input validation on args, then moves
 *               data between the args and req structs.
 *
 *  Arguments: system_call_arguments_t* args - the args needed for the request, then holds returned data
 *             DiskRequest* req - an initialized struct that facilitates data transfer between the disk
                                  and calling process
 *
 *  Return Value: 0 means success, -1 means invalid parameter
 *
 */
int create_write_request(system_call_arguments_t* args, DiskRequest* req)
{
    req->deviceName = (char*)args->arguments[0];

    req->type = WRITE;
    req->dataBuffer = args->arguments[1];
    req->platter = args->arguments[2];
    req->track = args->arguments[3];
    req->firstSector = args->arguments[4];
    req->sectors = args->arguments[5];

    for (int i = 0; i < THREADS_MAX_DISKS; i++)
    {
        if (strcmp(req->deviceName, diskInfo[i].deviceName) == 0)
        {
            // Input validation
            if (req->platter < 0 || req->platter >= diskInfo[i].platters)
            {
                console_output(false, "SYS_DISKWRITE(): Invalid platter.\n");
                return -1;
            }
            if (req->track < 0 || req->track >= diskInfo[i].tracks)
            {
                console_output(false, "SYS_DISKWRITE(): Invalid track.\n");
                return -1;
            }
            if (req->firstSector < 0 || req->firstSector >= THREADS_DISK_SECTOR_COUNT)
            {
                console_output(false, "SYS_DISKWRITE(): Invalid first sector.\n");
                return -1;
            }
            if (req->sectors < 1 || req->sectors > THREADS_DISK_SECTOR_COUNT)
            {
                console_output(false, "SYS_DISKWRITE(): Invalid number of sectors.\n");
                return -1;
            }

            pushDiskReq(i, req);
            k_semv(devicesProcs[(i + 6) % MAXPROC].semID); // i + 6 because disk pids start at 6
            k_semp(devicesProcs[req->pid % MAXPROC].semID); // Sleep until disk is finished

            args->arguments[3] = (intptr_t)req->result;
            args->arguments[0] = (intptr_t)req->status;

            return 0;
        }
    }

    args->arguments[3] = (intptr_t)(-1);
    console_output(false, "SYS_DISKWRITE(): Invalid device name.\n");
    return -1;
}

/*
 *  Routine: sysCall4
 *
 *  Description: Handles device-based system calls from user processes, including SLEEP, INFO, READ,
 *               and WRITE requests. A diskRequest object is created and sent to relevant request
 *               functions, except in the case of a SLEEP request, where it is then unused.
 *
 *  Arguments: system_call_arguments_t* args - the args needed for the request, then holds returned data
 *
 *  Return Value: None
 *
 */
static void sysCall4(system_call_arguments_t* args)
{
    int status = 0;

    // Initialize Disk request
    DiskRequest req;
    req.nextReq = NULL;
    req.prevReq = NULL;
    req.pid = -1;
    req.deviceName = NULL;
    req.sectorSize = -1;
    req.sectorCount = -1;
    req.trackCount = -1;
    req.platterCount = -1;
    req.dataBuffer = NULL;
    req.platter = -1;
    req.track = -1;
    req.firstSector = -1;
    req.sectors = -1;
    req.result = -1;
    req.status = -1;

    // Add to device process list
    int pid = k_getpid();
    req.pid = pid;

    devicesProcs[pid % MAXPROC].pid = pid;

    switch (args->call_id)
    {
        case SYS_SLEEP:
            args->arguments[3] = (intptr_t)sys_sleep(&args->arguments[0]);
            break;

        case SYS_DISKREAD:
            create_read_request(args, &req);
            break;

        case SYS_DISKWRITE:
            create_write_request(args, &req);
            break;

        case SYS_DISKINFO:
            create_info_request(args, &req);
            break;
    }

    clearProcess(&devicesProcs[pid % MAXPROC]);
    return;
}

/*
 *  Routine: pushSleepQueue
 *
 *  Description: Places the target DevicesProcess onto the end of the sleep queue.
 *
 *  Arguments: DevicesProcess* target - the target process to be added
 *
 *  Return Value: None
 *
 */
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

/*
 *  Routine: popSleepQueue
 *
 *  Description: Removes the head of the queue and returns it.
 *
 *  Arguments: None
 *
 *  Return Value: DevicesProcess* target - the head of the queue or NULL if the queue is empty
 *
 */
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

/*
 *  Routine: pushDiskReq
 *
 *  Description: Places the target DiskRequest* onto the end of the disk requests queue.
 *
 *  Arguments: int index - determines which disk is used during the push operation
 *             DiskRequest* target - the target request to be added
 *
 *  Return Value: None
 *
 */
static void pushDiskReq(int index, DiskRequest* target)
{
    k_semp(disk_mutex[index]);

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

    k_semv(disk_mutex[index]);
}

/*
 *  Routine: popDiskReq
 *
 *  Description: If the current disk arm algorithm is set to FCFS, returns the head of the disk
 *               request queue indicated by index. If algorithm is set to SSTP, the request that
 *               operates closest to the current location of the disk arm is returned.
 *
 *  Arguments: int index - determines which disk is used during the pop operation
 *             int* currentTrack - the current track the disk arm is located at
 *             int* currentSector - the current sector the disk arm is located at
 *
 *  Return Value: DiskRequest* target - the head of the disk request queue if using FCFS, the 
 *                                      nearest request if SSTP, or NULL if the queue is empty
 *
 */
static DiskRequest* popDiskReq(int index, int* currentTrack, int* currentSector)
{
    k_semp(disk_mutex[index]);

    DiskRequest* target;

    if (DISK_ARM_ALG == DISK_ARM_ALG_FCFS)
    {
        target = popDiskReqHelperFCFS(index);
    }
    else if (DISK_ARM_ALG == DISK_ARM_ALG_SSTF)
    {
        target = popDiskReqHelperSSTF(index, currentTrack, currentSector);
    }

    if (target == NULL)
    {
        console_output(false, "popDiskReq(): Attempted to pop from request queue, but the queue is empty!\n");
    }

    k_semv(disk_mutex[index]);

    return target;
}

/*
 *  Routine: popDiskReqHelperFCFS
 *
 *  Description: This helper function is used when the disk arm algorithm is set to FCFS. Performs
 *               the actual pop operation and returns the head of the queue or NULL if the queue is
 *               empty.
 *
 *  Arguments: int index - determines which disk is used during the pop operation
 *
 *  Return Value: DiskRequest* target - the head of the disk request queue if using FCFS or NULL if 
                                        the queue is empty
 *
 */
static DiskRequest* popDiskReqHelperFCFS(int index)
{
    DiskRequest* target = NULL;

    target = diskRequestHead[index];
    
    if (diskRequestHead[index] != NULL)
    {
        diskRequestHead[index] = diskRequestHead[index]->nextReq;
    }

    return target;
}

/*
 *  Routine: popDiskReqHelperSSTF
 *
 *  Description: This helper function is used when the disk arm algorithm is set to SSTP. Performs
 *               the actual pop operation and returns the request whose operation is closest to the 
                 current position of the arm. or NULL if the queue is empty.
 *
 *  Arguments: int index - determines which disk is used during the pop operation
 *             int* currentTrack - the current track the disk arm is located at
 *             int* currentSector - the current sector the disk arm is located at
 *
 *  Return Value: DiskRequest* target - the nearest request or NULL if the queue is empty
 *
 */
static DiskRequest* popDiskReqHelperSSTF(int index, int* currentTrack, int* currentSector)
{
    DiskRequest* target = getClosestReq(index, currentTrack, currentSector);

    if (target == NULL) // Empty request queue
    {
        return NULL;
    }

    // Detach request from queue
    if (target->prevReq != NULL)
    {
        target->prevReq->nextReq = target->nextReq;
    }
    if (target->nextReq != NULL)
    {
        target->nextReq->prevReq = target->prevReq;
    }

    // If target is the head of the list, move head to next in queue
    if (target == diskRequestHead[index])
    {
        diskRequestHead[index] = target->nextReq;
    }

    return target;
}

/*
 *  Routine: getClosestReq
 *
 *  Description: This helper function is used when the disk arm algorithm is set to SSTP. Every
 *               request in the queue has its distance from the disk arm calculated. The nearest
 *               request is then returned.
 *
 *  Arguments: int index - determines which disk is used during the pop operation
 *             int* currentTrack - the current track the disk arm is located at
 *             int* currentSector - the current sector the disk arm is located at
 *
 *  Return Value: DiskRequest* closestReq - the request closest to the disk arm
 *
 */
static DiskRequest* getClosestReq(int index, int* currentTrack, int* currentSector)
{
    DiskRequest* closestReq = NULL;
    DiskRequest* currReq = diskRequestHead[index];
    int shortestDist = -1;
    int currDist = 0;

    while (currReq != NULL)
    {
        currDist = abs(currReq->track - *currentTrack);// + abs(currReq->firstSector - *currentSector);

        if (currDist < shortestDist || shortestDist == -1)
        {
            shortestDist = currDist;
            closestReq = currReq;
        }
        
        currReq = currReq->nextReq;
    }

    return closestReq;
}

/*
 *  Routine: clearProcess
 *
 *  Description: Cleans the process struct's fields, preparing it for future use by a new process.
 *
 *  Arguments: DevicesProcess* target - the target to be cleared out
 *
 *  Return Value: None
 *
 */
static void clearProcess(DevicesProcess* target)
{
    target->pid = -1;
    target->wakeupTime = -1;
    target->pNextSleeper = NULL;
    target->pPrevSleeper = NULL;
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