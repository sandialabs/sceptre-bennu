#include "modelHooks.h"


/* **********************************
 * Model Hooks (called from Simulink)
 ********************************** */
void init(rtwCAPI_ModelMappingInfo* _modelMap)
{
  setbuf(stdout, NULL); /* Disable stdout buffering */

  if (-1 != access(DEBUG_FILE, F_OK)) {
    DEBUG = true;
  }
  if (-1 != access(DEBUG_VERBOSE_FILE, F_OK)) {
    DEBUG = true;
    DEBUG_VERBOSE = true;
  }

  printf("Info: Model initialization hooked\n");

  /* Simulink Model */
  modelMap = _modelMap;
  if (NULL == modelMap) {
    printf("Fatal: Model Mapping Info is NULL\n");
    exit(EXIT_ERROR);
  }

  /* Value to Data Type Mapping */
  dataTypeMap = rtwCAPI_GetDataTypeMap(modelMap);
  if (NULL == dataTypeMap) {
    printf("Fatal: Data Type Map is NULL\n");
    exit(EXIT_ERROR);
  }

  /* Value to Dimension Metadata Mapping */
  dimensionMap = rtwCAPI_GetDimensionMap(modelMap);
  dimensionArray = rtwCAPI_GetDimensionArray(modelMap);
  if ((NULL == dimensionMap) || (NULL == dimensionArray)) {
    printf("Fatal: Data Dimension Info is NULL\n");
    exit(EXIT_ERROR);
  }

  /* Value to Fixed-Point Metadata Mapping */
  fixedPointMap = rtwCAPI_GetFixPtMap(modelMap);
  if (NULL == fixedPointMap) {
    printf("Fatal: Fix Point Map is NULL\n");
    exit(EXIT_ERROR);
  }

  /* Value Data Address */
  dataAddressMap = rtwCAPI_GetDataAddressMap(modelMap);
  if (NULL == dataAddressMap) {
    printf("Fatal: Data Address Map is NULL\n");
    exit(EXIT_ERROR);
  }

  /* Model Signals - aka PublishPoints */
  numModelSignals = rtwCAPI_GetNumSignals(modelMap);
  modelSignals = rtwCAPI_GetSignals(modelMap);
  if (NULL == modelSignals) {
    printf("Fatal: Signal Mapping Info is NULL\n");
    exit(EXIT_ERROR);
  }

  /* Model Parameters - aka UpdatePoints */
  numModelParameters = rtwCAPI_GetNumBlockParameters(modelMap);
  modelParameters = rtwCAPI_GetBlockParameters(modelMap);
  if (NULL == modelParameters) {
    printf("Fatal: Parameter Mapping Info is NULL\n");
    exit(EXIT_ERROR);
  }

  if (DEBUG) {
    printModelMemberCounts();
  }

  /* Read PublishPoints file into memory */
  numPublishPoints = 128; /*number will be set to the correct count after reading*/
  readFileToMemory(PUBLISH_POINTS_FILE, &publishPoints, &numPublishPoints);

  printf("Info: Read %d PublishPoints\n", numPublishPoints);

  /* Create a semaphores in the locked state (last arg = 0)*/
  publishSemaphore = sem_open(PUBLISH_SEM, O_CREAT, 0644, 0);
  if((void*)-1 == publishSemaphore) {
    printf("Fatal: Unable to create publish semaphore\n");
    exit(EXIT_ERROR);
  }
  updatesSemaphore = sem_open(UPDATES_SEM, O_CREAT, 0644, 0);
  if((void*)-1 == updatesSemaphore) {
    printf("Fatal: Unable to create updates semaphore\n");
    exit(EXIT_ERROR);
  }

  /* Create a FIFO queue for passing UpdatePoints from provider to solver */
  unlink(UPDATES_FIFO); /* just incase fifo still exists from last run */
  if (-1 == mkfifo(UPDATES_FIFO, 0666)) {
    printf("Fatal: Unable to create update FIFO at %s\n", UPDATES_FIFO);
    exit(EXIT_ERROR);
  }
  updatesFifo = open(UPDATES_FIFO, O_RDONLY | O_TRUNC | O_NONBLOCK);
  if (-1 == updatesFifo) {
    printf("Fatal: Unable to open updates FIFO at %s for non-blocked reading\n", UPDATES_FIFO);
    exit(EXIT_ERROR);
  }

  /* Shared Memory Count Blocks */
  numPublishPointsShmId = createSharedMemory(NUM_PUBLISH_POINTS_SHM_KEY, sizeof(unsigned int));
  numPublishPointsShmAddress = attachSharedMemory(numPublishPointsShmId);
  sprintf(numPublishPointsShmAddress, "%u", numPublishPoints);

  /* Shared Memory Blocks */
  publishPointsShmSize = numPublishPoints * MAX_MSG_LEN;
  publishPointsShmId = createSharedMemory(PUBLISH_POINTS_SHM_KEY, publishPointsShmSize);
  publishPointsShmAddress = attachSharedMemory(publishPointsShmId);
  publishState();
  
  /* Unlock semaphores */
  sem_post(publishSemaphore);
  sem_post(updatesSemaphore);

  /* Shared Memory Block for time in-order to calculate the external execution time outside 
     of the step function. Set initial times to zero refasan*/
  t_oldSim    = 0;      t_oldStart = 0;
  ns2ms = 1e-3;         s2ms  = 1e+6;
  expectedInterval = 0; jitter = 0;
  currentTime = 0;      
  
  timeShmId = createSharedMemory(TIME_SHM_KEY, sizeof(long));
  timeShmAddress = attachSharedMemory(timeShmId);
  sprintf(timeShmAddress, "%lu", 0);

  clock_gettime(CLOCK_REALTIME, &initial);
}


