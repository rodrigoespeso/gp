	
#include "IGP_library.h"

#ifdef USE_THREAD_ALLOC
#include "threadalloc.h"
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>

#include <unistd.h>
#include <sys/syscall.h>

#define gettid() syscall(SYS_gettid)
#define getpid() syscall(SYS_getpid)

//******************************************************************************************************
// Modified by Juan Fº Sanjuan Estrada: NOTE: THREARE ARE TWO STRATEGIES TO BE PROGRAMMED:
// 1st: EACH RUNNING THREAD VERIFIES IF THERE IS A FREE CPU BY EXECUTING THE SYSTEM CALL __NR_get_cpuidle. THE FIRST THREAD WHICH DETECTS A FREE CPU, IS DIVIDED. 
// ADVANTAGE: THE MAIN PROGRAM DOESN'T NEED TO KNOW THE WORKLOAD OF HIS THREADS. 
// DISADVANTAGE: A THREAD WITH FEW WORKLOAD COULD BE DIVIDED. EMPHASIZED UNBALANCE OF LOAD.
// 2nd: THE MAIN PROGRAM RECEIVES AN S.O WARNING WHEN IT DETECTS AN IDDLE CPU. THE PROGRAM STUDIES THE WORKLOAD OF ALL THE THREADS AND IT DECIDES WHAT THREAD NEED TO BE DIVIDED. THE DECISION APPROACH IS THE THREAD WITH THE MAXIMUM WORKLOAD.
// ADVANTAGE: LOAD BALANCE IN COMPLIANCE WITH THE PROBLEM.
// DISADVANTAGE: THE MAIN PROGRAM REQUIRES TO KNOW ALL HIS THREADS.


struct IGP_Configuration IGP_Conf; 
size_t	total_memory=0;	// Total memory reservation
int num_total_allocators=0; 

// Variables used from productivity and power strategy
double *Productivity=NULL;
double *Power=NULL;
struct timeval last_time; 	// Shows the instant of time in which starts the interval of current analysis.
struct timeval start_time;  	// Shows the time in which the program starts his execution
bool Level; 			// 0 = Kernel level 1 = Aplication level

int num_max_active_threads=0;		// Maximum number of threads running simultaneously.
int num_analysis=0;					// Number of times that decision model has been executed
int num_evaluation_analysis=0; 		// Number of times that decision rule has been evaluated
int num_efective_analysis=0;		// Number of system call in which performance has been analysed.
unsigned long *jiffies_idle;		// In READ_FILE stores the number of idle jiffies of each cpu in the last reading
int NCpus; 							// Is only used in READ_FILE for now.
int id_proc_acs=-1;
bool creating_thread=false;
int next_id_thread=0;

bool Analyze=false;				// It notifies an analysis in process
bool Reset_analysis=false;		// It notifies that analysis parameters are being resetting.
bool Cancel_analysis=false;		// It notifies that the decision of the current analysis need to be cancelled
bool Report_to_loadest_thread=false;	// It notifies that GP is waiting the loadest thread to report that it can creates a new thread.

#define NONPRIORITY		0 		// All the applications have the same priority, if any application request monitoring will be attended.
#define	COMPLETELY_FAIR		1 	// The number of confirmations of all applications is counted, this way, all have the same number of positive confirmations. 
#define	PRIORITY		2 		// Only one application could be monitored, and until an application doesn't end another one could not be monitored.

// Manager levels
#define KERNEL			0
#define APPLICATION 		1

struct infothread {
	bool active;			// It reports if the thread is active
bool reserved;
	int pid;
	bool reported;			// It reports if the thread has reported his workload
	bool divide;			// It reports that this thread can create a new thread.
	unsigned long workload;  	// It stores the future workload
	unsigned long workdone; 	// It stores the work done by each thread between the previous and the next analysis
	unsigned long utime_thread; 	// It stores the number of jiffies that the thread has been executing since the previous analysis
} *ind;

struct timeval Interval_time;  	// It shows the time since the interval of analysis starts his measuring, in which a number of active threads remain fixed
struct timeval Inform_time; 	// It shows the instant of time in which the creation a of thread is positively reported
unsigned long created_time;		// It shows the instant of time since the creation a of thread is positively reported until the creation of the thread
unsigned long *ThreadsCount;  	// It count the time in which a number of active threads has been running.
unsigned long analysis_time=0;

pthread_mutex_t mutex_analyze_eficiency=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_end_thread=PTHREAD_MUTEX_INITIALIZER;

// Checks if a string is an integer
bool is_Integer(const char* str){
  	char *endptr;
 	strtol(str, &endptr, 10);

        if(*endptr != '\n' || endptr == str)
                return false;	

        return true;
}

// Checks if a string is a double
bool is_Double(const char* str){
        char* endptr;
        strtod(str, &endptr);

        if(*endptr != '\n' || endptr == str)
                return false;
        return true;
}

bool igp_set_default_config(){
	bool recovered = false;
	FILE *file;

	// Default config file template
	file = fopen("IGP_config", "w");
	if (file==NULL){
		fprintf(stderr, "[IGP_Set_Default_Config] Error opening config file!\n");
		exit(1);
	}

	fprintf(file, "//                                              Configuration file of the IGP library.\n");
	fprintf(file, "//                                             IGP (Interfaz del Gestor de Paralelismo)\n");
	fprintf(file, "// The method used by parallelism manager to decide when multithreaded application has to create a new thread.\n");
	fprintf(file, "// The following methods have been built, where the parallelism manager decides to create news threads,\n");
	fprintf(file, "// METHOD / APPROACH: NON ADAPTIVE METHOD\n");
	fprintf(file, "//   0    /    0    : if total number of active threads is less than MAX_THREADS.\n");
	fprintf(file, "// METHOD / APPROACH: ADAPTIVE METHODS AT APPLICATION LEVEL\n");
	fprintf(file, "//   1    /    0    : if the application performance with n threads measured by parallelism manager in last time between the\n");
	fprintf(file, "//                  : application performance with n-1 threads is better than a THRESHOLD chosen and total number of active\n");
	fprintf(file, "//                  : threads is less than MAX_THREADS. The THRESHOLD value is between 0 and 1.\n");
	fprintf(file, "//   2    /    0    : if last PERIOD between idle average time of all cpus in this interval time is higher than THRESHOLD\n");
	fprintf(file, "//                  : chosen and total number of active threads is less than MAX_THREADS. The idle time of cpus are reading\n");
	fprintf(file, "//                  : from /proc/stat file. The THRESHOLD value is between 0 and 100.\n");
	fprintf(file, "//   2    /    1    : if sleeping time of all threads in the last PERIOD is null and total number of active threads is less than\n");
	fprintf(file, "//                  : MAX_THREADS. The sleeping time of threads are reading form /proc/[id_pid]/task/[id_tid]/stat files.\n");
	fprintf(file, "//   2    /    2    : if Phi temperature less fan-in temperature doesn`t increase a THRESHOLD chosen and total number\n");
	fprintf(file, "//                  : of active threads is less than MAX_THREADS. The Phi and fan-in temperatures are reading from /sys/class/\n");
	fprintf(file, "//                  : micras/temp file. The THRESHOLD value is between 0 and 100.\n");
	fprintf(file, "//   2    /    3    : if Phi power running multithreaded application less phi power in idle state doesn’t increase a THRESHOLD\n");
	fprintf(file, "//                  : chosen and total number of active threads is less than MAX_THREADS. The Phi power is reading from /sys/\n");
	fprintf(file, "//                  : class/micras/temp file. The THRESHOLD value is between 0 and 100.\n");
	fprintf(file, "//   3    /    0    : if the application power is under a POWER THRESHOLD and the total_work hasn't decreased\n");
	fprintf(file, "//                  : The THRESHOLD value is between 0 and 1.\n");
	fprintf(file, "// METHOD / APPROACH: ADAPTIVE METHODS AT KERNEL LEVEL\n");
	fprintf(file, "// 3,4,5  /   0-5   : Kernel-level methods.\n");
	fprintf(file, "METHOD=3\n");
	fprintf(file, "APROACH=0\n");
	fprintf(file, "// The Max_Threads is a maximum number of running threads at the same time.\n");
	fprintf(file, "MAX_THREADS=100\n");
	fprintf(file, "// Threshold used in application performance and power method.\n");
	fprintf(file, "WORK_THRESHOLD=2.0\n");
	fprintf(file, "// Threshold used in application power method\n");
	fprintf(file, "POWER_THRESHOLD=1.0\n");
	fprintf(file, "// Period is a time interval (ms) between two consecutive samples to read the manager decision\n");
	fprintf(file, "PERIOD=0.00\n");
	fprintf(file, "// Manager shows times and information by console.\n");
	fprintf(file, "TIMING=yes\n");
	fprintf(file, "// Manager sleeps running threads if it´s neccesary.\n");
	fprintf(file, "DETENTION=no\n");
	fprintf(file, "// Main process resolves workload like a thread\n"); // Las hebras pueden crear hebras
	fprintf(file, "PROCESS_IS_A_THREAD=no\n");
	fprintf(file, "// Main process uses loadmax as approach or the first thread\n");
	fprintf(file, "MAXLOAD=yes\n\n");

	fclose(file);

	fprintf(stderr, "[IGP_Set_Default_Config] Default configuration file created (IGP_config)\n");

	return true;
}

