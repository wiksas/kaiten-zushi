#include "common.h"

int shmid, semid, msgid;
SharedData* sdata = NULL;
pid_t kucharz_pid, obsluga_pid, kierownik_pid;


void cleanup_ipc() {
    printf("\n[Main] Konczenie pracy... Sprzatanie IPC.\n");
    if (kucharz_pid > 0) kill(kucharz_pid, SIGTERM);
    if (obsluga_pid > 0) kill(obsluga_pid, SIGTERM);
    if (kierownik_pid > 0) kill(kierownik_pid, SIGTERM);

    if (sdata != NULL && sdata != (void*)-1) shmdt(sdata);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    msgctl(msgid, IPC_RMID, NULL);
}

void handle_sigint(int sig) { (void)sig; cleanup_ipc(); exit(0); }

int main() {
    srand(time(NULL));
    signal(SIGINT, handle_sigint);


    shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0600);
    if (shmid == -1) { perror("shmget"); exit(1); }
    sdata = (SharedData*)shmat(shmid, NULL, 0);


    sdata->start_time = 600;
    sdata->end_time = 610;
    sdata->current_time = sdata->start_time;
    sdata->open = true;
    sdata->emergency_exit = false;
    sdata->is_closed_for_new = false;
    sdata->kitchen_delay_us = 1000000;


    for (int i = 0; i < 6; i++) { sdata->stats_produced[i] = 0; sdata->stats_sold[i] = 0; }
    sdata->stats_special_revenue = 0;
    for (int i = 0; i < P; i++) sdata->belt[i].is_empty = true;


    semid = semget(SEM_KEY, 6, IPC_CREAT | 0600);
    semctl(semid, 0, SETVAL, 1);
    for (int i = 1; i <= 4; i++) semctl(semid, i, SETVAL, 5);
    semctl(semid, 5, SETVAL, 1);
    msgid = msgget(MSG_KEY, IPC_CREAT | 0600);


    kucharz_pid = fork();
    if (kucharz_pid == 0) execl("./kucharz", "kucharz", NULL);

    obsluga_pid = fork();
    if (obsluga_pid == 0) execl("./obsluga", "obsluga", NULL);

    kierownik_pid = fork();
    if (kierownik_pid == 0) execl("./kierownik", "kierownik", NULL);

    int client_count = 0;
    printf("[Main] Restauracja otwarta. Tp: %02d:%02d\n", sdata->start_time / 60, sdata->start_time % 60);


    while (sdata->current_time < sdata->end_time && !sdata->emergency_exit) {

        usleep(1500000 + (rand() % 3000000));

        if (sdata->current_time >= sdata->end_time || sdata->emergency_exit) break;

        if (fork() == 0) {
            char s[3], v[2];


            int is_vip = (rand() % 100 < 2);

            sprintf(s, "%d", (rand() % 4) + 1);
            sprintf(v, "%d", is_vip);

            if (is_vip) {
                printf("\n\033[1;31m[System] !!! NADCHODZI GRUPA VIP !!! (PID: %d)\033[0m\n", getpid());
            }

            execl("./klient", "klient", s, v, NULL);
            exit(0);
        }
        client_count++;
    }



    sdata->is_closed_for_new = true;
    printf("\n[Main] Godzina Tk wybita. Nie przyjmujemy nowych grup. Czekam na %d grup...\n", client_count);

    int finished_clients = 0;
    while (finished_clients < client_count) {
        pid_t p = wait(NULL);
        if (p != kucharz_pid && p != obsluga_pid && p != kierownik_pid) {
            finished_clients++;
        }
    }

    sdata->open = false;
    usleep(500000);

    printf("\n=======================================\n");
    printf("[Kucharz] RAPORT PRODUKCJI:\n");
    int ceny_prod[] = { 10, 15, 20, 40, 50, 60 };
    int suma_produkcji = 0;

    for (int i = 0; i < 6; i++) {
        int ilosc = sdata->stats_produced[i];
        int wartosc = ilosc * ceny_prod[i];
        suma_produkcji += wartosc;
        printf("Danie typ %d, %d sztuk, cena za sztuke %d\n", i + 1, ilosc, ceny_prod[i]);
    }
    printf("KOSZT PRODUKCJI: %d zl\n", suma_produkcji);
    printf("=======================================\n");
    printf("[Obsluga] Koniec zmiany\n");
    printf("=======================================\n");

    printf("[Obsluga] RAPORT FINANSOWY:\n");
    int utarg_std = sdata->stats_sold[0] * 10 + sdata->stats_sold[1] * 15 + sdata->stats_sold[2] * 20;
    int utarg_total = utarg_std + sdata->stats_special_revenue;
    printf(" przychod: %d zl\n", utarg_total);
    printf(" Jedzenie na tasmie:\n \n");

    int wartosc_strat = 0;
    for (int i = 0; i < P; i++) {
        if (!sdata->belt[i].is_empty) {
            int p = sdata->belt[i].price;
            int typ = 0;
            // Mapowanie ceny na typ
            if (p == 10) typ = 1; else if (p == 15) typ = 2; else if (p == 20) typ = 3;
            else if (p == 40) typ = 4; else if (p == 50) typ = 5; else if (p == 60) typ = 6;

            printf("Pozycja %2d: Danie typu %d o wartosci %d zl\n", i, typ, p);
            wartosc_strat += p;
        }
    }
    printf("\nLACZNA WARTOSC STRAT: %d zl\n", wartosc_strat);
    printf("=======================================\n");
    cleanup_ipc();
    return 0;
}