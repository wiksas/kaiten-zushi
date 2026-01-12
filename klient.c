#include "common.h"
#include <pthread.h>
#include <signal.h>
#include <sched.h>

int table_idx = -1;
int group_size;
int is_vip_global = 0;
bool sitting_at_bar = false;

pthread_mutex_t group_lock = PTHREAD_MUTEX_INITIALIZER;
int eaten_total = 0;
int total_paid = 0;
int target_to_eat;
int msgid;
int pending_special_orders = 0;
int semid;
SharedData* sdata = NULL;

int eaten_types[6] = { 0, 0, 0, 0, 0, 0 };

typedef struct { int id; int age; } PersonInfo;

void print_bill() {
    static int already_printed = 0;
    if (already_printed) return;
    already_printed = 1;

    int napiwek = 0;
    if (is_vip_global) napiwek = (total_paid * 20) / 100;

    char* name = is_vip_global ? "VIP" : "Grupa";
    char* exit_color = is_vip_global ? "\033[1;31m" : "\033[1;33m";

    printf("\n%s[%s %d] KONIEC (Status: %s). Rachunek: %d zl.\033[0m\n",
        exit_color, name, getpid(),
        (eaten_total < target_to_eat ? "Przerwano" : "Najedzeni"),
        total_paid);

    if (napiwek > 0) printf(" NAPIWEK (VIP): %d zl | SUMA: %d zl\n", napiwek, total_paid + napiwek);

    printf(" SZCZEGOLY KONSUMPCJI:\n");
    int prices[] = { 10, 15, 20, 40, 50, 60 };
    int found_any = 0;
    for (int i = 0; i < 6; i++) {
        if (eaten_types[i] > 0) {
            printf(" - %d x Danie Typ %d (%d zl)\n", eaten_types[i], i + 1, prices[i]);
            found_any = 1;
        }
    }
    if (!found_any) printf(" - (Nic nie zjedzono)\n");
    printf("----------------------------------------\n");
    fflush(stdout);
}


void cleanup_handler(int sig) {
    print_bill();

    if (table_idx != -1 && sdata != NULL) {
        sem_op(semid, 0, -1);
        sdata->current_occupancy[table_idx] -= group_size;
        if (sdata->current_occupancy[table_idx] < 0) sdata->current_occupancy[table_idx] = 0;
        
        int napiwek = (is_vip_global) ? (total_paid * 20) / 100 : 0;
        if (napiwek > 0) sdata->stats_tips += napiwek;
        
        sem_op(semid, 0, 1);
    }

    if (sdata != NULL) shmdt(sdata);
    exit(0);
}

