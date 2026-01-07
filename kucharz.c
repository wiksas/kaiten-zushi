#include "common.h"

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    SharedData* sdata = shmat(shmid, NULL, 0);
    int semid = semget(SEM_KEY, 1, 0600);

    srand(time(NULL));

    printf("[Kucharz] Zaczynam pracê!\n");

    while (sdata->restaurant_open) {
        sleep(2);

        sem_op(semid, -1); // BLOKUJÊ DOSTÊP (P)

        if (sdata->belt[0].is_empty) {
            sdata->belt[0].price = (rand() % 3 + 1) * 10;
            sdata->belt[0].is_empty = false;
            printf("[Kucharz] Po³o¿y³em sushi za %d z³ na pocz¹tku taœmy.\n", sdata->belt[0].price);
        }
        else {
            printf("[Kucharz] Taœma jest zablokowana, czekam...\n");
        }

        sem_op(semid, 1); // ZWALNIAM DOSTÊP (V)
    }

    shmdt(sdata);
    return 0;
}