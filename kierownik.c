#include "common.h"

SharedData* sdata = NULL;


void handle_sig1(int sig) {
    if (sdata) {
        sdata->kitchen_delay_us /= 2;
        printf("\n[Kierownik] Polecenie: PRZYŒPIESZAMY WYDAWANIE DAÑ! (Aktualny delay: %d us)\n", sdata->kitchen_delay_us);
    }
}

void handle_sig2(int sig) {
    if (sdata) {
        sdata->kitchen_delay_us *= 2;
        printf("\n[Kierownik] Polecenie: ZWALNIAMY WYDAWANIE DAÑ! (Aktualny delay: %d us)\n", sdata->kitchen_delay_us);
    }
}

void handle_sig3(int sig) {
    if (sdata) {
        sdata->emergency_exit = true;
        sdata->open = false;
    }
    printf("\n\033[1;31m[Kierownik] ALARM! EWAKUACJA I ZAMKNIÊCIE RESTAURACJI!\033[0m\n");
}

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    if (shmid == -1) {
        perror("B³¹d shmget w kierownik.c");
        exit(1);
    }

    sdata = (SharedData*)shmat(shmid, NULL, 0);
    if (sdata == (void*)-1) {
        perror("B³¹d shmat w kierownik.c");
        exit(1);
    }

    signal(SIGUSR1, handle_sig1);
    signal(SIGUSR2, handle_sig2);
    signal(SIGALRM, handle_sig3);

    printf("[KIEROWNIK] Zarz¹dca uruchomiony. PID: %d. Czekam na sygna³y (SIGUSR1, SIGUSR2, SIGALRM).\n", getpid());

    sdata->current_time = sdata->start_time;

    printf("[Kierownik] Restauracja otwarta o godzinie %02d:%02d\n",
        sdata->start_time / 60, sdata->start_time % 60);

    while (sdata->current_time < sdata->end_time && !sdata->emergency_exit) {
        sleep(1);
        sdata->current_time += 1;

        printf("[ZEGAR] Godzina: %02d:%02d\n",
            sdata->current_time / 60, sdata->current_time % 60);
    }


    if (!sdata->emergency_exit) {
        sdata->is_closed_for_new = true;
        printf("[Kierownik] Godzina %02d:%02d osi¹gniêta. Zamykamy biletomat. Koñczymy obs³ugê obecnych goœci.\n",
            sdata->end_time / 60, sdata->end_time % 60);
    }

    shmdt(sdata);
    return 0;
}