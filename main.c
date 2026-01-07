#include "common.h"

int main() {

    int shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0600);
    SharedData* sdata = shmat(shmid, NULL, 0);


    sdata->restaurant_open = true;
    for (int i = 0; i < BELT_SIZE; i++) sdata->belt[i].is_empty = true;

    int semid = semget(SEM_KEY, 1, IPC_CREAT | 0600);
    semctl(semid, 0, SETVAL, 1); 

    printf("[SYSTEM] Restauracja otwarta. Uruchamiam procesy...\n");

    pid_t kucharz_pid = fork();
    if (kucharz_pid == 0) {
        execl("./kucharz", "kucharz", NULL);
        exit(0);
    }


    printf("[SYSTEM] Naciœnij Enter, aby zamkn¹æ restauracjê...\n");
    getchar();

    sdata->restaurant_open = false;
    kill(kucharz_pid, SIGTERM);


    shmdt(sdata);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);

    printf("[SYSTEM] Zasoby zwolnione. Koniec.\n");
    return 0;
}