// ****************************************************************
// Author: Alberto Morante Bailón
// Change date: 17/02/2018
// The configuration values are read from the file IGP_config
// ****************************************************************
bool igp_read_config(){
        char line[2048];
	bool recovered = false;

        FILE *file;

	fprintf(stderr, "[IGP_Read_Config] Reading configuration file (IGP_config)\n");	

        file = fopen("IGP_config", "r");
        if (file == NULL){
		igp_set_default_config();
        	file = fopen("IGP_config", "r");
        }

	char parameters[10][20] = {"MAX_THREADS", "WORK_THRESHOLD", "METHOD", "PERIOD", "APROACH", "TIMING", "DETENTION", "PROCESS_IS_A_THREAD", "MAXLOAD", "POWER_THRESHOLD"};
	
	int size = sizeof(parameters)/sizeof(*parameters);
	
	bool found[size];
	int i;
	for (i=0; i<size; i++)
		found[i]=false;

	int findparam = 0;
	while (feof(file) == 0){
		// ANY parameter or comment is read
		fgets(line, 200, file);	

      		char delimiter[] = "=";
        	char *tokens;
		char *parameter[2];
		
		tokens = strtok(line, delimiter);
		parameter[0]=tokens;		
		tokens = strtok(NULL, "\0");
		parameter[1]=tokens;

		if (strcmp(parameter[0], "") != 0){
			for (i=0; i<size;i++){
				if (!strcmp(parameter[0], parameters[i]) && found[i] == false){
					found[i]=true;
					findparam++;
					switch(i) {
						case 0:
							if (!is_Integer(parameter[1])){
								fprintf(stderr, "[IGP_Read_Config] ERROR: MAX_THREADS must be an integer value. Edit the file IGP_config.\n");
								exit(1);
							}
							IGP_Conf.Max_Threads = atoi(parameter[1]);
							
							fprintf(stderr, "[IGP_Read_Config] MAX_THREADS VALUE: %d\n", IGP_Conf.Max_Threads);
							break;
						case 1:
							if (!is_Double(parameter[1])){
								fprintf(stderr, "[IGP_Read_Config] ERROR: WORK_THRESHOLD must be a double value. Edit the file IGP_config.\n");
								exit(1);
							}
							IGP_Conf.Work_Threshold = atof(parameter[1]);

							fprintf(stderr, "[IGP_Read_Config] Work Threshold: %f\n", IGP_Conf.Work_Threshold);
							break;
						case 2:
							if (!is_Integer(parameter[1])){
								fprintf(stderr, "[IGP_Read_Config] ERROR: METHOD must be an integer value. Edit the file IGP_config.\n");
								exit(1);
							}
							IGP_Conf.Method = atoi(parameter[1]);

							fprintf(stderr, "[IGP_Read_Config] Method: %d\n", IGP_Conf.Method);
							break;
						case 3:
							if (!is_Double(parameter[1])){
								fprintf(stderr, "[IGP_Read_Config] ERROR: PERIOD must be an integer value. Edit the file IGP_config.\n");
								exit(1);
							}
							IGP_Conf.Period = atof(parameter[1]);

							break;
						case 4:
							if (!is_Integer(parameter[1])){
								fprintf(stderr, "[IGP_Read_Config] ERROR: APROACH must be an integer value. Edit the file IGP_config.\n");
								exit(1);
							}
							IGP_Conf.Approach = atoi(parameter[1]);

							break;
						case 5:
							if (strcmp(parameter[1],"yes\n")!=0 && strcmp(parameter[1], "no\n")!=0){
								fprintf(stderr, "[IGP_Read_Config] ERROR: TIMING must be 'yes' or 'no'\n");
								exit(1);
							}
							if(!strcmp(parameter[1],"yes\n")) 
								IGP_Conf.Timing = true; 
							else 
								IGP_Conf.Timing = false;

//							fprintf(stderr, "[IGP] Timing measure: %s\n", IGP_Conf.Timing);
							break; 
						case 6:
							if (strcmp(parameter[1],"yes\n")!=0 && strcmp(parameter[1], "no\n")!=0){
								fprintf(stderr, "[IGP_Read_Config] ERROR: DETENTION must be 'yes' or 'no'\n");
								exit(1);
							}
							if(!strcmp(parameter[1],"yes\n")) 
								IGP_Conf.Detention = true; 
							else 
								IGP_Conf.Detention = false;

//							fprintf(stderr, "[IGP] DETENTION: %s\n", IGP_Conf.Detention);
							break;

						case 7:
							if (strcmp(parameter[1],"yes\n")!=0 && strcmp(parameter[1], "no\n")!=0){
								fprintf(stderr, "[IGP_Read_Config] ERROR: PROCESS IS A THREAD must be 'yes' or 'no'\n");
								exit(1);
							}
							if(!strcmp(parameter[1],"yes\n")) 
								IGP_Conf.Process_is_a_thread = true; 
							else 
								IGP_Conf.Process_is_a_thread = false;

//							fprintf(stderr, "[IGP] PROCESS_IS_A_THREAD: %s\n", IGP_Conf.Detention);
							break;
						case 8:
							if (strcmp(parameter[1],"yes\n")!=0 && strcmp(parameter[1], "no\n")!=0){
								fprintf(stderr, "[IGP_Read_Config] ERROR: MAXLOAD must be 'yes' or 'no'\n");
								exit(1);
							}
							if(!strcmp(parameter[1],"yes\n")) 
								IGP_Conf.Maxload = true; 
							else 
								IGP_Conf.Maxload = false;

//							fprintf(stderr, "[IGP_Read_Config] PROCESS_IS_A_THREAD: %s\n", IGP_Conf.Maxload);
							break;

						case 9:
							if (!is_Double(parameter[1])){
								fprintf(stderr, "[IGP_Read_Config] ERROR: POWER_THRESHOLD must be a double value. Edit the file IGP_config.\n");
								exit(1);
							}
							IGP_Conf.Power_Threshold = atof(parameter[1]);

//							fprintf(stderr, "[IGP_Read_Config] Power Threshold: %f\n", IGP_Conf.Power_Threshold);
							break;

						default:
							break;
					}
					break;							
				}
			}
		}
	}

	// Check if any parameter is missing
	findparam = size - findparam;
	char msg[50] = "[IGP_Read_Config] Parameter";

	if (findparam>0){
		if (findparam>1)
			strcat(msg, "s");		
		
		strcat(msg, " ");
		
		for (i=0; i<size; i++){
			if (!found[i]){
				strcat(msg, parameters[i]);
				if (findparam > 2)
					strcat(msg, ", ");
				if (findparam == 2)
					strcat(msg, " & ");
				findparam--;			
			}
		}
		strcat(msg, " not found.\n");
		fprintf(stderr, "%s\n", msg);
		recovered = igp_set_default_config();
		return recovered;
	}	

	// Check if the values are right
	if (IGP_Conf.Max_Threads<1 || IGP_Conf.Max_Threads>2050){
		fprintf(stderr, "[IGP_Read_Config] ERROR: MAX_THREADS must be a value between 1 and 1000.\n");
		exit(1);
	}

	if (IGP_Conf.Work_Threshold<0 || IGP_Conf.Work_Threshold>300){
		fprintf(stderr, "[IGP_Read_Config] ERROR: THRESHOLD must be a value between 0 and 300.\n");
		exit(1);
	}

	if (IGP_Conf.Power_Threshold<0 || IGP_Conf.Power_Threshold>100){
		fprintf(stderr, "[IGP_Read_Config] ERROR: THRESHOLD must be a value between 0 and 100.\n");
		exit(1);
	}

	if (IGP_Conf.Method<0 || IGP_Conf.Method>6){
		fprintf(stderr, "[IGP_Read_Config] ERROR: METHOD must be a value between 0 and 5.\n");
		exit(1);
	}

	if (IGP_Conf.Period<0.0 || IGP_Conf.Period>10000.0){
		fprintf(stderr, "[IGP_Read_Config] ERROR: PERIOD must be a value between 0.0 and 10000.0.\n");
		exit(1);
	}

	if (IGP_Conf.Approach<0 || IGP_Conf.Approach>21){
		fprintf(stderr, "[IGP_Read_Config] ERROR: APROACH must be a value between 0 and 21.\n");
		exit(1);
	}

        fclose(file);

	return false;
}

