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
static const rtwCAPI_ModelMappingInfo*   modelMap;
static const rtwCAPI_DataTypeMap*        dataTypeMap;
static const rtwCAPI_DimensionMap*       dimensionMap;
static const rtwCAPI_FixPtMap*           fixedPointMap;
static const uint_T*                     dimensionArray;
static void**                            dataAddressMap;


/* *******************************
 * PublishPoints Related Variables
 ******************************* */
/* from file */
static unsigned int  numPublishPoints;
static char**        publishPoints;

/* from model */
static unsigned int            numModelSignals;
static const rtwCAPI_Signals*  modelSignals;

/* shared memory */
static int           numPublishPointsShmId;
static char*         numPublishPointsShmAddress;
static unsigned int  publishPointsShmSize;
static int           publishPointsShmId;
static char*         publishPointsShmAddress;

/* provider <-> solver sync */
static sem_t*        publishSemaphore;


/* ******************************
 * UpdatePoints Related Variables
 ****************************** */
/* from model */
static unsigned int                    numModelParameters;
static const rtwCAPI_BlockParameters*  modelParameters;

/* provider <-> solver sync */
static sem_t*        updatesSemaphore;
static int           updatesFifo;

/* *******************************
 * TimeShm Related Variables
 ******************************* */
static int           timeShmId;
static char*         timeShmAddress;
static char* t_old;
static char *eptr;
static struct timespec start, finish, initial;
static double currentTime;
static double t_oldSim;
static double t_oldStart;
static double wait;
static double jitter;
static double ns2ms;
static double s2ms;
static double internalExeTime;
static double externalExeTime;
static double majorTimeStep;
static double expectedInterval;
static bool isIntegral;
static bool isInterval;

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
