#include "common.h"

int table_size, table_idx;
//watki
pthread_mutex_t group_lock = PTHREAD_MUTEX_INITIALIZER;
int eaten_total = 0;
int total_paid = 0;
int target_to_eat;
int msgid;
int pending_special_orders = 0;

typedef struct {
    int id;
    int age;
} PersonInfo;

void* person(void* arg) {
    PersonInfo* info = (PersonInfo*)arg;
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    SharedData* sd = shmat(shmid, NULL, 0);
    int semid = semget(SEM_KEY, 6, 0600);


    usleep(rand() % 500000);
    if (rand() % 100 < 20) {
        SpecialOrder order;
        order.mtype = getpid();
        order.price = 40 + (rand() % 3 * 10);


        if (msgsnd(msgid, &order, sizeof(int), IPC_NOWAIT) == 0) {
            pthread_mutex_lock(&group_lock);
            pending_special_orders++;
            pthread_mutex_unlock(&group_lock);
            printf("\033[1;35m[Klient %d-%d] Zamowil danie specjalne za %d zl. Czekamy...\033[0m\n",
                getpid(), info->id, order.price);
            fflush(stdout);
        }
    }

    while (!sd->emergency_exit && (eaten_total < target_to_eat || pending_special_orders > 0)) {


        usleep(100000 + (rand() % 900000));

        sem_op(semid, 5, -1);

        if (!sd->belt[table_idx].is_empty) {
            bool matches_pid = (sd->belt[table_idx].target_table_pid == getpid());
            bool is_general = (sd->belt[table_idx].target_table_pid == -1);

            if (matches_pid || (is_general && eaten_total < target_to_eat)) {
                int p = sd->belt[table_idx].price;

                if (matches_pid) {
                    printf("\033[1;35m[Klient %d-%d] ODEBRALEM moje specjalne (%d zl)!\033[0m\n",
                        getpid(), info->id, p);
                }
                else {
                    printf("\033[1;34m[Klient %d-%d] Zjada standardowy talerzyk (%d zl)\033[0m\n",
                        getpid(), info->id, p);
                }
                fflush(stdout);

                sd->belt[table_idx].is_empty = true;

                pthread_mutex_lock(&group_lock);
                eaten_total++;
                total_paid += p;

                if (matches_pid) pending_special_orders--;

                if (p == 10) sd->stats_sold[0]++;
                else if (p == 15) sd->stats_sold[1]++;
                else if (p == 20) sd->stats_sold[2]++;
                else sd->stats_special_revenue += p;

                pthread_mutex_unlock(&group_lock);
            }
        }
        sem_op(semid, 5, 1);
        usleep(500000);
    }

    shmdt(sd);
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 3) exit(1);


    srand(time(NULL) ^ getpid());

    table_size = atoi(argv[1]);
    int is_vip = atoi(argv[2]);
    table_idx = rand() % P;
    target_to_eat = (rand() % 5) + 3;

    PersonInfo info[table_size];
    int adults = 0, kids = 0;
    for (int i = 0; i < table_size; i++) {
        info[i].id = i + 1;
        info[i].age = (rand() % 70) + 1;
        if (info[i].age <= 10) kids++; else adults++;
    }


    if ((kids > 0 && adults == 0) || (kids > 3 * adults)) {
        printf("[Grupa %d] Nie spelniamy wymogow opieki. Odchodzimy.\n", getpid());
        exit(0);
    }

    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    SharedData* sdata = shmat(shmid, NULL, 0);
    int semid = semget(SEM_KEY, 6, 0600);
    msgid = msgget(MSG_KEY, 0600);

    if (!is_vip) {
        if (sdata->is_closed_for_new) { shmdt(sdata); exit(0); }
        sem_op(semid, 0, -1);
        sleep(1);
        sem_op(semid, 0, 1);
    }

    sem_op(semid, table_size, -1);
    printf("\033[1;32m[Grupa %d] WSZEDLEM! Stolik %d-os przy pozycji %d\033[0m\n",
        getpid(), table_size, table_idx);
    fflush(stdout);

    pthread_t th[table_size];
    for (int i = 0; i < table_size; i++) pthread_create(&th[i], NULL, person, &info[i]);
    for (int i = 0; i < table_size; i++) pthread_join(th[i], NULL);



    printf("\n\033[1;33m[Grupa %d] KONIEC JEDZENIA. Laczny rachunek: %d zl. Wychodzimy.\033[0m\n",
        getpid(), total_paid);
    fflush(stdout);

    sem_op(semid, table_size, 1);
    shmdt(sdata);
    return 0;
}