unsigned long Read_Power_File(){
	FILE *file;
        char line[20];
	char *str;
	unsigned long output;
	int i;
	char *token;
	char delimiter[] = " ";

        file = fopen("/sys/class/micras/power", "r");
        if (file == NULL)
        	file = fopen("/sys/class/micras/power", "r");
	
	// Window 0
	fgets(line, 20, file);

	// Window 1
	fgets(line, 20, file);	

	// Instant power
	fgets(line, 20, file);

	for (i=1; i<6; i++)
		fgets(line, 20, file);

	token = strtok(line, delimiter);
	output = strtol(token, &str, 10);

	fclose(file);	

	return output;    	
}

// Only the main process executes it
void IGP_Initialice() {
	int i;
	size_t size, powerSize;
	int detencion = 0;	// Añadido por Rodrigo Epseso						
	int piat = -1;
	int mxld = -1;
	int modificar_p = 0;	//

	if(!IGP_Conf.Timing) 
		gettimeofday(&start_time, NULL); // We store the instant of time in which the program starts his execution

	// ****************************************************************
        // Author: Alberto Morante Bailón
        // Change date: 31/05/2017
        // The configuration values are read from the file IGP_config
	// ****************************************************************

	bool recovered = igp_read_config();

	if (recovered)
		igp_read_config();

	fprintf(stderr, "[IGP_Initialice] Initial configuration of the IGP: METHOD: %d APPROACH: %d MAX_THREADS: %d  PERIOD: %f  WORK_THRESHOLD: %f POWER_THRESHOLD: %f\n", IGP_Conf.Method, IGP_Conf.Approach, IGP_Conf.Max_Threads, IGP_Conf.Period, IGP_Conf.Work_Threshold, IGP_Conf.Power_Threshold);

	if(IGP_Conf.Timing) 
		fprintf(stderr, "[IGP_Initialice] TIMING: yes\n"); 
	else 
		fprintf(stderr, "[IGP_Initialice] TIMING: no\n");

	if(IGP_Conf.Detention) 
		fprintf(stderr, "[IGP_Initialice] DETENTION: yes\n"); 
	else 
		fprintf(stderr, "[IGP_Initialice] DETENTION: no\n");

	if(IGP_Conf.Process_is_a_thread) 
		fprintf(stderr, "[IGP_Initialice] PROCESS_IS_A_THREAD: yes\n"); 
	else 
		fprintf(stderr, "[IGP_Initialice] PROCESS_IS_A_THREAD: no\n");

	if(IGP_Conf.Maxload) 
		fprintf(stderr, "[IGP_Initialice] MAXLOAD: yes\n"); 
	else 
		fprintf(stderr, "[IGP_Initialice] MAXLOAD: no\n");

	switch(IGP_Conf.Method) {	
		case NONADAPTABLE:
			Level=APPLICATION;
			fprintf(stderr, "[IGP_Initialice] NONADAPTABLE: First thread entering the critical section of testing is the first which is divided, if there are not %d threads running simultaneously.\n", IGP_Conf.Max_Threads);
			break;
		case ACW: 
			Level=APPLICATION;
			fprintf(stderr, "[IGP_Initialice] ACW: A thread periodically compares the productivity of n threads with the obtained with n-1 threads, with a threshold of %f.\n", IGP_Conf.Work_Threshold);
			if(IGP_Conf.Work_Threshold<=0 || IGP_Conf.Work_Threshold>1) {
				fprintf(stderr, "[IGP_Initialice] The threshold must have a value between 0.0 and 1.0.\n");
				exit(1);
			} 
			break;
		case AST: 
			Level=APPLICATION;
			fprintf(stderr, "[IGP_Initialice] READ_FILE: The process reads from /proc/stats, the iddle states of the cpus.\n"); 
			break;
		case APW:
			Level=APPLICATION;
			fprintf(stderr, "[IGP_Initialice] READ_FILE: The process reads from /sys/class/micras/power the power of the cpus.\n");
			if(IGP_Conf.Work_Threshold<0.0 || IGP_Conf.Work_Threshold>200.0) {
				fprintf(stderr, "[IGP_Initialice] The work threshold must have a value between 0.0 and 200.0.\n");
				exit(1);
			} 
			if(IGP_Conf.Power_Threshold<0.5 || IGP_Conf.Power_Threshold>2.0) {
				fprintf(stderr, "[IGP_Initialice] The power threshold must have a value between 0.5 and 2.0.\n");
				exit(1);
			} 
			break;
		case KST: 
			Level=KERNEL;
			fprintf(stderr, "[IGP_Initialice] KST: The analysis is performed by each thread when it makes a system call get_cpuidle().\n"); 
			break;
		case KITST: 
			Level=KERNEL;
			fprintf(stderr, "[IGP_Initialice] KITST: The analysis is performed by the idle thread of the kernel.\n"); 
			break;
		case SST: 
			Level=KERNEL;
			fprintf(stderr, "[IGP_Initialice] SST: The analysis is performed by the kernel scheduler.\n"); 
			break;

		default:
			fprintf(stderr, "[IGP_Initialice] ERROR: Unknown method.\n");
			exit(1);
	}

	if(Level==KERNEL) {

		switch(IGP_Conf.Approach) {
			case TEST_APPLICATION: 
				fprintf(stderr, "[IGP_Initialice] TEST_APPLICATION: It allows to test the application working for a different number of active threads.\n"); 
				break;
			case KERNEL_NONADAPTABLE: 
				fprintf(stderr, "[IGP_Initialice] KERNEL_NONADAPTABLE: It always allows to divide\n"); 
				break;
			case NOSLEEPTHREAD: 
				fprintf(stderr, "[IGP_Initialice] NOSLEEPTHREAD: It creates a new thread if any has been recently blocked.\n"); 
				break; 
			case NO_IBT: 
				fprintf(stderr, "[IGP_Initialice] NO_IBT: \n"); 
				break;
			case NO_NIBT: 
				fprintf(stderr, "[IGP_Initialice] NO_NIBT: \n"); 
				break;
			case NO_WAITING: 
				fprintf(stderr, "[IGP_Initialice] NO_WAITING: \n"); 
				break;
			case SLEEPTIMEvsRUNTIME: 
				fprintf(stderr, "[IGP_Initialice] SLEEPTIMEvsRUNTIME: It creates a new thread if the maximum time that a thread sleeps doesn't exceed the 50%% of the time interval between two checks (TODO(Alberto): Debug).\n"); 
				break; 
			case SLEEPTIMEvsINTERVAL: 
				fprintf(stderr, "[IGP_Initialice] SLEEPTIMEvsINTERVAL\n"); 
				break; 
			case SLEEPTIMEvsINI_INTERVAL: 
				fprintf(stderr, "[IGP_Initialice] SLEEPTIMEvsINI_INTERVAL\n"); 
				break; 
			case MNT_IBTvsRUNTIME: 
				fprintf(stderr, "[IGP_Initialice] MNT_IBTvsRUNTIME: It creates a new thread if the block time of the threads doesn't exceed the 50%% of the interval of time of an average iteration.\n"); 
				break; 
			case MNT_IBTvsINTERVAL: 
				fprintf(stderr, "[IGP_Initialice] MNT_IBTvsINTERVAL: ....\n"); 
				break;
			case MNT_IBTvsINI_INTERVAL: 
				fprintf(stderr, "[IGP_Initialice] MNT_IBTvsINI_INTERVAL: ....\n"); 
				break;
			case MNT_NIBTvsRUNTIME: 
				fprintf(stderr, "[IGP_Initialice] MNT_NIBTvsRUNTIME: ....\n"); 
				break;
			case MNT_NIBTvsINTERVAL: 
				fprintf(stderr, "[IGP_Initialice] MNT_NIBTvsINITERVAL: ....\n"); 
				break;
			case MNT_NIBTvsINI_INTERVAL: 
				fprintf(stderr, "[IGP_Initialice] MNT_NIBTvsINI_INITERVAL: ....\n"); 
				break;
			case MNT_WAITINGvsRUNTIME: 
				fprintf(stderr, "[IGP_Initialice] MNT_WAITINGvsRUNTIME: ....\n"); 
				break;
			case MNT_WAITINGvsINTERVAL: 
				fprintf(stderr, "[IGP_Initialice] MNT_WAITINGvsINTERVAL: ....\n"); 
				break;
			case MNT_WAITINGvsINI_INTERVAL: 
				fprintf(stderr, "[IGP_Initialice] MNT_WAITINGvsINI_INTERVAL: ....\n"); 
				break;
			case MNT_BTvsRUNTIME: 
				fprintf(stderr, "[IGP_Initialice] MNT_BTvsRUNTIME: ....\n"); 
				break;
			case MNT_BTvsINTERVAL: 
				fprintf(stderr, "[IGP_Initialice] MNT_BTvsINITERVAL: ....\n"); 
				break;
			case MNT_BTvsINI_INTERVAL: 
				fprintf(stderr, "[IGP_Initialice] MNT_BTvsINI_INITERVAL: ....\n"); 
				break;
			case PREDICTOR: 
				fprintf(stderr, "[IGP_Initialice] PREDICTOR: TODO: Implementation\n"); 
				break;
			case CPUIDLE: 
				fprintf(stderr, "[IGP_Initialice] CPUIDLE: It creates a new thread if any cpu has been idle.\n"); 
				break;
			default: 
				fprintf(stderr, "[IGP_Initialice] Approach must be TEST_APPLICATION, KERNEL_NONADAPTABLE, NOSLEEPTHREAD, NO_IBT, NO_NIBT, NO_WAITING, SLEEPTIMEvsRUNTIME, SLEEPTIMEvsINTERVAL, SLEEPTIMEvsINI_INTERVAL, MNT_IBTvsRUNTIME, MNT_IBTvsINTERVAL, MNT_IBTvsINI_INTERVAL, MNT_NIBTvsRUNTIME, MNT_NIBTvsINTERVAL, MNT_NIBTvsINI_INTERVAL, MNT_WAITINGvsRUNTIME, MNT_WAITINGvsINTERVAL, MNT_WAITINGvsINI_INTERVAL, PREDICTOR, CPUIDLE.\n");
				exit(1);
		}
	}

	switch(IGP_Conf.Method) {
		case NONADAPTABLE: 
			break;
		case ACW: // The own application autoregulates itself keeping the level of productivity over a threshold.
			size=sizeof(double)*IGP_Conf.Max_Threads;
#ifdef USE_THREAD_ALLOC
			fprintf(stderr, "[IGP_Initialice] Thread_alloc used.\n");
			Productivity=(double *)thread_alloc(size);
#else
			fprintf(stderr, "[IGP_Initialice] Malloc used.\n");
			Productivity=(double *)malloc(size);
			if(Productivity==NULL){ 
				fprintf(stderr, "[IGP_Initialice] ERROR: Failure reserving dinamic memory.\n"); 
				exit(1); 
			}
#endif
			break;
		case AST: // The process reads from /proc/stats the idle states of the cpu.
			ReadStatFile(1);
			break; 
		case APW: // The own application autoregulates itself according to a power and productivity threshold.
			size=sizeof(double)*IGP_Conf.Max_Threads;
#ifdef USE_THREAD_ALLOC
			fprintf(stderr, "[IGP_Initialice] Thread_alloc used.\n");
			Productivity=(double *)thread_alloc(size);
			Power=(double *)thread_alloc(size);
#else
			fprintf(stderr, "[IGP_Initialice] Malloc used.\n");
			Productivity=(double *)malloc(size);
			Power=(double *)malloc(size);
			if(Productivity==NULL || Power==NULL){ 
				fprintf(stderr, "[IGP_Initialice] ERROR: Failure reserving dinamic memory.\n"); 
				exit(1); 
			}
#endif
			break;
		case KST: // We report the S.O to report us each time a new thread can be created
		case KITST:
		case SST:
			if (IGP_Conf.Process_is_a_thread) piat = 1;
			else piat = 0;

			if (IGP_Conf.Maxload) mxld = 1;
			else mxld = 0;
													// Añadido por Rodrigo Espeso.
													// Para que la syscall reciba int en lugar de bool.
			if (IGP_Conf.Detention) detencion = 1;	//
			else detencion = 0;					//
													// Modificado por Rodrigo Espeso. Añadidos IGP_Conf.Process_is_a_thread, IGP_Conf.Maxload.
			id_proc_acs=syscall(__NR_warns_status, IGP_Conf.Method, IGP_Conf.Approach, detencion, IGP_Conf.Max_Threads, piat, mxld/*, modificar_p*/);	
			if(id_proc_acs==-1) { 
				fprintf(stderr, "[IGP_Initialice] ERROR: It can't be executed, the scheduler cannot asign it a monitoring code.\n"); 
				exit(1); 
			}
			break;

		default: 
			fprintf(stderr, "[IGP_Initialice] ERROR: Only methods METHOD: 0 - NONAUTOADPATABLE, 1 - ACW, 2 - AST , 3 - APW , 4 - Señales, 5 - KST, y 6 - KITST has been programmed.\n");
			exit(1); 
			break;
	}	
	if(Level==APPLICATION || Level == KERNEL) {	// Modificado por Rodrigo Espeso +KERNEL
		if(IGP_Conf.Process_is_a_thread)
			size=sizeof(struct infothread)*(IGP_Conf.Max_Threads+1);
		else 
//			size=sizeof(struct infothread)*IGP_Conf.Max_Threads;
			size=sizeof(struct infothread)*(IGP_Conf.Max_Threads+1);
#ifdef USE_THREAD_ALLOC
		ind=(struct infothread *)thread_alloc(size);
#else
		ind=(struct infothread *)malloc(size);
		if(ind==NULL)  { 
			fprintf(stderr, "[IGP_Initialice] ERROR: Failure reserving dinamic memory.\n"); 
			exit(1); 
		}
#endif
		Reset_data(true);
		Analyze=false;
		Cancel_analysis=false;

		if(!IGP_Conf.Process_is_a_thread) { // Añadido nuevo para permitir que el proceso principal cree todas las hebras
				if(ind[0].active) {
					fprintf(stderr, "[IGP_Initialice] ERROR: the identifier %d is already enabled.\n", 0);
					exit(1);
				}
				ind[0].active=ind[0].reserved=true;
//				active_num_threads=Reset_data(false);
				ind[0].pid=(long int)syscall(__NR_gettid);
				ind[0].workload=0;
				ind[0].workdone=0;
				ind[0].reported=true;
				ind[0].divide=true;	
		}
	}	

	creating_thread=false;
	if(!IGP_Conf.Timing) {
		// We reserve memory and the statistics of active threads is initialized
		size=sizeof(unsigned long)*IGP_Conf.Max_Threads;
#ifdef USE_THREAD_ALLOC
		ThreadsCount=(unsigned long *)thread_alloc(size);
#else
		ThreadsCount=(unsigned long *)malloc(size);
		if(ThreadsCount==NULL)  { 
			fprintf(stderr, "[IGP_Initialice] ERROR: Failure reserving dinamic memory.\n"); 
			exit(1); 
		}
#endif
		// The time count is initialized
		for(i=0; i< IGP_Conf.Max_Threads; i++)	
			ThreadsCount[i]=0;
	}
	gettimeofday(&Inform_time,NULL);
}
	
