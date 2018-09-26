#include <dirent.h>
#include <signal.h>		// Libreria para trabajar con se√±ales
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/resource.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#include "define_adaptive.h"


#define __NR_gettid			186 // Esta llamada al sistema ya est√° implementada, pero el n√∫mero puede variar de una versi√≥n a otra.
#define __NR_get_status			333	// Obtiene el resultado de la monitorización: dividirse o no . 	
#define __NR_warns_status		334	// Solicita al planificador que le monitorice
#define __NR_new_thread_created	335	// Informa al planificador que se ha creado una hebra nueva
#define __NR_thread_end			336	// Informa al planificador que se ha terminado la hebra
#define __NR_detener_hebra		338


void IGP_Initialice();
void IGP_Finalice(void);
int IGP_Get(int id_thread, unsigned long trabajo_pendiente);
void IGP_Report(int id_thread, unsigned long workdone, unsigned long trabajo_pendiente);
void IGP_Begin_Thread(int id_thread, unsigned long workload);
int IGP_End_Thread(int id_thread);

void Handler(int code, siginfo_t *info, void *context);
double ReadStatFile(int);
int ReadStatusFiles(int, int);
void ShowFile(int);
int analizar_eficiencia(int   id, int   elementos, int iteraciones);
void GetParameter(char *string, int param, char *parameter);
//void RetardarSeccionCritica(void);
int Reset_data(bool all);


struct IGP_Configuration {
	int Max_Threads;
	int Method;
	double Period;
	int Approach;
	double Work_Threshold;
	double Power_Threshold;
	bool Timing;
	bool Detention;
	bool Process_is_a_thread;
	bool Maxload;
};




