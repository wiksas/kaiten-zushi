#include "common.h"

int table_idx = -1;
int group_size;
bool sitting_at_bar = false;

pthread_mutex_t group_lock = PTHREAD_MUTEX_INITIALIZER;
int eaten_total = 0;
int total_paid = 0;
int target_to_eat;
int msgid;
int pending_special_orders = 0;

typedef struct { int id; int age; } PersonInfo;

void* person(void* arg) {
    PersonInfo* info = (PersonInfo*)arg;
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    SharedData* sd = shmat(shmid, NULL, 0);
    int semid = semget(SEM_KEY, 7, 0600);
    int eating_time = sitting_at_bar ? 200000 : 500000;

    usleep(rand() % 500000);

    if (!sitting_at_bar && rand() % 100 < 20) {
        SpecialOrder order;
        order.mtype = getpid();
        order.price = 40 + (rand() % 3 * 10);

        if (msgsnd(msgid, &order, sizeof(int), IPC_NOWAIT) == 0) {
            pthread_mutex_lock(&group_lock);
            pending_special_orders++;
            pthread_mutex_unlock(&group_lock);

            printf("\033[1;35m[Klient %d-%d] >> ZAMAWIA danie specjalne (%d zl)\033[0m\n",
                getpid(), info->id, order.price);
            fflush(stdout);
        }
    }

    while (!sd->emergency_exit && (eaten_total < target_to_eat || pending_special_orders > 0)) {
        usleep(100000 + (rand() % 300000));
        sem_op(semid, 5, -1);
        if (!sd->belt[table_idx].is_empty) {
            bool matches = (sd->belt[table_idx].target_table_pid == getpid());
            bool general = (sd->belt[table_idx].target_table_pid == -1);

            if (matches || (general && eaten_total < target_to_eat)) {
                int p = sd->belt[table_idx].price;
                char* typ_dania = (p >= 40) ? "SPECJALNE" : "Standard";
                char* miejsce = sitting_at_bar ? "LADA" : "STOLIK";
                char* color = sitting_at_bar ? "\033[1;36m" : (p >= 40 ? "\033[1;35m" : "\033[1;34m");

                printf("%s[Klient %d-%d] [%s] Zjada: %s (%d zl)\033[0m\n",
                    color, getpid(), info->id, miejsce, typ_dania, p);

                sd->belt[table_idx].is_empty = true;
                pthread_mutex_lock(&group_lock);
                eaten_total++;
                total_paid += p;
                if (matches) pending_special_orders--;

                if (p == 10) sd->stats_sold[0]++;
                else if (p == 15) sd->stats_sold[1]++;
                else if (p == 20) sd->stats_sold[2]++;
                else sd->stats_special_revenue += p;
                pthread_mutex_unlock(&group_lock);
            }
        }
        sem_op(semid, 5, 1);
        usleep(eating_time);
    }
    shmdt(sd);
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 3) exit(1);
    srand(time(NULL) ^ getpid());

    group_size = atoi(argv[1]);
    int is_vip = atoi(argv[2]);
    target_to_eat = (rand() % 4) + 2;

    PersonInfo info[group_size];
    for (int i = 0; i < group_size; i++) { info[i].id = i + 1; info[i].age = (rand() % 70) + 1; }

    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    SharedData* sdata = shmat(shmid, NULL, 0);
    int semid = semget(SEM_KEY, 7, 0600);
    msgid = msgget(MSG_KEY, 0600);

    if (group_size == 1 && (rand() % 100 < 50)) sitting_at_bar = true;
    else sitting_at_bar = false;

    if (!is_vip) {
        if (sdata->is_closed_for_new) { shmdt(sdata); exit(0); }
        sem_op(semid, 0, -1);
        usleep(sitting_at_bar ? 50000 : 800000);
        sem_op(semid, 0, 1);
    }

    int start_search = 0;
    int end_search = 0;
    int sem_to_take = 0;

    if (sitting_at_bar) {
        sem_to_take = 6; start_search = 0; end_search = 6;
    }
    else {
        if (group_size == 1) { sem_to_take = 2; start_search = 6; end_search = 11; }
        else if (group_size == 2) { sem_to_take = 4; start_search = 16; end_search = 20; }
        else if (group_size == 3) { sem_to_take = 3; start_search = 11; end_search = 16; }
        else { sem_to_take = 4; start_search = 16; end_search = 20; }
    }

    struct sembuf sb = { sem_to_take, -group_size, 0 };
    semop(semid, &sb, 1);

    bool dosiedlismy_sie = false;
    sem_op(semid, 5, -1);
    for (int i = start_search; i < end_search; i++) {
        int wolne = sdata->table_capacity[i] - sdata->current_occupancy[i];
        if (wolne >= group_size) {
            table_idx = i;
            if (sdata->current_occupancy[i] > 0) dosiedlismy_sie = true;
            sdata->current_occupancy[i] += group_size;
            break;
        }
    }
    sem_op(semid, 5, 1);

    if (table_idx == -1) { printf("BLAD: Brak miejsca!\n"); exit(1); }

    int capacity = sdata->table_capacity[table_idx];

    printf("\n\033[1;32m========================================\n");
    if (sitting_at_bar) {
        printf("[Grupa %d] ZAJMUJE MIEJSCE PRZY LADZIE (Nr %d)\n", getpid(), table_idx);
        printf(">> Wielkosc grupy: %d os.\n", group_size);
    }
    else if (dosiedlismy_sie) {
        printf("[Grupa %d] DOSIADA SEÊ do stolika nr %d (Pojemnosc: %d)\n", getpid(), table_idx, capacity);
        printf(">> Wielkosc grupy: %d os. (Razem przy stole: %d)\n", group_size, sdata->current_occupancy[table_idx]);
    }
    else {
        printf("[Grupa %d] ZAJMUJE PUSTY STOLIK nr %d (Pojemnosc: %d)\n", getpid(), table_idx, capacity);
        printf(">> Wielkosc grupy: %d os.\n", group_size);
    }
    printf("========================================\033[0m\n");
    fflush(stdout);

    pthread_t th[group_size];
    for (int i = 0; i < group_size; i++) pthread_create(&th[i], NULL, person, &info[i]);
    for (int i = 0; i < group_size; i++) pthread_join(th[i], NULL);

    printf("\n\033[1;33m[Grupa %d] Koniec posilku. Rachunek: %d zl. Zwalniamy %d miejsc.\033[0m\n",
        getpid(), total_paid, group_size);
    fflush(stdout);

    sem_op(semid, 5, -1);
    sdata->current_occupancy[table_idx] -= group_size;
    sem_op(semid, 5, 1);

    struct sembuf sb_out = { sem_to_take, group_size, 0 };
    semop(semid, &sb_out, 1);

    shmdt(sdata);
    return 0;
}