// Only the main process executes it
void IGP_Finalice() {
	unsigned long dif_time, dif_usecs, total_time;
	struct timeval end_time;
	
	if(Level==KERNEL) 
		syscall(__NR_thread_end, 1);  // The application ending is reported and no longer it needs to be monitored
	
	fprintf(stderr, "[IGP_Finalice] Final statistic of the self adapted process.\n");
	if(!IGP_Conf.Timing) {
		gettimeofday(&end_time,NULL); // We store the time in which the program starts his execution
		total_time = (end_time.tv_sec-start_time.tv_sec) * 1000000 + (end_time.tv_usec-start_time.tv_usec);
		dif_usecs=dif_time=0;

		int i;
		for(i=0; i<IGP_Conf.Max_Threads; i++) {
			dif_usecs=dif_usecs+ThreadsCount[i]*(i+1);
			dif_time=dif_time+ThreadsCount[i];
		}
		fprintf(stderr, "[IGP_Finalice] Average number of threads:  %lu (X10).\n", 10 * dif_usecs/dif_time);
		fprintf(stderr, "[IGP_Finalice] Total time: %llu ms\n", total_time);
	}
	fprintf(stderr, "[IGP_Finalice] Analysis time: %lu ms\n", analysis_time);
	fprintf(stderr, "[IGP_Finalice] N. of executions of the decision model:  %d.\n", num_analysis);
	fprintf(stderr, "[IGP_Finalice] N. of evaluations of the decision rule:  %d.\n", num_evaluation_analysis);
	fprintf(stderr, "[IGP_Finalice] N. of positive analysis:  %d\n", num_efective_analysis);
	fprintf(stderr, "[IGP_Finalice] Maximum number of threads:  %d\n", num_max_active_threads); 

	if(Level==KERNEL){
		if(!IGP_Conf.Timing) {
			fprintf(stderr, "[IGP_Finalice] Total memory required: %lu.\n", total_memory/2048);
			fprintf(stderr, "[IGP_Finalice] Total number of reservations: %d.\n", num_total_allocators);
		}
	}
}

