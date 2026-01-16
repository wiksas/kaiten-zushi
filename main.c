#include "common.h"
#include <time.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

int shmid, semid, msgid;
SharedData* sdata = NULL;
pid_t kucharz_pid, obsluga_pid, kierownik_pid;

volatile sig_atomic_t stop_request = 0;

void handle_timeout(int sig) {
    printf("\n\033[1;31m[Main] ALARM! Czas bezpieczenstwa minal.\033[0m\n");
    if (sdata != NULL && sdata != (void*)-1) sdata->emergency_exit = true;
    stop_request = 1;
}

void handle_sigint(int sig) {
    printf("\n\033[1;33m[Main] SIGINT. Koncze wpuszczanie.\033[0m\n");
    if (sdata != NULL && sdata != (void*)-1) sdata->emergency_exit = true;
    stop_request = 1;
}

void cleanup_system() {
    printf("[Main] Sprzatanie procesow pracownikow...\n");
    if (kucharz_pid > 0) kill(kucharz_pid, SIGTERM);
    if (obsluga_pid > 0) kill(obsluga_pid, SIGTERM);
    if (kierownik_pid > 0) kill(kierownik_pid, SIGTERM);

    if (sdata != NULL && sdata != (void*)-1) shmdt(sdata);
    
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    msgctl(msgid, IPC_RMID, NULL);
    
    printf("[Main] Zasoby IPC usuniete.\n");
}

int main() {
    srand(time(NULL));
    
    signal(SIGINT, handle_sigint);
    signal(SIGALRM, handle_timeout);

    //zombie
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa, NULL);

    shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0600);
    if (shmid == -1) { perror("shmget"); return 1; }
    sdata = (SharedData*)shmat(shmid, NULL, 0);

    sdata->start_time = 600;
    sdata->end_time = 1320;
    sdata->current_time = sdata->start_time;
    sdata->open = true;
    sdata->emergency_exit = false;
    sdata->is_closed_for_new = false;
    sdata->kitchen_delay_us = 0;

    for (int i = 0; i < 6; i++) { sdata->stats_produced[i] = 0; if (i < 3) sdata->stats_sold[i] = 0; }
    sdata->stats_special_revenue = 0;
    sdata->stats_tips = 0;


    int th_lada = NUM_LADA;
    int th_1os  = th_lada + NUM_1OS;
    int th_2os  = th_1os  + NUM_2OS;
    int th_3os  = th_2os  + NUM_3OS;


    for (int i = 0; i < P; i++) {
        sdata->belt[i].is_empty = true;
        sdata->belt[i].target_table_pid = -1;
        sdata->current_occupancy[i] = 0;
        
        if (i < th_lada)      sdata->table_capacity[i] = 1; // Lada
        else if (i < th_1os)  sdata->table_capacity[i] = 1; // 1-os
        else if (i < th_2os)  sdata->table_capacity[i] = 2; // 2-os
        else if (i < th_3os)  sdata->table_capacity[i] = 3; // 3-os
        else                  sdata->table_capacity[i] = 4; // 4-os
    }

    printf("[Main] Konfiguracja Lokalu (Total: %d):\n", P);
    printf(" - Lada: %d\n - 1-os: %d\n - 2-os: %d\n - 3-os: %d\n - 4-os: %d\n",
           NUM_LADA, NUM_1OS, NUM_2OS, NUM_3OS, NUM_4OS);


    semid = semget(SEM_KEY, 8, IPC_CREAT | 0600);
    
    semctl(semid, 0, SETVAL, 1);       // Mutex taśmy
    semctl(semid, 1, SETVAL, 0);       // Kucharz
    semctl(semid, 2, SETVAL, NUM_2OS * 2);   
    semctl(semid, 3, SETVAL, NUM_3OS * 3);
    semctl(semid, 4, SETVAL, NUM_4OS * 4);
    semctl(semid, 5, SETVAL, 1);       // Kasjer
    semctl(semid, 6, SETVAL, NUM_LADA);
    semctl(semid, 7, SETVAL, NUM_1OS); 

    msgid = msgget(MSG_KEY, IPC_CREAT | 0600);

    if ((kucharz_pid = fork()) == 0) { execl("./kucharz", "kucharz", NULL); exit(1); }
    if ((obsluga_pid = fork()) == 0) { execl("./obsluga", "obsluga", NULL); exit(1); }
    if ((kierownik_pid = fork()) == 0) { execl("./kierownik", "kierownik", NULL); exit(1); }

    int client_count = 0;
    printf("[Main] RESTAURACJA OTWARTA.\n");

