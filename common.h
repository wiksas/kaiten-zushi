#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>

#define P 20
#define LADA_LIMIT 6 

#define SHM_KEY 123456
#define SEM_KEY 123457
#define MSG_KEY 123458
#define LOG_FILE "logi.txt"

typedef struct {
    int price;
    int target_table_pid;
    bool is_empty;
} Plate;

typedef struct {
    Plate belt[P];
    int kitchen_delay_us;
    bool open;
    bool emergency_exit;
    int current_time;
    int start_time;
    int end_time;
    bool is_closed_for_new;

    int current_occupancy[P];
    int table_capacity[P];

    // Statystyki
    int stats_produced[6];
    int stats_sold[6];
    int stats_special_revenue;
    int stats_tips;
} SharedData;

typedef struct {
    long mtype;
    int price;
} SpecialOrder;


static inline void print_and_log(const char* format, ...) {
    va_list args;

    // 1. Wypisz na ekran (stdout)
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // 2. Wypisz do pliku
    FILE* file = fopen(LOG_FILE, "a");
    if (file != NULL) {
        va_start(args, format);
        vfprintf(file, format, args);
        va_end(args);
        fclose(file);
    }
}

#define printf(...) print_and_log(__VA_ARGS__)

static inline void sem_op(int semid, int sem_num, int op) {
    struct sembuf sb = { sem_num, op, 0 };
    if (semop(semid, &sb, 1) == -1) {
        if (errno == EINVAL || errno == EIDRM) exit(0);
        if (errno == EINTR) { sem_op(semid, sem_num, op); return; }
    }
}

#endif