// All threads executes it sequentially.
// Return:
//	>0 = Number of threads that it´s possible to create.
//	-1 = The decision method reports that it isn´t possible to create a new thread.
//	-2 = GP waits because someone is creating a new thread.
//	-3 = GP waits because it doesn’t allow to create more threads (MAX_THREADS).
// 	-4 = GP waits to report the loadest thread to create a new thread.
//	-5 = GP resets current analysis. 
//	-6 = GP waits because all threads haven`t reported their workload. 
//	-7 = GP waits because interval time is shorter than PERIOD.
int IGP_Get(int id_thread, unsigned long trabajo_pendiente) { // Modificado por Rodrigo Espeso. Añadido parámetro trabajo_pendiente
	unsigned long workload=0;
//	int jump1=0, jump2=0;
//	fprintf(stderr, "[IGP_Get] id_thread.%d\n", id_thread);
//	fprintf(stderr, "[IGP_Get] workload.%lu wordone.%lu\n", workload, workdone);
	int total_work=0;
	int i, id_i, idnew=-1; 
	struct timeval now_time, start, end;
	double dif_time, Performance;
	struct timeval t1;
	double dif_usecs;
	char data_efficiency[2048];
	int new_id_thread=-1;
	int active_num_threads=0, reporting_num_threads=0;
	bool exit=false;
	
	// GP has noticed to create a new thread before, so GP is waiting a new thread.
	if(creating_thread) { 
		exit=true; 
		new_id_thread=-2; 
	}
	if(!exit) { // No creating thread
	    switch(Level) {
		case APPLICATION:
			//printf("\n[IGP_Get %d] No creating_thread \n", id_thread);		
			if(ind[id_thread].divide && !exit) { // This thread is marked because it can create a new thread
				new_id_thread=1;
				//printf("\n[IGP_Get %d] Bk. P. 1 \n", id_thread);		
				if(IGP_Conf.Process_is_a_thread || id_thread){
					ind[id_thread].divide=false;
					//printf("\n[IGP_Get %d] Bk. P. !2 \n", id_thread);		
					
				}
				exit=true;
				Report_to_loadest_thread=false;
				//printf("\n[IGP_Get %d] Bk. P. 3 \n", id_thread);		
			}

			if(Report_to_loadest_thread && !exit) { 
				exit=true; 
				new_id_thread=-4;
				//printf("\n[IGP_Get %d] Bk. P. !4 \n", id_thread);		

			} 
			if(Reset_analysis && !exit) { 
				exit=true; 
				new_id_thread=-5;
				//printf("\n[IGP_Get %d] Bk. P. !5 \n", id_thread);		
			}		

			if(!exit) {
				//printf("\n[IGP_Get %d] Bk. P. 6 !exit \n", id_thread);		

				// We mark the thread has reported his workload
				if(!ind[id_thread].reported){
					ind[id_thread].reported=true;
					//printf("\n[IGP_Get %d] Bk. P. ind[id_thread].reported=true \n", id_thread);		
				}
				if(pthread_mutex_trylock(&mutex_analyze_eficiency)!=EBUSY) {
					gettimeofday(&start,NULL);
					num_analysis++;
					Analyze=true;

					// We count the reporting and active threads
					for(i=0; i<IGP_Conf.Max_Threads; i++) {
						if(ind[i].active) {
							active_num_threads++;
							if(ind[i].reported) 
								reporting_num_threads++;
						}
					}

					//fprintf(stderr, "\n[IGP_Get] Bk P. active_num_threads %d; reporting_num_threads %d\n",active_num_threads, reporting_num_threads);	

					// We check all the threads have reported his workload
					if(reporting_num_threads!=active_num_threads) {
						Analyze=false;
						exit=true; 
						new_id_thread=-6;
					//printf("\n[IGP_Get %d] Bk. P. !7 \n", id_thread);		

					}
					if(!exit) {
						num_evaluation_analysis++;

						switch(IGP_Conf.Method) { // IGP tests decision method to verify if it`s posible to create a new thread
							case NONADAPTABLE:
								if(active_num_threads<IGP_Conf.Max_Threads) 
									new_id_thread=1; 
									//fprintf(stderr, "[IGP_Get]: active_num_threads: %d IGP_Conf.Max_Threads: %d\n",active_num_threads,IGP_Conf.Max_Threads);	

								break;
							case ACW: // This strategy allows the multithreaded application autoregulates itself keeping his productivity level over a threshold.
								gettimeofday(&now_time,NULL);
								dif_time = (now_time.tv_sec-last_time.tv_sec) * 1000000 + now_time.tv_usec-last_time.tv_usec;
								//printf("\n[IGP_Get %d] ACW dif_time= %d \n", id_thread,dif_time);		
								if(active_num_threads>0) {
									total_work=0;
									for (i=0; i<IGP_Conf.Max_Threads;i++){ // Calculate total work done by all threads in this interval time.
										if (ind[i].active){
											total_work+=ind[i].workdone;
											//total_work+=total_work+ind[i].workdone;
											//printf("\n[IGP_Get %d] ACW +i %d total_work= %d \n", id_thread,i,total_work);		
										}		
									}
									//fprintf(stderr, "[IGP_Get Core power de [%d] threads -> [%lu]\n", active_num_threads-1, Read_Power_File());

									//Productivity[active_num_threads-1]=total_work/(dif_time*active_num_threads);
									*(Productivity + active_num_threads-1) =total_work/(dif_time*active_num_threads);
									//printf("\n[IGP_Get %d] ACW active_num_threads= %d, Productivity[active_num_threads-1]= %d \n", id_thread,active_num_threads,Productivity[active_num_threads-1]);		
									//printf("\t\tProductivity at %d = %lf \n\n",active_num_threads-1, (double) *(Productivity + active_num_threads-1));

									/*int i =0;
									do{
										//printf("\t\tProductivity at %d = %lf \n\n",i, (double)*(Productivity + i));
										i++;
									}while (i<active_num_threads);*/

									if(active_num_threads!=1) {
										//printf("\n[IGP_Get %d] (!=) active_num_threads = %d \n",id_thread,active_num_threads);		

										Performance = (*(Productivity + active_num_threads-1))/(*(Productivity + active_num_threads-2));
										
										if(Performance>=IGP_Conf.Work_Threshold)
											new_id_thread=1;
									
										else 
											new_id_thread=-1;
									} else 
										new_id_thread=1; // if there is only one thread, GP decides to create a new thread.
										//printf("\n[IGP_Get %d] ACW active_num_threads!=1; new_id_thread = %d \n", id_thread,new_id_thread);		

								} else 
									new_id_thread=1; // if there isn´t any thread, GP decides to create a new thread.
								
								//printf("\n[IGP_Get %d] ACW active_num_threads= %d; new_id_thread = %d \n", id_thread,active_num_threads,new_id_thread);		
								//printf("\n[IGP_Get %d] ACW Performance= %d \n", id_thread,Performance);		

								break;
							case AST: // The process reads from /proc/stats the idle states of the cpu
								switch(IGP_Conf.Approach) {
									case 0:  // It detects if some cpu is idle reading from /proc/stat
										gettimeofday(&now_time,NULL);
										dif_time = (now_time.tv_sec-last_time.tv_sec) * 1000000 + /*(*/now_time.tv_usec-last_time.tv_usec/*)/(double)1000000*/;
								
										if(100*(dif_time/ReadStatFile(0))>= IGP_Conf.Work_Threshold) 
											new_id_thread=1; // It divide the thread if the idle average time is > 5% of the analysis period.
										else 
											new_id_thread=-1;
										break;
									case 1: // It detects if some thread is asleep by reading the status variable of each /proc/[pid]/[tgid]/stat file
										if(ReadStatFile(0)) // It verifies the existence of available cpu
											new_id_thread=ReadStatusFiles(getpid(), active_num_threads);
										else 
											new_id_thread=-1;
										break;
								} 
								break;	
							case APW: // This strategy allows the multithreaded application autoregulates itself keeping his power level over a power and work threshold.
								if(active_num_threads>0) {
									Power[active_num_threads-1]=Read_Power_File();  // power

									if(active_num_threads>2) {
										//fprintf(stderr, "Power with %d active threads[n-3] is [%f]\n", active_num_threads, Power[active_num_threads-3]);
										//fprintf(stderr, "Power with %d active threads[n-2] is [%f]\n", active_num_threads, Power[active_num_threads-2]);
										//fprintf(stderr, "Power with %d active threads[n-1] is [%f]\n", active_num_threads, Power[active_num_threads-1]);					
										//jump1=Power[active_num_threads-1]-Power[active_num_threads-2];
										//jump2=Power[active_num_threads-2]-Power[active_num_threads-3];

										//fprintf(stderr, "Jump1 [%d].jump2[%d]\n", jump1, jump2);

										if(Power[active_num_threads-1]<=Power[active_num_threads-2] || Power[active_num_threads-2]<=Power[active_num_threads-3])
											new_id_thread=1; 
										else
											new_id_thread=-1;										
									} else {
										new_id_thread=1; // if there is only one thread, GP decides to create a new thread.
									}
								} else 
									new_id_thread=1; // if there isn´t any thread, GP decides to create a new thread.*/
								break;
						}

						// Decision method says it´s possible to create a new thread
						if(new_id_thread>=0 && IGP_Conf.Process_is_a_thread) {
							if(active_num_threads<IGP_Conf.Max_Threads) {
								unsigned long maxload=0; 
								int idmax=-1;
								
								//******************************************************************************************************
								// 30/09/2017 - Modified by Alberto Morante Bailón: Add maxload flag:
								if (IGP_Conf.Maxload){
									for(i=0; i<IGP_Conf.Max_Threads; i++) {
										if(ind[i].active) {
											if(maxload<ind[i].workload) { 
												// Loadest thread
												maxload=ind[i].workload;
												idmax=i;
											}
										}
									}
								} else {
									idmax=id_thread;
								}

								// The loadest thread (idmax) has been found
								if(idmax!=-1) {
									if(idmax!=id_thread) {// The current thread is not the loadest one, therefore it must be divided.
										ind[idmax].divide=true;
										Report_to_loadest_thread=true;
										new_id_thread=-4;  
									}
								}	
							} else { // No more threads can be created, since the limit allowed by the user has been exceeded.
								fprintf(stderr, "[IGP_Get] EXCEED THE ALLOWED NUMBER OF THREADS: active_num_threads: %d MAXIMUM: %d.\n", active_num_threads, IGP_Conf.Max_Threads);	
								new_id_thread=-3;
								exit=true;
							}
						}
		

						if(Cancel_analysis) { // It cancelles because parameters has been reset during the analysis.
							Cancel_analysis=false;
							exit=true;
							new_id_thread=-5;
						}
					}
					gettimeofday(&end,NULL);
					analysis_time = analysis_time + (end.tv_sec-start.tv_sec) * 1000000 + /*(*/end.tv_usec-start.tv_usec/*)/(double)1000000*/;
					Analyze=false;
					pthread_mutex_unlock(&mutex_analyze_eficiency);

				}
			}
		break;
		case KERNEL:	
			switch(IGP_Conf.Method) {
				case KST:
//					new_id_thread=syscall(__NR_get_status, workdone, ind[id_thread].workload,  (int)IGP_Conf.Period/*, &data_efficiency*/);
					//printf("[IGP_Get] Pre syscall\n" );
					//new_id_thread=syscall(__NR_get_status, 1, ind[id_thread].workload,  (int)IGP_Conf.Period/*, &data_efficiency*/);
					new_id_thread=syscall(__NR_get_status, 1, trabajo_pendiente, (int)IGP_Conf.Period,1);	// Modificado por Rodrigo Espeso. Añadido tb parametro para distinguir entre get y report
					//printf("[IGP_Get] Post syscall\n" );
					
					break;
				case KITST: case SST:	
//					new_id_thread=syscall(__NR_get_status, workdone, ind[id_thread].workload,  (int)IGP_Conf.Period/*, &data_efficiency*/);
					new_id_thread=syscall(__NR_get_status, 1, trabajo_pendiente,  (int)IGP_Conf.Period,1/*, &data_efficiency*/); // Modificado por Rodrigo Espeso. Añadido tb parametro para distinguir entre get y report
					break;
			
			}
			break;
	}
		
	if(!exit) {
		if(new_id_thread>=0) {
			if(!IGP_Conf.Timing) {
				gettimeofday(&now_time,NULL);
				dif_time = (now_time.tv_sec-last_time.tv_sec) * 1000000 + /*(*/now_time.tv_usec-last_time.tv_usec/*)/(double)1000000*/;
//				if(created_time>ind[id_thread].workload*dif_time/workdone) {
				if(created_time>dif_time) {
				// fprintf(stderr, "[IGP_Get] Created_time: %lu id_thread:  %d workload: %lu dif_time: %f.\n", created_time, id_thread, ind[id_thread].workload, dif_time);
					new_id_thread=-7;  // The remaining workload is not enough.
				}		
				// A new sample cannot be performed until the time specified by the user since the previous sample passes
				if(dif_time<IGP_Conf.Period) 
					new_id_thread=-7;
			}
		} 
	}
	if(new_id_thread>=0) 
		num_efective_analysis++;
}

	if(new_id_thread>=0) { // Current thread can create a new thread.
		creating_thread=true;
		gettimeofday(&Inform_time,NULL);
	}

	//printf("[IGP_Get] End. Level = %d; new_id_thread = %d\n",Level, new_id_thread);
	return(new_id_thread);	

}