while (sdata->current_time < sdata->end_time && !sdata->emergency_exit && !stop_request) {

        usleep(1000000); 
        
        if (sdata->current_time >= sdata->end_time || sdata->emergency_exit || stop_request) break;

        if (client_count >= MAX_CLIENTS) {
            printf("\033[1;31m[Main] WYJATEK: Osiagnieto limit %d procesow klientow! Zamykam wejscie.\033[0m\n", MAX_CLIENTS);
            
            sdata->is_closed_for_new = true; 
            break; 
        }

        if ((rand() % 100) < 30) {
            pid_t pid = fork();
            
            if (pid == -1) {
                perror("[Main] Blad fork (brak zasobow)");
                sdata->emergency_exit = true; 
                break;
            }
            
            if (pid == 0) {
                char s[3], v[2];
                int g_size = (rand() % 4) + 1;
                int is_vip = (rand() % 100 < 50);
                sprintf(s, "%d", g_size);
                sprintf(v, "%d", is_vip);
                
                execl("./klient", "klient", s, v, NULL);
                perror("[Main] Blad execl");
                exit(1);
            }
            
            if (pid > 0) {
                client_count++;

            }
        }
    }

    sdata->is_closed_for_new = true;
    printf("\n\033[1;33m[Main] ZAMYKAMY WEJSCIE. Czekam na opróżnienie lokalu...\033[0m\n");

    while (1) {
        int total_occupancy = 0;
        struct sembuf sb_lock = {0, -1, 0}; semop(semid, &sb_lock, 1);
        for(int i=0; i<P; i++) total_occupancy += sdata->current_occupancy[i];
        struct sembuf sb_unlock = {0, 1, 0}; semop(semid, &sb_unlock, 1);

        if (total_occupancy == 0) break;
        sleep(1); 
    }

    sdata->open = false;
    sleep(1);

    printf("\n\033[1;32m=======================================\n");
    printf("        RAPORT KONCOWY DNIA          \n");
    printf("=======================================\033[0m\n");

    printf("[Kucharz] STATYSTYKA PRODUKCJI:\n");
    int ceny[] = { 10, 15, 20, 40, 50, 60 };
    int suma_kosztow = 0;
    for (int i = 0; i < 6; i++) {
        int ilosc = sdata->stats_produced[i];
        int wartosc = ilosc * ceny[i];
        suma_kosztow += wartosc;
        printf(" - Danie Typ %d: %2d szt. (Koszt: %3d zl)\n", i + 1, ilosc, wartosc);
    }
    printf(" LACZNY KOSZT PRODUKCJI: %d zl\n", suma_kosztow);

    printf("---------------------------------------\n");
    printf("[Kasjer] STATYSTYKA SPRZEDAZY:\n");
    int przychod_std = 0;
    for (int i = 0; i < 3; i++) przychod_std += sdata->stats_sold[i] * ceny[i];
    printf(" - Sprzedaz Dan Podstawowych: %d zl\n", przychod_std);
    printf(" - Sprzedaz Dan Specjalnych : %d zl\n", sdata->stats_special_revenue);
    printf(" - Napiwki (VIP)     : %d zl\n", sdata->stats_tips);
    int utarg_total = przychod_std + sdata->stats_special_revenue + sdata->stats_tips;
    printf(" CALKOWITY PRZYCHOD: %d zl\n", utarg_total);

    printf("---------------------------------------\n");
    printf("[Sprzatacz] STRATY (Jedzenie na tasmie):\n");
    int straty = 0;
    for (int i = 0; i < P; i++) {
        if (!sdata->belt[i].is_empty) {
            printf(" - Slot %2d: %d zl\n", i, sdata->belt[i].price);
            straty += sdata->belt[i].price;
        }
    }
    printf(" ZMARNOWANE JEDZENIE: %d zl\n", straty);
    printf("=======================================\n");

    cleanup_system();
    printf("[Main] System zamkniety poprawnie.\n");
    return 0;
}