void step(time_T* time)
{
  clock_gettime(CLOCK_REALTIME, &start);

  // Calculate the simulation time step by subtracting the current simulation time
  // from the previous simulation time
  majorTimeStep = (*time - t_oldSim)*s2ms;
  t_oldSim = *time;
  // Keep track of the current simulation time
  expectedInterval = expectedInterval + majorTimeStep;
  
  // .tv_nsec returns the number of nanosecounds in the current second
  // therefore if t_oldStart is less than the current start time
  // a second has elapsed. Add one micro second is added to the current time
  if (t_oldStart > (double)(start.tv_nsec)) {
    currentTime = currentTime + 1e+6; 
  }
  t_oldStart = start.tv_nsec;
  jitter = expectedInterval - currentTime;
  
  isIntegral = (*time - (int)*time == 0);
  isInterval = ((int)*time % DEBUG_PRINT_INTERVAL == 0);
  if (isIntegral && isInterval) {
    printf("[time: %f]\n", *time);
    char* shmPtr = publishPointsShmAddress;
    for (int i = 0; i < numPublishPoints; i++) {
      printf("PublishPoint: %s\n", shmPtr);
      fflush(stdout);
      shmPtr += MAX_MSG_LEN;
    }
  /* NOTE: Whoever is going to be reading the GroundTruth.txt file should also
   * acquire the same semaphores to prevent reading it in an incomplete state.
   * This is locked by the PublishPoints Semaphore because PublishPoints are
   * a subset of the GroundTruth, whereas UpdatePoints are not included. */
    sem_wait(publishSemaphore);
    printGroundTruth();
    sem_post(publishSemaphore);

  }
  
  // Using rate trasition blocks in simulink guarantees that the majorTimeStep 
  // will be zero for the rate specified in the rate transition block
  if (majorTimeStep == 0) {
    sem_wait(publishSemaphore);
    publishState();
    sem_post(publishSemaphore);
  }
    
  sem_wait(updatesSemaphore);
  applyUpdates();
  sem_post(updatesSemaphore);
  
  clock_gettime(CLOCK_REALTIME, &finish);
  internalExeTime = (double)(finish.tv_nsec - start.tv_nsec)*ns2ms;
  if (internalExeTime < 0){
      internalExeTime = 0;
  }
  externalExeTime = (double)(start.tv_nsec - strtol(timeShmAddress,&eptr,10))*ns2ms;
  if (externalExeTime < 0){
      externalExeTime = 0;
  }
  
  if (isIntegral && isInterval) {
    printf("%f majorTimeStep [Mico-seconds]\n",majorTimeStep);
    printf("%f internalExeTime [Mico-seconds]\n",internalExeTime);
    printf("%f externalExeTime [Micro-seconds]\n",externalExeTime);
    printf("%f jitter [Micro-seconds]\n",jitter);
    fflush(stdout);
  }
  
  wait = majorTimeStep - internalExeTime - externalExeTime + jitter;
  
  if (*time == 0) {
    wait = majorTimeStep;
  }else if (wait < 0){
    wait = 0;
  }
  
  usleep((unsigned int)(wait));
  clock_gettime(CLOCK_REALTIME, &finish);
  sprintf(timeShmAddress, "%lu", finish.tv_nsec);
}