// It reports the workdone of a single thread.
void IGP_Report(int id_thread, unsigned long workdone,  unsigned long trabajo_pendiente) {	// Modificado por Rodrigo Espeso. Añadido parametro trabajo_pendiente
	int new_id_thread=-1;
	bool out=false;

//	fprintf(stderr, "[IGP_GET] id_thread.%d\n", id_thread);
//	fprintf(stderr, "[IGP_GET] workload.%lu wordone.%lu\n", workload, workdone);

	if(id_thread<0) {
		fprintf(stderr, "[IGP_Report] ERROR: IGP_id_thread is not an acceptable number.\n");
		exit(1);
	}
	
	// GP has noticed to create a new thread before, so GP is waiting a new thread.
	if(creating_thread) { 
		out=true; 
		new_id_thread=-2; 
	}
	if(!out) { // No creating thread
	    switch(Level) {
		case APPLICATION:		
			ind[id_thread].workdone=ind[id_thread].workdone + workdone;
			ind[id_thread].workload=ind[id_thread].workload - workdone;
			// We mark the thread has reported his workload
			if(!ind[id_thread].reported) ind[id_thread].reported=true;
		 	break;
		case KERNEL:	
			switch(IGP_Conf.Method) {
				case KST:
					//new_id_thread=syscall(__NR_get_status, workdone, ind[id_thread].workload,  (int)IGP_Conf.Period);
					new_id_thread=syscall(__NR_get_status, workdone, trabajo_pendiente, (int)IGP_Conf.Period,0);	// Modificado por Rodrigo Espeso. Añadido tb parametro para distinguir entre get y report
					break;
				case KITST: case SST:	
					//new_id_thread=syscall(__NR_get_status, workdone, ind[id_thread].workload,  (int)IGP_Conf.Period);
					new_id_thread=syscall(__NR_get_status, workdone, trabajo_pendiente, (int)IGP_Conf.Period,0);	// Modificado por Rodrigo Espeso. Añadido tb parametro para distinguir entre get y report
					break;
			}
			break;
		}
	}
}


