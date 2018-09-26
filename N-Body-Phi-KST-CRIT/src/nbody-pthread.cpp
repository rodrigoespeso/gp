#include <pthread.h>
#include "utils.h"
#include <iostream>
#include <fstream>
extern "C" {
    #include "IGP_library.h"
}

extern int num_analysis,num_evaluation_analysis,num_efective_analysis,num_max_active_threads;
extern unsigned long analysis_time;

bool finnish = false, print1 = true, print2 = false;
bool done = false;
double mass, t, Gmm;
Body *bodies, *new_bodies;
int nchunks, iters, num_body;
int queuing_jobs = 0, num_done = 0;
unsigned long trabajo_pendiente_total, trabajo_realizado_total;
pthread_mutex_t queuing, creating_thd;
pthread_cond_t processing, iter_fin, allfinnished, created;
nanoseconds total_time;
vector<int> chunk_workloads, workers_id;
high_resolution_clock::time_point s;

typedef struct{
    int id_igp;
    int chunk_work;
} worker_info;

worker_info *param_worker;

worker_info *thread_param = NULL;
worker_info *reall_thread_param;

//worker_info *two_param_worker;

void* worker(void* param)
{
    pthread_mutex_lock(&creating_thd);
    worker_info *param_w = (worker_info*) param ;
    
    int id = param_w->id_igp;
    int chunk_workload = param_w->chunk_work;
    pthread_mutex_unlock(&creating_thd);
    int workdone = 0; bool isworkdone = false;
    if(print1) printf("[W igp=%i, work=%i] INITIALIZE WORKER\n", id, chunk_workload);                                            
    IGP_Begin_Thread(id,chunk_workload);
    while (workdone<chunk_workload) {
        pthread_mutex_lock(&queuing);
        while (!finnish && queuing_jobs <= 0){
            //if (print2) printf("[W igp=%i, chunk_work= %i] Pre-wait processing\n",id, chunk_workload);
            pthread_cond_wait(&processing, &queuing);
            //if (print2) printf("[W igp=%i, chunk_work= %i] Post-wait processing\n",id, chunk_workload);
        }
        int i = --queuing_jobs;
        pthread_mutex_unlock(&queuing);
        if (finnish){ 
            //if (print2) printf("[W igp=%i, chunk_work= %i]Finnish! Break.\n",id, chunk_workload);
            break;
        }   
        //if (print2) printf("[W igp=%i, chunk_work= %i] Brk Pt. 1\n",id, chunk_workload);
        move_nth_body(i);
        workdone++;
        pthread_mutex_lock(&queuing);
        num_done++;
        trabajo_pendiente_total--;                                    
        trabajo_realizado_total++;
        IGP_Report(id, 1, trabajo_pendiente_total);
        //IGP_Report(0, 1, trabajo_pendiente_total);
        //if (print2) printf("[W igp=%i, chunk_work= %i] Brk Pt. 2\n",id, chunk_workload);
        if (num_done >= num_body){
            pthread_cond_signal(&iter_fin);
            //if (print2) printf("[W igp=%i, chunk_work= %i] num_done >= num_body\n",id, chunk_workload);
        }          
        pthread_mutex_unlock(&queuing);
    }
    IGP_End_Thread(id);
    //if (print2) printf("[W igp=%i, chunk_work= %i] End! Workdone = %i\n",id, chunk_workload,workdone);                                 
}

void* switch_bodies(void* param)
{   
    for (int i = 0; i < iters; ++i) {
        s = high_resolution_clock::now();
        pthread_mutex_lock(&queuing);
        queuing_jobs = num_body, num_done = 0;
        pthread_cond_broadcast(&processing);
        //if (print2) printf("[SB %i] Pre-wait iter_fin\n",i);
        pthread_cond_wait(&iter_fin, &queuing);
        //if (print2) printf("[SB %i] Post-wait iter_fin\n",i);
        pthread_mutex_unlock(&queuing);
        Body* t = new_bodies; new_bodies = bodies; bodies = t;
        total_time += timeit(s);
    }
    finnish = true;
    pthread_cond_broadcast(&allfinnished);   
    if (print1) printf("[SB] End. Broadcast...\n");
}

