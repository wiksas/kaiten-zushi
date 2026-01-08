#include "common.h"
#include <time.h>

int shmid, semid, msgid;
SharedData* sdata = NULL;
pid_t kucharz_pid, obsluga_pid, kierownik_pid;

void emergency_cleanup(int sig) {
    kill(0, SIGKILL);
    if (sdata != NULL && sdata != (void*)-1) shmdt(sdata);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    msgctl(msgid, IPC_RMID, NULL);
    exit(1);
}

void cleanup_ipc() {
    if (kucharz_pid > 0) kill(kucharz_pid, SIGTERM);
    if (obsluga_pid > 0) kill(obsluga_pid, SIGTERM);
    if (kierownik_pid > 0) kill(kierownik_pid, SIGTERM);
    usleep(100000);
    kill(0, SIGKILL);
    if (sdata != NULL && sdata != (void*)-1) shmdt(sdata);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    msgctl(msgid, IPC_RMID, NULL);
}

void handle_sigint(int sig) { (void)sig; cleanup_ipc(); exit(0); }

int main() {
    srand(time(NULL));
    signal(SIGINT, handle_sigint);
    signal(SIGALRM, emergency_cleanup);
    alarm(60);

    shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0600);
    sdata = (SharedData*)shmat(shmid, NULL, 0);


    sdata->start_time = 600;
    sdata->end_time = 615;
    sdata->current_time = sdata->start_time;
    sdata->open = true;
    sdata->emergency_exit = false;
    sdata->is_closed_for_new = false;
    sdata->kitchen_delay_us = 1000000;

    for (int i = 0; i < 6; i++) { sdata->stats_produced[i] = 0; if (i < 3) sdata->stats_sold[i] = 0; }
    sdata->stats_special_revenue = 0;


    for (int i = 0; i < P; i++) {
        sdata->belt[i].is_empty = true;
        sdata->current_occupancy[i] = 0;

        if (i < LADA_LIMIT) sdata->table_capacity[i] = 1;
        else if (i >= 6 && i <= 10) sdata->table_capacity[i] = 2;
        else if (i >= 11 && i <= 15) sdata->table_capacity[i] = 3;
        else if (i >= 16 && i <= 19) sdata->table_capacity[i] = 4;
    }


    semid = semget(SEM_KEY, 7, IPC_CREAT | 0600);
    semctl(semid, 0, SETVAL, 1);
    semctl(semid, 1, SETVAL, 0);


    semctl(semid, 2, SETVAL, 5 * 2);
    semctl(semid, 3, SETVAL, 5 * 3);
    semctl(semid, 4, SETVAL, 4 * 4);

    semctl(semid, 5, SETVAL, 1);
    semctl(semid, 6, SETVAL, LADA_LIMIT);

    msgid = msgget(MSG_KEY, IPC_CREAT | 0600);


    if ((kucharz_pid = fork()) == 0) execl("./kucharz", "kucharz", NULL);
    if ((obsluga_pid = fork()) == 0) execl("./obsluga", "obsluga", NULL);
    if ((kierownik_pid = fork()) == 0) execl("./kierownik", "kierownik", NULL);

    int client_count = 0;
    printf("[Main] Start restauracji.\n");


    while (sdata->current_time < sdata->end_time && !sdata->emergency_exit) {

        // 1. LOSOWY CZAS PRZYBYCIA
        // Czeka od 0.5 sekundy (500000 us) do 3.5 sekundy (3500000 us)
        usleep(500000 + (rand() % 3000000));

        if (sdata->current_time >= sdata->end_time || sdata->emergency_exit) break;

        if (fork() == 0) {
            char s[3], v[2];


            int g_size = (rand() % 4) + 1;
            int is_vip = (rand() % 100 < 2);

            sprintf(s, "%d", g_size);
            sprintf(v, "%d", is_vip);

            if (is_vip) printf("\n\033[1;31m[System] !!! NADCHODZI GRUPA VIP (%d os.) !!!\033[0m\n", g_size);
            else printf("[System] Nowa grupa klientów: %d os.\n", g_size);

            execl("./klient", "klient", s, v, NULL);
            exit(0);
        }
        client_count++;
    }

    sdata->is_closed_for_new = true;
    printf("\n[Main] Koniec wpuszczania. Czekam na %d grup...\n", client_count);

    int finished = 0;
    while (finished < client_count) {
        pid_t p = wait(NULL);
        if (p == -1) break;
        if (p != kucharz_pid && p != obsluga_pid && p != kierownik_pid) finished++;
    }

    sdata->open = false;
    kill(kucharz_pid, SIGTERM);
    kill(obsluga_pid, SIGTERM);
    kill(kierownik_pid, SIGTERM);
    usleep(200000);

    printf("\n=======================================\n");
    printf("[Kucharz] RAPORT PRODUKCJI:\n");
    int ceny[] = { 10, 15, 20, 40, 50, 60 };
    int suma = 0;
    for (int i = 0; i < 6; i++) {
        int ilosc = sdata->stats_produced[i];
        int wartosc = ilosc * ceny[i];
        suma += wartosc;
        printf("Danie typ %d, %d sztuk, cena %d zl\n", i + 1, ilosc, ceny[i]);
    }
    printf("KOSZT PRODUKCJI: %d zl\n", suma);

    printf("=======================================\n");
    printf("[Obsluga] RAPORT FINANSOWY:\n");
    int utarg = 0;
    for (int i = 0; i < 3; i++) utarg += sdata->stats_sold[i] * ceny[i];
    utarg += sdata->stats_special_revenue;
    printf(" Przychod: %d zl\n", utarg);
    printf(" Jedzenie na tasmie (Straty):\n");

    int straty = 0;
    for (int i = 0; i < P; i++) {
        if (!sdata->belt[i].is_empty) {
            printf(" Pozycja %2d: %d zl\n", i, sdata->belt[i].price);
            straty += sdata->belt[i].price;
        }
    }
    printf("\nLACZNA WARTOSC STRAT: %d zl\n", straty);
    printf("=======================================\n");

    cleanup_ipc();
    return 0;
}