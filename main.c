#include <stdio.h>
#include <threads.h>
#include <semaphore.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdbool.h>

#define NAME_LENGTH 256
#define STOP_SKIERS_NAME "/waitings"
#define STOP_SKIERS_LENGTH(n) n * sizeof(int)
#define LOG_COUNT_NAME "/A"
#define LOG_COUNT_LENGTH sizeof(int)
#define MUTEX_NAME "/mutex"
#define BOARDED_SEMAPHORE_NAME "/boarded"

void format_name(char* const str, const int len, const int id)
{
    snprintf(str, len, "/bus%d", id);
}

void* shared_get(const char* name, const int length, const bool truncate)
{
    const int fd = shm_open(name, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);

    if(truncate)
    {
        ftruncate(fd, length);
    }

    int* p = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd);

    return p;
}

int fn_bus(const int Z, const int K, const int TB)
{
    int* stop_skiers = shared_get(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z), false);
    int* ptr_log_count = shared_get(LOG_COUNT_NAME,LOG_COUNT_LENGTH, false);

    sem_t* mutex = sem_open(MUTEX_NAME, O_CREAT, 0755, 1);
    sem_t* boarded_semaphore = sem_open(BOARDED_SEMAPHORE_NAME, O_CREAT, 0755, 0);
    sem_t* stop_semaphores[Z];

    sem_wait(mutex);
    printf("%d: bus started\n", ++*ptr_log_count);
    sem_post(mutex);

    for(int i = 0; i < Z; i++)
    {
        char name[NAME_LENGTH];
        format_name(name, NAME_LENGTH, i);

        stop_semaphores[i] = sem_open(name, O_CREAT, 0755, 0);
    }

    bool go_back;

    do {
        int free_seats = K;

        go_back = false;

        for(int j = 0; j < Z; j++) {

            usleep(rand() % TB);

            sem_wait(mutex);
            printf("%d: bus arrived to %d\n", ++*ptr_log_count, j + 1);
            const int n = (stop_skiers[j] > free_seats) ? free_seats: stop_skiers[j];
            for (int i = 0; i < n; i++) {
                sem_post(stop_semaphores[j]);
                sem_wait(boarded_semaphore);
            }

            free_seats -= n;

            stop_skiers[j] -= n;

            if(stop_skiers[j] > 0)
            {
                go_back = true;
            }

            printf("%d: bus leaving  %d\n", ++*ptr_log_count, j + 1);

            sem_post(mutex);

        }

        sem_wait(mutex);
        printf("%d: bus arrived to final\n", ++*ptr_log_count);
        sem_post(mutex);

        // TODO synchronizace vystupu na konecne

        sem_wait(mutex);
        printf("%d: bus leaving final\n", ++*ptr_log_count);
        sem_post(mutex);

    } while(go_back);

    sem_wait(mutex);
    printf("%d: bus finish\n", ++*ptr_log_count);
    sem_post(mutex);

    sem_close(mutex);
    sem_close(boarded_semaphore);

    for(int i = 0; i < Z; i++)
    {
        sem_close(stop_semaphores[i]);
    }

    return 0;
}

int fn_rider(const int rider_id, const int Z, const int TL) {

    const int bus_stop_index = rand() % Z;

    // Ziskani sdilene pameti

    int* stop_skiers = shared_get(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z), false);
    int* ptr_log_count = shared_get(LOG_COUNT_NAME,LOG_COUNT_LENGTH, false);

    // Ziskani semaforu

    sem_t* mutex = sem_open(MUTEX_NAME, O_CREAT, 0755, 1);
    sem_t* boarded = sem_open(BOARDED_SEMAPHORE_NAME, O_CREAT, 0755, 0);

    char name[NAME_LENGTH];
    format_name(name, NAME_LENGTH, bus_stop_index);
    sem_t* bus = sem_open(name, O_CREAT, 0755, 0);

    // Vlastni algoritmus

    sem_wait(mutex);
    printf("%d: L %d started\n", ++*ptr_log_count, rider_id);
    sem_post(mutex);

    usleep(rand() % TL); // nahodne trvani snidane

    sem_wait(mutex);
    stop_skiers[bus_stop_index]++;
    printf("%d: L %d arrived to %d\n", ++*ptr_log_count, rider_id, bus_stop_index + 1);
    sem_post(mutex);

    sem_wait(bus);
    printf("%d: L %d boarded\n", ++*ptr_log_count, rider_id);
    sem_post(boarded);

    sem_close(mutex);
    sem_close(boarded);
    sem_close(bus);

    munmap(LOG_COUNT_NAME, LOG_COUNT_LENGTH);
    munmap(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z));

    return 0;
}

int main(const int argc, char *argv[])
{
    // Zpracovani vstupnich argumentu

    if(argc != 6)
    {
        return -1;
    }

    const int L = atoi(argv[1]);
    const int Z = atoi(argv[2]);
    const int K = atoi(argv[3]);
    const int TL = atoi(argv[4]);
    const int TB = atoi(argv[5]);

    // Inicializace sdilene pameti
    int* bus_stop_skiers = shared_get(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z), true);

    for(int i = 0; i < Z; i++)
    {
        bus_stop_skiers[i] = 0;
    }

    int* ptr_log_count = shared_get(LOG_COUNT_NAME,LOG_COUNT_LENGTH, true);

    *ptr_log_count = 0;

    // Spusteni procesu vcetne inicializace nahodnych cisel

    for(int i = 0; i < L; i++)
    {
        const pid_t p = fork();

        if(p == 0)
        {
            time_t t;
            srand((unsigned)time(&t) ^ getpid());

            fn_rider(i + 1, Z, TL);

            exit(0);
        }
    }

    pid_t p = fork();

    if(p == 0)
    {
        time_t t;
        srand((unsigned)time(&t) ^ getpid());

        fn_bus(Z, K, TB);

        exit(0);
    }

    // Cekani na dokonceni procesu

    int status;

    while((wait(&status)) > 0)
    {

    }

    // Uvolneni sdilene pameti a mutexu

    shm_unlink(LOG_COUNT_NAME);
    shm_unlink(STOP_SKIERS_NAME);

    sem_unlink(MUTEX_NAME);
    sem_unlink(BOARDED_SEMAPHORE_NAME);

    for(int i = 0; i < Z; i++)
    {
        char name[NAME_LENGTH];
        format_name(name, NAME_LENGTH, i);

        sem_unlink(name);
    }
}