// The most recently thread executes it, and in NONADAPTABLE is posible to create threads simultaneously. While in ADAPTABLE only one thread creates each instant.
void IGP_Begin_Thread(int id_thread, unsigned long workload) {
	//printf("[IGP_Begin_Thread] Begin id: %d, work: %d\n",id_thread, workload);
	int i;
//	int id_thread;
	unsigned long workdone = 0;
	int salida=0;
	double dif_usecs, dif_time;
	int active_num_threads=0;
	
	if(id_thread<0) {
		fprintf(stderr, "[IGP_Begin_Thread] ERROR: IGP_id_thread is not an acceptable number.\n");
		exit(1);
	}

	if(workload==0) { // no se crea la hebra
		fprintf(stderr, "[IGP_Begin_Thread] No more threads are created because workload is zero.\n");
		switch(Level) {
			case APPLICATION: Reset_data(false); break;
			case KERNEL: syscall(__NR_new_thread_created,0, id_proc_acs, id_thread, NULL,NULL); break;
		}
	}
	else {
		switch(Level) {
			case APPLICATION:
				if(ind[id_thread].active) {
					fprintf(stderr, "[IGP_Begin_Thread] ERROR: the identifier %d is already enabled.\n", id_thread);
					exit(1);
				}
				ind[id_thread].active=true;
				ind[id_thread].reserved=false;
				active_num_threads=Reset_data(false);
				ind[id_thread].pid=(long int)syscall(__NR_gettid);
				ind[id_thread].workload=workload;
				ind[id_thread].workdone=0;	
				break;
			case KERNEL:
				//printf("[IGP_Begin_Thread] Pre-syscall\n" );
				ind[id_thread].active=true;		// Añadido por Rodrigo Espeso
				ind[id_thread].reserved=false;	// Añadido por Rodrigo Espeso
				active_num_threads=syscall(__NR_new_thread_created,1, id_proc_acs, id_thread, 0, workload);
				//printf("[IGP_Begin_Thread] Post-syscall\n" );

				break;
		}
		if(!IGP_Conf.Timing) { // Shows the time in which the thread ends
			gettimeofday(&Interval_time,NULL); // Shows the time in which the program starts his execution
			dif_usecs = (Interval_time.tv_sec-start_time.tv_sec) * 1000000 + (Interval_time.tv_usec-start_time.tv_usec);
			dif_time=(double)dif_usecs/(double)1000000;
			
			// Memoria el tiempo transcurrido desde que se informó la posibilidad de crear una hebra, hasta que se ha creado la hebra.
			created_time = (Interval_time.tv_sec-Inform_time.tv_sec) * 1000000 + (Interval_time.tv_usec-Inform_time.tv_usec);
			
			fprintf(stderr, "[IGP_Begin_Thread] [%f] Create thread %d of %d\n", dif_time, id_thread, num_max_active_threads);	
		}
		
	}
	creating_thread=false;
}

// All threads executes it before their ending, that because it cannot be executed in parallel.
int IGP_End_Thread(int id_thread) {
//	int id_thread;
	//printf("[IGP_End_Thread] The thread %d ends with workdone = %d \n", id_thread,ind[id_thread].workdone );
	int i; 
	int num_test;
	int active_num_threads;
	struct timeval t1;
	double dif_usecs, dif_time;

	if(id_thread<0) {
		fprintf(stderr, "[IGP_End_Thread] ERROR: IGP_id_thread is not an acceptable number.\n");
		exit(1);
	}

	pthread_mutex_lock(&mutex_end_thread);
	
	switch(Level) {
		case APPLICATION:
			ind[id_thread].active=ind[id_thread].reserved=false;
			active_num_threads=Reset_data(false);
			break;
		case KERNEL:
			ind[id_thread].active=ind[id_thread].reserved=false;	// Añadido por Rodrigo Espeso
			active_num_threads=syscall(__NR_thread_end,0);
			break;
	}	

	if(!IGP_Conf.Timing) { // It shows the instant of time in which the thread ends
		gettimeofday(&t1,NULL); // The time in which the actual thread is created is stored
		dif_usecs = (t1.tv_sec-start_time.tv_sec) * 1000000 + (t1.tv_usec-start_time.tv_usec);
		dif_time=(double)dif_usecs/(double)1000000;

		fprintf(stderr, "[IGP_End_Thread] [%f] The thread %d ends\n", dif_time, id_thread);	
				
		// The active thread statistic is updated in order to count the time interval en which iterations are being performed
		dif_usecs = (t1.tv_sec-Interval_time.tv_sec) * 1000000 + (t1.tv_usec-Interval_time.tv_usec);
		dif_time=(double)dif_usecs/(double)1000000;
		Interval_time.tv_sec=t1.tv_sec;
		Interval_time.tv_usec=t1.tv_usec;
		ThreadsCount[active_num_threads]=ThreadsCount[active_num_threads]+dif_usecs;
	}	

	creating_thread=false;
	pthread_mutex_unlock(&mutex_end_thread);

	//printf("[IGP_End_Thread] The thread %d ends. Active_num_threads now = %d\n", id_thread,active_num_threads);	
	return(active_num_threads);

}

int Reset_data(bool All) {
	int i; 
	int active_num_threads=0;
	Report_to_loadest_thread=false;
	Reset_analysis=true;
	if(Analyze) 
		Cancel_analysis=true;

	for(i=0; i< IGP_Conf.Max_Threads; i++) { // Values are initialized
		if(All) ind[i].active=false;
		else
			if(ind[i].active) active_num_threads++;			
		if(!IGP_Conf.Process_is_a_thread && !i) ind[0].reported=true;
		else ind[i].reported=false;
		ind[i].divide=false;
		ind[i].workdone=0;
		ind[i].workload=0;
		ind[i].utime_thread=0;
	}

	if(num_max_active_threads<active_num_threads) 
		num_max_active_threads=active_num_threads;

	if(Level==APPLICATION){
		if(!IGP_Conf.Timing){
			gettimeofday(&last_time,NULL);
		}
	}

	Reset_analysis=false;
	Report_to_loadest_thread=false;
	return(active_num_threads);
}

