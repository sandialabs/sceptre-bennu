/*
 * ************************
 * Hooking Model Code Setup
 * ************************
 *
 * NOTE: Compile the model once before hooking so that MODEL_NAME can be found
 * in the compiled model c code. (const object under 'Real-time model' comment)
 *    (e.g. MODEL_NAME = CoDCon_Feb18_FirstRun_update_M)
 *
 * Header File:
 *    #include "modelHooks.h"
 *
 * Source Files:
 *    modelHooks.c
 *
 * Libraries:
 *    /usr/lib/x86_64-linux-gnu/libpthread.so
 *    (used for semaphores)
 *
 * Initialize Function:
 *    rtwCAPI_ModelMappingInfo *mmi = &(rtmGetDataMapInfo(MODEL_NAME).mmi);
 *    init(mmi);
 *
 * System Outputs (Drag and drop from MATLAB 'custcode' command into model):
 *  - Declaration Code:
 *      time_T* time = rtmGetTPtr(MODEL_NAME);
 *  - Execution Code:
 *      step(time);
 *
 * Terminatie Function:
 *    term();
 */

/*
 * *************
 * Sync DTO Info
 * *************
 *
 * NOTE: Sometimes if the solver crashes the shared memory will still exist and
 * be locked. To fix this use the following commands:
 * ```ipcs``` Lists the shared memory segments that exists and their shmid
 * ```ipcrm``` Removes shared memory segments, either -a all or -m by shmid
 *
 * NOTE: In a similar fashion, if you need to manually remove the FIFO file,
 * use the ```unlink``` command
 *
 * PublishPoint Shared Memory DTO Format:
 *    "name:type:value"
 *      (e.g. StripMixer1_VFD_SpeedFeedback_AI:DOUBLE:0.88)
 *
 * UpdatePoint FIFO DTO Format:
 *    "name:type:value"
 *      (e.g. PLC - plc_StripMixer8_VFD_RunEnable_DO:BOOLEAN:0)
 *
 * Parts:
 *  - name:
 *      The name of the I/O point.
 *       (e.g. StripMixer1_VFD_SpeedFeedback_AI)
 *       (see publishPoints.txt and updatePoints.txt for a list of I/O points)
 *      Can provide field name with '.'
 *       (e.g. StripMixer1_VFD_SpeedFeedback_AI.speed)
 *       (if no field name is provided it will be set to processModelIO in filter)
 *  - type:
 *      The data type of the I/O point.
 *       (e.g. DOUBLE)
 *  - value:
 *      The value of the I/O point.
 *       (e.g. 0.88)
 */

#ifndef MODEL_HOOKS
#define MODEL_HOOKS

/* For real-time code execution in step() function */
#define __USE_POSIX199309
#define _POSIX_C_SOURCE 199309L

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <semaphore.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mman.h>

#include "rtw_modelmap.h"
#include "builtin_typeid_types.h"
#include "rtwtypes.h"


/* *******************************************************************
 * Global Constants (VALUES MUST MATCH CONSTANTS IN SIMULINK PROVIDER)
 ******************************************************************* */
#define MAX_MSG_LEN 256 /* using #define because c90 has issues with variable arrays */

/* shared memory for PublishPoints */
static const unsigned int NUM_PUBLISH_POINTS_SHM_KEY = 10610;
static const unsigned int PUBLISH_POINTS_SHM_KEY     = 10613;
/* For real-time code execution in step() function */
static const unsigned int TIME_SHM_KEY               = 10623;

/* fifo queue for UpdatePonts messages */
static const char* UPDATES_FIFO = "/tmp/updates_fifo";

/* provider <-> solver sync */
static const char* PUBLISH_SEM  = "publish_sem";
static const char* UPDATES_SEM  = "updates_sem";


/* ********************************
 * Global Internal Helper Variables
 ******************************** */
/* solver input/output files */
static const char* PUBLISH_POINTS_FILE = "publishPoints.txt";
static const char* GROUND_TRUTH_FILE   = "groundTruth.txt";
static const char* DEBUG_FILE          = "debug.on";
static const char* DEBUG_VERBOSE_FILE  = "debug.verbose";
static const char* GROUND_TRUTH_FILTER = "Input-Output Signals";
static const unsigned int GT_PRINT_INTERVAL = 9000; /* no real measurement */

/* 1 = debug printing enabled, 0 = no debug output */
static bool DEBUG = 0;
static bool DEBUG_VERBOSE = 0;
static const unsigned int DEBUG_PRINT_INTERVAL = 1;

static const unsigned int EXIT_ERROR = 1;


/* ********************************
 * Simulink Model Mapping Variables
 ******************************** */
const rtwCAPI_ModelMappingInfo*   modelMap;
const rtwCAPI_DataTypeMap*        dataTypeMap;
const rtwCAPI_DimensionMap*       dimensionMap;
const rtwCAPI_FixPtMap*           fixedPointMap;
const uint_T*                     dimensionArray;
void**                            dataAddressMap;


/* *******************************
 * PublishPoints Related Variables
 ******************************* */
/* from file */
unsigned int  numPublishPoints;
char**        publishPoints;

/* from model */
unsigned int            numModelSignals;
const rtwCAPI_Signals*  modelSignals;

/* shared memory */
int           numPublishPointsShmId;
char*         numPublishPointsShmAddress;
unsigned int  publishPointsShmSize;
int           publishPointsShmId;
char*         publishPointsShmAddress;

/* provider <-> solver sync */
sem_t*        publishSemaphore;


/* ******************************
 * UpdatePoints Related Variables
 ****************************** */
/* from model */
unsigned int                    numModelParameters;
const rtwCAPI_BlockParameters*  modelParameters;

/* provider <-> solver sync */
sem_t*        updatesSemaphore;
int           updatesFifo;

/* *******************************
 * TimeShm Related Variables
 ******************************* */
int           timeShmId;
char*         timeShmAddress;
char* t_old;
char *eptr;
struct timespec start, finish, initial;
double currentTime;
double t_oldSim;
double t_oldStart;
double wait;
double jitter;
double ns2ms;
double s2ms;
double internalExeTime;
double externalExeTime;
double majorTimeStep;
double expectedInterval;
bool isIntegral;
bool isInterval;

/* **********************************
 * Model Hooks (called from Simulink)
 ********************************** */
void init(rtwCAPI_ModelMappingInfo*);
void step(time_T*);
void term();


/* *************************
 * Model Interaction Helpers
 ************************* */
void publishState();
void applyUpdates();
char* getModelSignalValue(const char*);
void updateModelParameterValue(const char*, char*);


/* *********************
 * Shared Memory Helpers
 ********************* */
int createSharedMemory(key_t, int);
char* attachSharedMemory(int);
void destroySharedMemory(key_t, char*);


/* *************
 * Misc. Helpers
 ************* */
void readFileToMemory(const char*, char***, unsigned int*);
void printGroundTruth();
void printModelMemberCounts();

#endif /* MODEL_HOOKS */
