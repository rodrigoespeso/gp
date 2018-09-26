#include "utils.h"
using namespace std;

nanoseconds timeit(high_resolution_clock::time_point s)
{
    return duration_cast<nanoseconds>(high_resolution_clock::now() - s);
}

void init_env(int count, const char**argv)
{
    if (count < 3) {
        puts("Usage: ./$out nchunks testX.txt");    
        exit(-1);
    }

    double len;
    nchunks = stoi(argv[1]), iters = stoi(argv[2]);
    mass = 100, t = 0.1;
    //print1 = stoi(argv[4]) == string("1"); print2 = stoi(argv[5]) == string("1");
    input_bodies(argv[3]);
    
    trabajo_pendiente_total = (unsigned long) num_body*iters;
    trabajo_realizado_total = 0;
    // Intialize diferents chunk workloads

    int resto=(num_body*iters)%nchunks;
    chunk_workloads.reserve(nchunks);
        for (int i=0;i<nchunks;i++){
        chunk_workloads[i]=num_body*iters/nchunks;
        if (resto>0) {
            chunk_workloads[i]++;
            resto--;
        }
    }

    // Initialize workers_id
    for(int i = 1;i<=nchunks;++i){
        workers_id.push_back(i);
    }
}

void input_bodies(string filename)
{

#ifdef _IO_TIME
    high_resolution_clock::time_point t = high_resolution_clock::now();
#endif
    ifstream input;
    input.open(filename);
    input >> num_body;
    bodies = new Body[num_body];
    new_bodies = new Body[num_body];

    for (int i = 0; i < num_body; ++i) {
        Body& t = bodies[i];
        input >> t.x >> t.y >> t.vx >> t.vy;
    }
    input.close();
#ifdef _IO_TIME
    io_time += timeit(t);
#endif
}
