#include "common.h"


void handle_speed_signals(int sig) {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    SharedData* sd = shmat(shmid, NULL, 0);

    if (sig == SIGUSR1) {
        sd->kitchen_delay_us /= 2; // Przyspieszenie pracy
        printf("\033[1;33m[Kucharz] Sygnal odebrany: Przyspieszam tempo produkcji!\033[0m\n");
    }
    else if (sig == SIGUSR2) {
        sd->kitchen_delay_us *= 2; // Spowolnienie pracy
        printf("\033[1;33m[Kucharz] Sygnal odebrany: Zwalniam tempo produkcji...\033[0m\n");
    }
    shmdt(sd);
}

int main() {
    // Rejestracja sygna³ów
    signal(SIGUSR1, handle_speed_signals);
    signal(SIGUSR2, handle_speed_signals);

    // Pod³¹czenie zasobów IPC
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    if (shmid == -1) { perror("shmget kucharz"); exit(1); }
    SharedData* sd = shmat(shmid, NULL, 0);

    int semid = semget(SEM_KEY, 6, 0600);
    int msgid = msgget(MSG_KEY, 0600);

    printf("[Kucharz] Kuchnia uruchomiona. Czekam na zamowienia...\n");

    while (sd->open && !sd->emergency_exit) {
        SpecialOrder order;
        bool has_special = false;

        // 1. SPRAWDZENIE KOLEJKI KOMUNIKATÓW (Zamówienia z tabletów)
        if (msgrcv(msgid, &order, sizeof(int), 0, IPC_NOWAIT) != -1) {
            has_special = true;
        }

        // 2. PRZYGOTOWANIE POSI£KU (Symulacja czasu pracy)
        usleep(sd->kitchen_delay_us);


        sem_op(semid, 5, -1);

        // Kucharz zawsze k³adzie dania na pozycjê 0
        if (sd->belt[0].is_empty) {
            if (has_special) {
                // Realizacja zamówienia specjalnego
                sd->belt[0].price = order.price;
                sd->belt[0].target_table_pid = order.mtype;
                sd->belt[0].is_empty = false;

                if (order.price == 40) sd->stats_produced[3]++;
                else if (order.price == 50) sd->stats_produced[4]++;
                else if (order.price == 60) sd->stats_produced[5]++;

                printf("\033[1;35m[Kucharz] KLADZIE danie SPECJALNE (%d zl) dla grupy %ld na pozycje 0\033[0m\n",
                    order.price, order.mtype);
            }
            else {
                // Produkcja dania standardowego
                int type = rand() % 3;
                int ceny[] = { 10, 15, 20 };

                sd->belt[0].price = ceny[type];
                sd->belt[0].target_table_pid = -1;
                sd->belt[0].is_empty = false;

                sd->stats_produced[type]++;

                printf("\033[1;36m[Kucharz] KLADZIE danie standardowe (%d z) na pozycje 0\033[0m\n",
                    sd->belt[0].price);
            }
        }
        else {
            // Jeœli pozycja 0 jest zajêta, zwracamy zamówienie specjalne do kolejki
            if (has_special) {
                msgsnd(msgid, &order, sizeof(int), 0);
            }
        }

        sem_op(semid, 5, 1);
    }

    shmdt(sd);
    printf("[Kucharz] Restauracja zamyka kuchnie. Koncze prace.\n");
    return 0;
}