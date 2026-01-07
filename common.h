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

#define P 20    // Pojemnoœæ taœmy
#define SHM_KEY 123456
#define SEM_KEY 123457
#define MSG_KEY 123458

// Ceny talerzyków
#define PRICE_LOW 10
#define PRICE_MED 15
#define PRICE_HIGH 20
#define PRICE_SPECIAL 40

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
    
    // Statystyki do raportu koñcowego
    int stats_produced[3];  
    int stats_sold[3];       
    int stats_special_sold; 
} SharedData;

typedef struct {
    long mtype;             
    int price;
} SpecialOrder;



static inline void sem_op(int semid, int sem_num, int op) {
    struct sembuf sb = {sem_num, op, 0};
    if (semop(semid, &sb, 1) == -1) {
        // Jeœli semafor zosta³ usuniêty (EINVAL/EIDRM), koñczymy proces po cichu Jeœli operacja zosta³a przerwana sygna³em, próbujemy ponownie
        if (errno == EINVAL || errno == EIDRM) {
            exit(0); 
        }
        if (errno == EINTR) {
            sem_op(semid, sem_num, op);
            return;
        }
        perror("B³¹d semop");
        exit(1);
    }
}

#endif