void term()
{
  /* NOTE: This never gets called if you ^c out of the solver */
  int i;

  printf("Info: Model termination hooked\n");

  sem_wait(publishSemaphore);
  sem_wait(updatesSemaphore);

  destroySharedMemory(numPublishPointsShmId, numPublishPointsShmAddress);
  destroySharedMemory(publishPointsShmId, publishPointsShmAddress);

  sem_close(publishSemaphore);
  sem_unlink(PUBLISH_SEM);

  sem_close(updatesSemaphore);
  sem_unlink(UPDATES_SEM);

  close(updatesFifo);
  unlink(UPDATES_FIFO);

  for (i = 0; i < numPublishPoints; i++) {
    if (NULL != publishPoints[i]) {
      free(publishPoints[i]);
    }
  }
  if (NULL != publishPoints) {
    free(publishPoints);
  }
}



/* *******************************
 * Start Model Interaction Helpers
 ******************************* */
void publishState()
{
  int i;
  char* shmPtr = publishPointsShmAddress;

  for (i = 0; i < numPublishPoints; i++) {
    char* dto = getModelSignalValue(publishPoints[i]);

    strcpy(shmPtr, dto);
    shmPtr += MAX_MSG_LEN;

    /*dto Memory is allocated in getModelSignalValue*/
    if (NULL != dto) {
      free(dto);
    }
  }
}

void applyUpdates()
{
  char *tag, *type, *val;
  char *updateMessage = (char*)calloc(MAX_MSG_LEN, sizeof(char));
  if (NULL == updateMessage) {
    printf("Fatal: Not enough memory for updateMessage allocation\n");
    exit(EXIT_ERROR);
  }

  while (read(updatesFifo, updateMessage, MAX_MSG_LEN) > 0) {
    tag = strtok(updateMessage, ":");
    type = strtok(NULL, ":");
    val = strtok(NULL, ":");

    if (DEBUG) {
      printf("updating tag: %s, type: %s, val: %s\n", tag, type, val);
    }

    updateModelParameterValue(tag, val);

    memset(updateMessage, 0, (MAX_MSG_LEN * sizeof(char)));
  }

  if (NULL != updateMessage) {
    free(updateMessage);
  }
  
}


char* getModelSignalValue(const char* match)
{
  uint_T signalIdx;
  const char* signalPath;

  uint16_T dataTypeIdx;
  uint8_T slDataId;

  uint16_T fixedPointMapIdx;
  real_T slope = 1.0;
  real_T bias = 0.0;

  uint_T addressIdx;
  void* signalAddress;

  char* dto = (char*)calloc(MAX_MSG_LEN, sizeof(char));
  char* dtoPtr = dto;
  if (NULL == dto) {
    printf("Fatal: Not enough memory for next dto allocation\n");
    exit(EXIT_ERROR);
  }

  for (signalIdx = 0; signalIdx < numModelSignals; signalIdx++) {
    signalPath = rtwCAPI_GetSignalBlockPath(modelSignals, signalIdx);
    if (NULL != strstr(signalPath, match)) {
      break;
    }
  }

  dataTypeIdx = rtwCAPI_GetSignalDataTypeIdx(modelSignals, signalIdx);
  slDataId = rtwCAPI_GetDataTypeSLId(dataTypeMap, dataTypeIdx);

  fixedPointMapIdx = rtwCAPI_GetSignalFixPtIdx(modelSignals, signalIdx);
  if (fixedPointMapIdx > 0) {
    real_T fracSlope = rtwCAPI_GetFxpFracSlope(fixedPointMap, fixedPointMapIdx);
    int8_T exponent = rtwCAPI_GetFxpExponent(fixedPointMap, fixedPointMapIdx);

    slope = fracSlope * pow(2.0, exponent);
  }

  addressIdx = rtwCAPI_GetSignalAddrIdx(modelSignals, signalIdx);
  signalAddress = (void *)rtwCAPI_GetDataAddress(dataAddressMap, addressIdx);
  if (NULL == signalAddress) {
    printf("Fatal: Signal Data Address is NULL\n");
    exit(EXIT_ERROR);
  }

  dtoPtr += sprintf(dtoPtr, "%s:", match);
  if (DEBUG_VERBOSE) {
    printf("%s:", match);
  }

  switch (slDataId) {
    case SS_BOOLEAN:
      dtoPtr += sprintf(dtoPtr, "BOOLEAN:%d", *((boolean_T *)signalAddress));
      if (DEBUG_VERBOSE) {
        printf("BOOLEAN:%d\n", *((boolean_T *)signalAddress));
      }
      break;
    case SS_DOUBLE:
      dtoPtr += sprintf(dtoPtr, "DOUBLE:%f", *((real_T *)signalAddress));
      if (DEBUG_VERBOSE) {
        printf("DOUBLE:%f\n", *((real_T *)signalAddress));
      }
      break;
    default:
      printf("Warn: UNHANDLED TYPE - unknown value type logic needed \n");
  }

  return dto;
}


