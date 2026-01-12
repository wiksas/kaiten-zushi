#include "common.h"
#include <sched.h>

void handle_speed_signals(int sig) {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    if (shmid == -1) return; 

    SharedData* sd = shmat(shmid, NULL, 0);
    if (sd == (void*)-1) return;

    if (sig == SIGUSR1) { 
        sd->kitchen_delay_us /= 2; 
        printf("\033[1;33m[Kucharz] Przyspieszam! (Delay: %d)\033[0m\n", sd->kitchen_delay_us); 
    }
    else if (sig == SIGUSR2) { 
        sd->kitchen_delay_us *= 2; 
        printf("\033[1;33m[Kucharz] Zwalniam... (Delay: %d)\033[0m\n", sd->kitchen_delay_us); 
    }
    shmdt(sd);
}

int main() {
    signal(SIGUSR1, handle_speed_signals);
    signal(SIGUSR2, handle_speed_signals);

    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    if (shmid == -1) { perror("shmget kucharz"); exit(1); }
    SharedData* sd = shmat(shmid, NULL, 0);

    int semid = semget(SEM_KEY, 7, 0600);
    int msgid = msgget(MSG_KEY, 0600);

    printf("[Kucharz] Gotowy.\n");

    while (sd->open && !sd->emergency_exit) {
        SpecialOrder order;
        bool has_special = false;

        if (msgrcv(msgid, &order, sizeof(int), 0, IPC_NOWAIT) != -1) has_special = true;

        if (sd->kitchen_delay_us > 0) usleep(sd->kitchen_delay_us);
        
        sched_yield();

        sem_op(semid, 0, -1); 

        if (sd->belt[0].is_empty) {
            if (has_special) {
                sd->belt[0].price = order.price;
                sd->belt[0].target_table_pid = order.mtype;
                sd->belt[0].is_empty = false;

                if (order.price == 40) sd->stats_produced[3]++;
                else if (order.price == 50) sd->stats_produced[4]++;
                else if (order.price == 60) sd->stats_produced[5]++;

                printf("\033[1;35m[Kucharz] !!! Wydano SPECJALNE (%d zl) dla grupy %ld !!!\033[0m\n",
                    order.price, order.mtype);
            }
            else {
                int r = rand() % 3;
                int ceny[] = { 10, 15, 20 };
                sd->belt[0].price = ceny[r];
                sd->belt[0].target_table_pid = -1;
                sd->belt[0].is_empty = false;

                sd->stats_produced[r]++;
                printf("\033[1;36m[Kucharz] Wydano standard (%d zl)\033[0m\n", ceny[r]);
            }
        }
        else if (has_special) {
            msgsnd(msgid, &order, sizeof(int), 0);
        }
        sem_op(semid, 0, 1);

        sched_yield();
    }
    shmdt(sd);
    return 0;
}