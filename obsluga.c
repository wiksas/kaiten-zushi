#include "common.h"

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    SharedData* sdata = shmat(shmid, NULL, 0);
    int semid = semget(SEM_KEY, 6, 0600);

    printf("[Obsluga] Silnik tasmy uruchomiony.\n");

    while (sdata->open && !sdata->emergency_exit) {
        sleep(1);

        sem_op(semid, 5, -1);


        Plate temp = sdata->belt[P - 1];
        for (int i = P - 1; i > 0; i--) {
            sdata->belt[i] = sdata->belt[i - 1];
        }


        sdata->belt[0].is_empty = true;
        sdata->belt[0].target_table_pid = -1;



        sem_op(semid, 5, 1);
    }

    shmdt(sdata);
    return 0;
}