void updateModelParameterValue(const char* match, char* newValue)
{

  uint_T parameterIdx;
  bool parameterFound;
  const char* parameterPath;

  uint16_T dataTypeIdx;
  uint8_T slDataId;

  uint_T addressIdx;
  void* parameterAddress;

  real_T* doubleParam;
  double newDoubleParam;
  boolean_T* boolParam;
  bool newBoolParam;

  /* TODO: can optimize here by storing the Idxs in publishPoints with tag
   * - As long as we can be sure they won't change over time */
  parameterFound = false;
  for (parameterIdx = 0; parameterIdx < numModelParameters; parameterIdx++) {
    parameterPath = rtwCAPI_GetBlockParameterBlockPath(modelParameters, parameterIdx);
    if (NULL != strstr(parameterPath, match)) {
      parameterFound = true;
      break;
    }
  }

  if (!parameterFound) {
    if (DEBUG) {
      printf("%s parameter not found, no updates will be performed\n", match);
    }
    return;
  }

  dataTypeIdx = rtwCAPI_GetBlockParameterDataTypeIdx(modelParameters, parameterIdx);
  slDataId = rtwCAPI_GetDataTypeSLId(dataTypeMap, dataTypeIdx);

  addressIdx = rtwCAPI_GetBlockParameterAddrIdx(modelParameters, parameterIdx);
  parameterAddress = (void *)rtwCAPI_GetDataAddress(dataAddressMap, addressIdx);
  if (NULL == parameterAddress) {
    printf("Fatal: Parameter Data Address is NULL\n");
    exit(EXIT_ERROR);
  }

  if (DEBUG) {
    printf("%s:", parameterPath);
  }

  switch (slDataId) {
    case SS_DOUBLE:
      doubleParam = (real_T *)parameterAddress;
      newDoubleParam = strtod(newValue, NULL);
      if (errno == ERANGE) {
        printf("Error: could not convert '%s' to a double", newValue);
      }
      if (DEBUG) {
        printf("DOUBLE:%f->%f\n", *doubleParam, newDoubleParam);
      }
      *doubleParam = newDoubleParam;
      break;
    case SS_BOOLEAN:
      boolParam = (boolean_T *)parameterAddress;
      newBoolParam = strtol(newValue, NULL, 2);
      if (errno == ERANGE) {
        printf("Error: could not convert '%s' to a boolean", newValue);
      }
      if (DEBUG) {
        printf("BOOLEAN:%d->%d\n", *boolParam, newBoolParam);
      }
      *boolParam = newBoolParam;
      break;
    default:
      printf("Warn: UNHANDLED TYPE - unknown value type logic needed \n");
  }
}



/* ***************************
 * Start Shared Memory Helpers
 *************************** */
int createSharedMemory(key_t key, int bytes)
{
  int shmid = shmget(key, bytes, IPC_CREAT | 0666);
  if (shmid < 0) {
    printf("Fatal: Failed to get shared memory for key %d\n", key);
    exit(EXIT_ERROR);
  }

  printf("Info: Created shared memory with key %d\n", key);
  return shmid;
}


char* attachSharedMemory(int shmid)
{
  char *mem = (char*)shmat(shmid, NULL, 0);
  if (mem == (char*)-1){
    printf("Fatal: Failed to attach to shared memory\n");
    exit(EXIT_ERROR);
  }

  printf("Info: Attached to shared memory\n");
  return mem;
}


void destroySharedMemory(int shmid, char* shmaddr)
{
  shmdt(shmaddr);
  shmctl(shmid, IPC_RMID, NULL);

  printf("Info: Destroyed shared memory\n");
}



/* *******************
 * Start Misc. Helpers
 ******************* */