//*****************************************************************************************************
// Modified by Juan F Sanjuan Estrada. The handler function must be simple, with a few lines of code, only the received signal is stored in the ORDER variable, and in task information about the transmitter.
//void Handler(int code, siginfo_t *info, void *context) {
//}

double ReadStatFile(int start) {
	FILE *file;
	size_t size;
	int i, id;
	double time_idle=0; char string[2048];
	unsigned long data[8]; // user, nice, system, idle, iowait, irq, softirq, steal.
	
	if(start) { // Read the number of active cpu
		NCpus=0;
		file=fopen("/proc/cpuinfo","r");
		if(file==NULL) {
			fprintf(stderr, "[IGP_ReadStatFile] Error: The file /proc/cpuinfo cannot be opened\n");
			exit(1);
		}	
		
		while(!feof(file)) {
			fgets(string, 2048,file);
			//fprintf(stderr, "[IGP_ReadStatFile] %s\n", string);
			if(strstr(string, "processor")!=NULL)
				NCpus++;
		}
		if(!NCpus) {
			fprintf(stderr, "[IGP_ReadStatfile] Error: The number of active cpu in the system could not be detected.\n");
			exit(1);
		}else{ // Reserva memoria
			size=sizeof(unsigned long)*NCpus;
#ifdef USE_THREAD_ALLOC
			jiffies_idle=(unsigned long *)thread_alloc(size);
#endif
#ifndef USE_THREAD_ALLOC
			jiffies_idle=(unsigned long *)malloc(size);
			if(jiffies_idle==NULL)  { 
				fprintf(stderr, "[IGP_ReadStatFile] ERROR: Failure reserving dynamic memory\n"); 
				exit(1); 
			}

#endif
		}
		fclose(file);
	}
	file=fopen("/proc/stat","r");
	if(file==NULL) {
		fprintf(stderr, "[IGP_ReadStatFile] Error: The file /proc/stat could not be opened\n");
		exit(1);
	}	
	fgets(string, 2048,file);
	if(strstr(string,"cpu")!=NULL) { // All the sum of the cpu statistics in the system has been read
		for(i=0; i<NCpus; i++) {
			fgets(string, 2048,file); // A single cpu statistic is read
			if(strstr(string,"cpu")!=NULL) {
				sscanf(string, "cpu%d %lu %lu %lu %lu %lu %lu %lu %lu", &id, &data[0], &data[1], &data[2], &data[3], &data[4], &data[5], &data[6], &data[7]);
				//fprintf(stderr, "[IGP_ReadStatFile] Se ha leido: %u %u %u %u %u\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
				if(start) jiffies_idle[id]=data[3];
				else {				
					time_idle=time_idle+data[3]-jiffies_idle[id];
					jiffies_idle[id]=data[3];
					//fprintf(stderr, "[IGP_ReadStatFile] id&%d dle: %lu \n",id, jiffies_idle[id]);
					
				}
			}
		}
	}		 
	fclose(file);
	
	return(time_idle);
}
// This function reads all stat file of each thread from the main process, and analyze the thread state. If the thread state is 'S' (Sleep) it returns a 0 indicating that the thread must not be divided.
int ReadStatusFiles(int pid_proc, int active_num_threads) {
	static DIR *dip;
	static struct dirent *dit;
	int pid;
	char utime_char[50];
	char status[5];
	unsigned long utime; 
	FILE *file;
	int  i = 0;
	char dir[2048], filename[2048], string[2048];
	int divide=-1;
	int num_threads=0;
	sprintf(dir,"/proc/%d/task",pid_proc);
	//fprintf(stderr, "[IGP_ReadStatusFiles] Read directory: %s\n", dir);
	
	/* DIR *opendir(const char *name); Open a directory stream to argv[1] and make sure it's a readable and valid (directory) */
	if ((dip = opendir(dir)) == NULL) {
		perror("opendir");
        return(-1);
	}
	/*  struct dirent *readdir(DIR *dir); Read in the files from argv[1] and print */
	while ((dit = readdir(dip)) != NULL) {
		//fprintf(stderr, "[IGP_ReadStatusFiles] d_name [%s]\n", dit->d_name);
		if(strstr(dit->d_name,".")==NULL) { // If the directory answers to one thread
			pid=atoi(dit->d_name);
			if(pid!=pid_proc) { // The file of the main process is not read	
				sprintf(filename, "/proc/%d/task/%s/stat", pid_proc, dit->d_name);
				//fprintf(stderr, "[IGP_ReadStatusFiles] Open the file %s\n", filename);
				file=fopen(filename,"r");
				if(file==NULL) {
//					fprintf(stderr, "[IGP_ReadStatusFiles] Error: File %s cannot be opened\n", filename);
					closedir(dip);

					return(-1);
				}	
				fgets(string, 2048,file);
				//fprintf(stderr, "[IGP_ReadStatusFiles] string: %s\n",string);
				if(ferror(file)) {
					fclose(file);
					closedir(dip);
					return(-1);
				}
				GetParameter(string, 3, status);
				GetParameter(string, 14, utime_char);
				utime=atol(utime_char);
				
				//fprintf(stderr, "[IGP_ReadStatusFiles] pid: %d status %c value of utime: %lu\n", pid, status[0], utime);

				// If any thread has not being executed from the last analysis, the current analysis is stopped
				// Search previous information of the thread
				for(i=0; i < IGP_Conf.Max_Threads; i++) {
					if(ind[i].active && ind[i].pid==pid) break;
						//fprintf(stderr, "[IGP_ReadStatusFiles] pid_proc: %d pid:%d id_thread: %d i=%d MAX_THREADS: %d\n", pid_proc, pid, ind[i].pid, i, num_max_active_threads);
				}
				if(i<IGP_Conf.Max_Threads) { // Found
					// If any thread is detected sleeping, the current analysis is stopped
					if(utime - ind[i].utime_thread==0 || status[0]!='R') divide=-1;
					else {
						divide=1;
						num_threads++;
					}
					ind[i].utime_thread=utime;
				} else {// Not found, search a free space
					divide=-1;
				}
				fclose(file);
				//fprintf(stderr, "[IGP_ReadStatusFiles] it returns: %d\n",divide);
				if(divide<0) {
					closedir(dip);
					return(-1);
				}
			}
		}
	}
	if(num_threads!=active_num_threads) divide=-1; // It checks all active threads have being analyzed.
	if(active_num_threads==IGP_Conf.Max_Threads) divide=-2; // It checks that the maximum allowed is not exceeded
	
	/* int closedir(DIR *dir); Close the stream to argv[1]. And check for errors. */
	closedir(dip);
	return(divide);
}
void GetParameter(char *string, int param, char *parameter) {
	char *p_ant, *p_sig;
	int i;
	p_ant=string;
	for(i=0; i<param; i++) {
		p_sig=strstr(p_ant, " ");
		if(i+1!=param) p_ant=p_sig+1;
	}
	strncpy(parameter, p_ant, strlen(p_ant)-strlen(p_sig));
	parameter[strlen(p_ant)-strlen(p_sig)]='\0';
	//fprintf(stderr, "[IGP_GetParameter] Parameter: %s\n", parameter);
}

void ShowFile(int pid_proc) {	
	FILE *file;
	char filename[2048], string[2048];
	
	sprintf(filename,"/proc/%d/status",pid_proc);
	file=fopen(filename,"r");
	if(file==NULL) {
		fprintf(stderr, "[IGP_Showfile] Error: The file /proc/stat cannot be opened\n");
		exit(1);
	}	
	fprintf(stderr, "[IGP_Showfile] File content /proc/pid/status: \n");
	while(!feof(file)){
		fgets(string, 2048, file);
		fprintf(stderr, "[IGP_ShowFile] %s", string);
	}

	fclose(file);
	
}


	
	
	