int main(int argc, char const **argv)
{
    init_env(argc, argv);
    
    // Print workloads
    if(print2){
        for (int i = 0; i<nchunks; i++){
            printf("chunk[%i] = %i\t",i,chunk_workloads[i]);
        }
        printf("\n");
    }

    pthread_t workers[nchunks];
    pthread_t switching;
    
    pthread_mutex_init(&queuing, NULL);
    pthread_mutex_init(&creating_thd, NULL);
    pthread_cond_init(&iter_fin, NULL);
    pthread_cond_init(&processing, NULL);
    pthread_cond_init(&created, NULL);
    
    int created_threads = 0;
    int att_created_threads = 0;
    int i=0;
    //int j=-1;
    int fails = 0;
    int conf_create=-1;
    int id_igp_given = -1;
    Gmm = G * mass * mass;
    int work_per_thread = -1;
    
    pthread_create(&switching, NULL, switch_bodies, NULL);
    
    IGP_Initialice();
    
    param_worker = (worker_info *) malloc(sizeof(worker_info)); 
    //two_param_worker = (worker_info *) malloc(sizeof(worker_info)); 
    //thread_param = NULL;
    //reall_thread_param = NULL;

    while(att_created_threads<nchunks){            
        
        id_igp_given=IGP_Get(0, trabajo_pendiente_total);
                    
        free (thread_param);
        thread_param = (worker_info *) malloc (sizeof(worker_info));
        
        if(id_igp_given>0){
            
            work_per_thread=param_worker->chunk_work=chunk_workloads[i];
            param_worker->id_igp=id_igp_given=created_threads+1;

            thread_param->id_igp = param_worker->id_igp;
            thread_param->chunk_work = param_worker->chunk_work;
            conf_create=pthread_create(&workers[i], NULL, worker, (void *) (param_worker));
            if(conf_create==0){
                fails=0; 
                pthread_mutex_lock(&creating_thd);
                created_threads++; 
                att_created_threads++; 
                i++;//j+=2;
                pthread_mutex_unlock(&creating_thd);
                if (print2) printf("\n\t[Main]PTHREAD[%i] CREATED. id_igp_given = %i . chunk_work = %i\n",i-1,id_igp_given,work_per_thread);
            }else{
                if(print2) printf("\t[Main]PTHREAD[%i] NOT CREATED! (Error: %i)id_igp %i \n",i-1,conf_create,id_igp_given);
            }
            //if(!done)
            //if(att_created_threads!=nchunks)
            //    pthread_cond_wait(&created, &creating_thd);
            //thread_mutex_unlock(&creating_thd);
        }else{
            fails++;
            
            if (fails%2000000==0){
                printf("(%i) %d Fails. Created threads: %i\n", id_igp_given, fails,created_threads);
                /*if (fails==10000000){
                    IGP_Finalice();
                    return 0;
                }   */
            }
        }
         
    }

    if(print1) printf("[Main] Waiting for joining... \n");
    if (!finnish)
        pthread_cond_wait(&allfinnished, &queuing);
    if(print1) printf("[Main] Joining. \n");
    for (int j = 0; j < created_threads; ++j) pthread_join(workers[j], NULL);
    pthread_join(switching,NULL);
    IGP_Finalice();
    INFO("Run in " << total_time.count() / 1000 << " us");
    free (param_worker);
    /*
    std::ofstream output;
    output.open("results.txt", std::ofstream::out | std::ofstream::app);
    output<<num_body<<" "<<nchunks<<" "<<iters<<" "<<analysis_time<<" "<<num_analysis<<" "<<num_evaluation_analysis<<" "<<num_efective_analysis<<" "<<num_max_active_threads<<" "<<total_time.count()/1000<< endl;
    output.close();
    */
    return 0;
}
