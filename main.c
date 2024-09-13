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

void* create_shared(const char* name, const int length)
{
    const int fd = shm_open(name, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);

    ftruncate(fd, length);

    int* p = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd);

    return p;
}

void* get_shared(const char* name, const int length)
{
    const int fd = shm_open(name, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);

    int* p = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd);

    return p;
}

void create_semaphore(const char* const name, const int init)
{
    sem_open(name, O_CREAT, 0755, init);
}

sem_t* get_semaphore(const char* const name)
{
    sem_t* semaphore = sem_open(name, 0);

    return semaphore;
}

int fn_bus(const int Z, const int K, const int TB)
{
    int* stop_skiers = get_shared(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z));
    int* ptr_log_count = get_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);

    sem_t* mutex = get_semaphore(MUTEX_NAME);
    sem_t* boarded_semaphore = get_semaphore(BOARDED_SEMAPHORE_NAME);

    sem_t* stop_semaphores[Z];

    for(int i = 0; i < Z; i++)
    {
        char name[NAME_LENGTH];
        format_name(name, NAME_LENGTH, i);

        stop_semaphores[i] = get_semaphore(name);
    }

    sem_wait(mutex);
    printf("%d: bus started\n", ++*ptr_log_count);
    sem_post(mutex);

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

    // Close semaphores

    sem_close(mutex);
    sem_close(boarded_semaphore);

    for(int i = 0; i < Z; i++)
    {
        sem_close(stop_semaphores[i]);
    }

    return 0;
}

int fn_rider(const int rider_id, const int Z, const int TL)
{

    // Nahodne priradim lyzare k zastavce

    const int bus_stop_index = rand() % Z;

    // Ziskani sdilene pameti

    int* stop_skiers = get_shared(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z));
    int* ptr_log_count = get_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);

    // Ziskani semaforu

    sem_t* mutex = get_semaphore(MUTEX_NAME);
    sem_t* boarded = get_semaphore(BOARDED_SEMAPHORE_NAME);

    char name[NAME_LENGTH];
    format_name(name, NAME_LENGTH, bus_stop_index);

    sem_t* bus = get_semaphore(name);

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

    // Zavreni semaforu a odmapovani sdilene pameti

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

    int* bus_stop_skiers = create_shared(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z));

    for(int i = 0; i < Z; i++)
    {
        bus_stop_skiers[i] = 0;
    }

    int* ptr_log_count = create_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);

    *ptr_log_count = 0;

    // Inicializace semaforu

    create_semaphore(MUTEX_NAME, 1);
    create_semaphore(BOARDED_SEMAPHORE_NAME, 0);

    for(int i = 0; i < Z; i++)
    {
        char name[NAME_LENGTH];
        format_name(name, NAME_LENGTH, i);

        create_semaphore(name, 0);
    }

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

    const pid_t p = fork();

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

    // Uvolneni semaforu a sdilene pameti z operacniho systemu

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