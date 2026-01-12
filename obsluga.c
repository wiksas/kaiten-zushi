#include "common.h"
#include <sched.h> 

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    if (shmid == -1) { perror("shmget obsluga"); exit(1); }
    SharedData* sdata = shmat(shmid, NULL, 0);

    int semid = semget(SEM_KEY, 7, 0600);

    printf("[Obsluga] Silnik tasmy uruchomiony (PID: %d).\n", getpid());

    while (sdata->open && !sdata->emergency_exit) {
        
        usleep(1000000); 
        sched_yield();

        sem_op(semid, 0, -1);

        for (int i = P - 1; i > 0; i--) {
            sdata->belt[i] = sdata->belt[i - 1];
        }

        sdata->belt[0].is_empty = true;
        sdata->belt[0].target_table_pid = -1;
        sdata->belt[0].price = 0;

        sem_op(semid, 0, 1);
    }

    shmdt(sdata);
    printf("[Obsluga] Koniec pracy.\n");
    return 0;
}