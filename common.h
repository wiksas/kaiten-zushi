#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_BELT 20
#define MAX_TABLES 10

typedef struct {
    int price;         
    int target_table; 
    bool is_empty;
} Plate;

typedef struct {
    Plate belt[MAX_BELT];
    int current_clients;
    bool restaurant_open;
    int speed_multiplier; 
} SharedData;


typedef struct {
    int id;
    int age;
    int eaten_count;
} ClientMember;