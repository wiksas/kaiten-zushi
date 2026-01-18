#include "common.h"
#include <time.h> 

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    if (shmid == -1) { perror("shmget obsluga"); exit(1); }
    SharedData* sdata = shmat(shmid, NULL, 0);

    int semid = semget(SEM_KEY, 7, 0600);

    printf("[Obsluga] Silnik tasmy.\n");

    while (sdata->open && !sdata->emergency_exit) {
        

        usleep(2000000); 

        sem_op(semid, 0, -1);


        Plate last_item = sdata->belt[P - 1];


        for (int i = P - 1; i > 0; i--) {
            sdata->belt[i] = sdata->belt[i - 1];
        }


        sdata->belt[0] = last_item;

        sem_op(semid, 0, 1);
        
    }

    shmdt(sdata);
    printf("[Obsluga] Koniec pracy.\n");
    return 0;
}