void readFileToMemory(const char* fileName, char** *array, unsigned int *num)
{
  FILE* fp = NULL;
  size_t lineLen = 0;
  ssize_t bytesRead = 0;
  int linesRead = 0;
  int k;

  fp = fopen(fileName, "r");
  if (NULL == fp) {
    printf("Fatal: Cannot open/find file: %s\n", fileName);
    exit(EXIT_ERROR);
  }

  *array = (char **)malloc(sizeof(char*) * (*num));
  if (NULL == *array) {
    printf("Fatal: Cannot allocate space for memory array\n");
    exit(EXIT_ERROR);
  }

  for (k = 0; k < *num; k++) {
    (*array)[k] = (char*)malloc(sizeof(char*) * MAX_MSG_LEN);;
    if (NULL == (*array)[k]) {
      printf("Fatal: Cannot allocate space for memory subarray\n");
      exit(EXIT_ERROR);
    }
  }

  while ((bytesRead = getline(&(*array)[linesRead], &lineLen, fp)) != -1) {
    if (linesRead >= *num) {
      /* Allocate more space in the array */
      *num = *num * 2;
      *array = (char **)realloc(*array, sizeof(char*) * *num);
      if (NULL == *array) {
        printf("Fatal: Cannot re-allocate space for more memory");
        exit(EXIT_ERROR);
      }
    }

    (*array)[linesRead][bytesRead-1] = 0; /*remove newline character*/

    linesRead++;
  }

  *num = linesRead;

  fclose(fp);
}

void printGroundTruth()
{
  FILE *groundTruth;

  uint_T signalIdx;
  const char* signalPath;
  char* signalPathCopy[MAX_MSG_LEN];
  char* signalNamePtr;
  char* signalName;
  uint16_T dataTypeIdx;
  uint8_T slDataId;
  uint_T addressIdx;
  void* signalAddress;

  char* dto = (char*)calloc(MAX_MSG_LEN, sizeof(char));
  char* dtoPtr = dto;
  if(NULL == dto){
     printf("Fatal: Not enough memory for ground truth dto allocation\n");
     exit(EXIT_ERROR);
  }

  groundTruth = fopen(GROUND_TRUTH_FILE, "w");
  if (NULL == groundTruth) {
    printf("Fatal: Cannot create/truncate file: %s\n", GROUND_TRUTH_FILE);
    exit(EXIT_ERROR);
  }

  for (signalIdx = 0; signalIdx < numModelSignals; signalIdx++) {
    signalPath = rtwCAPI_GetSignalBlockPath(modelSignals, signalIdx);
    if (NULL == strstr(signalPath, GROUND_TRUTH_FILTER)) {
      /* Ground truth filter not matched, do not include signal in groundTruth */
      continue;
    }

    dataTypeIdx = rtwCAPI_GetSignalDataTypeIdx(modelSignals, signalIdx);
    slDataId = rtwCAPI_GetDataTypeSLId(dataTypeMap, dataTypeIdx);

    addressIdx = rtwCAPI_GetSignalAddrIdx(modelSignals, signalIdx);
    signalAddress = (void *)rtwCAPI_GetDataAddress(dataAddressMap, addressIdx);
    if (NULL == signalAddress) {
      printf("Fatal: Signal Data Address is NULL\n");
      exit(EXIT_ERROR);
    }

    /* strip off the full path info, leaving only signal name */
    strncpy(signalPathCopy, signalPath, MAX_MSG_LEN);
    signalNamePtr = strtok(signalPathCopy, "/");
    while(NULL != signalNamePtr){
      signalName = signalNamePtr;
      signalNamePtr = strtok(NULL, "/");
    }

    dtoPtr += sprintf(dtoPtr, "%s:", signalName);
    switch (slDataId) {
      case SS_BOOLEAN:
        dtoPtr += sprintf(dtoPtr, "BOOLEAN:%d", *((boolean_T *)signalAddress));
        break;
      case SS_DOUBLE:
        dtoPtr += sprintf(dtoPtr, "DOUBLE:%f", *((real_T *)signalAddress));
        break;
      default:
        printf("Warn: UNHANDLED CODE - unknown value type logic needed \n");
    }

    fprintf(groundTruth, "%s\n", dto);

    memset(dto, 0, (MAX_MSG_LEN * sizeof(char)));
    dtoPtr = dto;
  }

  fclose(groundTruth);
}


void printModelMemberCounts()
{
  uint_T numModelParams = rtwCAPI_GetNumModelParameters(modelMap);
  uint_T numBlockParams = rtwCAPI_GetNumBlockParameters(modelMap);
  uint_T numStates = rtwCAPI_GetNumStates(modelMap);
  uint_T numModelSignals = rtwCAPI_GetNumSignals(modelMap);

  printf("Number of Model Parameters %d\n", numModelParams);
  printf("Number of Block Parameters %d\n", numBlockParams);
  printf("Number of States %d\n", numStates);
  printf("Number of Signals %d\n", numModelSignals);
}

