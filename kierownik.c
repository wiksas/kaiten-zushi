#include "common.h"

SharedData* sdata = NULL;

void handle_sig1(int sig) {
    if (sdata) {
        sdata->kitchen_delay_us /= 2;
        printf("\n[Kierownik] Polecenie: PRZYSPIESZAMY WYDAWANIE DAN! (Aktualny delay: %d us)\n", sdata->kitchen_delay_us);
    }
}

void handle_sig2(int sig) {
    if (sdata) {
        sdata->kitchen_delay_us *= 2;
        printf("\n[Kierownik] Polecenie: ZWALNIAMY WYDAWANIE DAN! (Aktualny delay: %d us)\n", sdata->kitchen_delay_us);
    }
}

void handle_sig3(int sig) {
    if (sdata) {
        sdata->emergency_exit = true;
        sdata->open = false;
    }
    printf("\n\033[1;31m[Kierownik] ALARM! EWAKUACJA I ZAMKNIECIE RESTAURACJI!\033[0m\n");
}

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    if (shmid == -1) { perror("Blad shmget w kierownik.c"); exit(1); }

    sdata = (SharedData*)shmat(shmid, NULL, 0);
    if (sdata == (void*)-1) { perror("Blad shmat w kierownik.c"); exit(1); }

    signal(SIGUSR1, handle_sig1);
    signal(SIGUSR2, handle_sig2);
    signal(SIGALRM, handle_sig3);

    printf("[Kierownik] Zarzadca uruchomiony. PID: %d. Jestem Glownym Zegarem.\n", getpid());



    printf("[Kierownik] Restauracja otwarta o godzinie %02d:%02d\n",
        sdata->start_time / 60, sdata->start_time % 60);


    while (sdata->open && !sdata->emergency_exit) {

        sleep(1);
        sdata->current_time += 1;

        printf("[ZEGAR] Godzina: %02d:%02d\n",
            sdata->current_time / 60, sdata->current_time % 60);

        if (sdata->current_time >= sdata->end_time && !sdata->is_closed_for_new) {
            sdata->is_closed_for_new = true;
            printf("\n\033[1;33m[Kierownik] Godzina %02d:%02d. ZAMYKAMY BILETOMAT (Brak nowych wstepow).\033[0m\n",
                sdata->end_time / 60, sdata->end_time % 60);
        }
    }

    printf("[Kierownik] Main zamknal lokal (open=false). Koncze prace.\n");

    shmdt(sdata);
    return 0;
}