void* person(void* arg) {
    PersonInfo* info = (PersonInfo*)arg;
    int eating_time = sitting_at_bar ? 200000 : 500000;
    
    usleep(rand() % 500000);

    int chance = is_vip_global ? 80 : 20;

    if (!sitting_at_bar && (rand() % 100 < chance)) {
        SpecialOrder order;
        order.mtype = getpid();
        int base_price_idx = is_vip_global ? (rand() % 3) : 0;
        int prices[] = { 40, 50, 60 };
        order.price = prices[base_price_idx];

        if (msgsnd(msgid, &order, sizeof(int), IPC_NOWAIT) == 0) {
            pthread_mutex_lock(&group_lock);
            pending_special_orders++;
            pthread_mutex_unlock(&group_lock);
        }
    }

    while (!sdata->emergency_exit && (eaten_total < target_to_eat || pending_special_orders > 0)) {
        
        usleep(100000 + (rand() % 300000));

        sem_op(semid, 0, -1);

        if (!sdata->belt[table_idx].is_empty) {
            int p = sdata->belt[table_idx].price;
            bool is_my_special = (sdata->belt[table_idx].target_table_pid == getpid());
            bool is_generic = (sdata->belt[table_idx].target_table_pid == -1);
            bool is_basic_food = (p <= 20);

            if (is_my_special || (is_generic && eaten_total < target_to_eat && is_basic_food)) {

                char* typ_dania = (p >= 40) ? "SPECJALNE" : "Standard";
                char* miejsce = sitting_at_bar ? "LADA" : "STOLIK";
                char* color;
                if (is_vip_global) color = "\033[1;31m";
                else if (sitting_at_bar) color = "\033[1;36m";
                else color = (p >= 40 ? "\033[1;35m" : "\033[1;34m");

                printf("%s[%s %d-%d (%d lat)] [%s] Zjada: %s (%d zl)\033[0m\n",
                    color, (is_vip_global ? "VIP" : "Klient"), getpid(), info->id, info->age, miejsce, typ_dania, p);
                
                sdata->belt[table_idx].is_empty = true;

                pthread_mutex_lock(&group_lock);
                eaten_total++;
                total_paid += p;

                if (is_my_special) pending_special_orders--;

                int type_idx = -1;
                if (p == 10) type_idx = 0; else if (p == 15) type_idx = 1; else if (p == 20) type_idx = 2;
                else if (p == 40) type_idx = 3; else if (p == 50) type_idx = 4; else if (p == 60) type_idx = 5;

                if (type_idx != -1) {
                    eaten_types[type_idx]++;
                    if (type_idx < 3) sdata->stats_sold[type_idx]++;
                    else sdata->stats_special_revenue += p;
                }
                pthread_mutex_unlock(&group_lock);
            }
        }
        sem_op(semid, 0, 1);
        
        sched_yield();
        usleep(eating_time);
    }
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 3) exit(1);
    srand(time(NULL) ^ getpid());

    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);

    group_size = atoi(argv[1]);
    is_vip_global = atoi(argv[2]);

    target_to_eat = 3 + (rand() % 8);
    if (is_vip_global) target_to_eat += 2;

    PersonInfo info[group_size];

    // --- LOGIKA WIEKU (OPIEKA NAD DZIEĆMI) ---
    for (int i = 0; i < group_size; i++) { 
        info[i].id = i + 1; 
        info[i].age = (rand() % 60) + 1; 
    }
    while (true) {
        int children = 0, adults = 0;
        for (int i = 0; i < group_size; i++) {
            if (info[i].age < 10) children++;
            else if (info[i].age >= 18) adults++;
        }
        int needed = (children + 2) / 3; 
        if (children == 0) needed = 0;

        if (adults >= needed) break;
        else {
            for (int i = 0; i < group_size; i++) {
                if (info[i].age < 18) {
                    info[i].age = 18 + (rand() % 30);
                    break; 
                }
            }
        }
    }

    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0600);
    if (shmid == -1) { perror("shmget"); exit(1); }
    sdata = shmat(shmid, NULL, 0);
    semid = semget(SEM_KEY, 7, 0600);
    msgid = msgget(MSG_KEY, 0600);

    if (group_size == 1 && (rand() % 100 < 50) && !is_vip_global) sitting_at_bar = true;
    else sitting_at_bar = false;

    if (is_vip_global) {
        printf("\n\033[1;31m[VIP %d] WCHODZI DO RESTAURACJI BEZ KOLEJKI!\033[0m\n", getpid());
    }
    else {
        if (sdata->is_closed_for_new) { shmdt(sdata); exit(0); }
        
        sem_op(semid, 0, -1);
        usleep(sitting_at_bar ? 50000 : 800000);
        sem_op(semid, 0, 1);
    }

    int start_search = 0, end_search = 0, sem_to_take = 0;

    if (sitting_at_bar) {
        sem_to_take = 6; start_search = 0; end_search = 6;
    }
    else {
        int r = rand() % 100;
        if (group_size == 1) { 
            // 40% na 2-os, 30% na 3-os, 30% na 4-os
            if (r < 40) { sem_to_take = 2; start_search = 6; end_search = 11; }
            else if (r < 70) { sem_to_take = 3; start_search = 11; end_search = 16; }
            else { sem_to_take = 4; start_search = 16; end_search = 20; }
        }
        else if (group_size == 2) { 
            // 50% na 2-os, 50% na 4-os
            if (r < 50) { sem_to_take = 2; start_search = 6; end_search = 11; }
            else { sem_to_take = 4; start_search = 16; end_search = 20; }
        }
        else if (group_size == 3) { 
            sem_to_take = 3; start_search = 11; end_search = 16; 
        }
        else { 
            sem_to_take = 4; start_search = 16; end_search = 20; 
        }
    }

    sem_op(semid, sem_to_take, -group_size);

    bool dosiedlismy_sie = false;
    sem_op(semid, 0, -1);


    for (int i = start_search; i < end_search; i++) {
        int wolne = sdata->table_capacity[i] - sdata->current_occupancy[i];
        if (wolne >= group_size) {
            table_idx = i;
            if (sdata->current_occupancy[i] > 0) dosiedlismy_sie = true;
            sdata->current_occupancy[i] += group_size;
            break;
        }
    }
    sem_op(semid, 0, 1);

    if (table_idx == -1) {
        sem_op(semid, sem_to_take, group_size);
        exit(1);
    }

    int capacity = sdata->table_capacity[table_idx];
    char* name = is_vip_global ? "VIP" : "Grupa";
    char* color_header = is_vip_global ? "\033[1;31m" : "\033[1;32m";


    printf("\n%s========================================\n", color_header);
    if (sitting_at_bar) {
        printf("[%s %d] ZAJMUJE MIEJSCE PRZY LADZIE (Nr %d)\n", name, getpid(), table_idx);
    }
    else if (dosiedlismy_sie) {
        printf("[%s %d] DOSIADA SIĘ do stolika nr %d (Pojemnosc: %d)\n", name, getpid(), table_idx, capacity);
    }
    else {
        char* extra = "";
        if (capacity > group_size) extra = " (LUZNY STOLIK)";
        printf("[%s %d] ZAJMUJE PUSTY STOLIK nr %d (Pojemnosc: %d)%s\n", name, getpid(), table_idx, capacity, extra);
    }
    
    printf(">> Wielkosc: %d os. CEL DO ZJEDZENIA: %d DAN.\n", group_size, target_to_eat);
    printf(">> Wiek: ");
    for(int i=0; i<group_size; i++) printf("%d ", info[i].age);
    printf("\n");
    
    printf("========================================\033[0m\n");
    fflush(stdout);

    pthread_t th[group_size];
    for (int i = 0; i < group_size; i++) pthread_create(&th[i], NULL, person, &info[i]);
    for (int i = 0; i < group_size; i++) pthread_join(th[i], NULL);

    sem_op(semid, 0, -1);
    sdata->current_occupancy[table_idx] -= group_size;

    int napiwek = (is_vip_global) ? (total_paid * 20) / 100 : 0;
    if (napiwek > 0) sdata->stats_tips += napiwek;

    sem_op(semid, 0, 1);
    sem_op(semid, sem_to_take, group_size);

    print_bill();

    shmdt(sdata);
    return 0;
}