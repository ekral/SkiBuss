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
#include <errno.h>
#include <string.h>
#include <stdarg.h>

#define NAME_LENGTH 256
#define STOP_SKIERS_NAME "/waitings"
#define STOP_SKIERS_LENGTH(n) n * sizeof(int)
#define LOG_COUNT_NAME "/A"
#define LOG_COUNT_LENGTH sizeof(int)
#define MUTEX_NAME "/mutex"
#define BOARDED_SEMAPHORE_NAME "/boarded"
#define UNBOARDED_SEMAPHORE_NAME "/unboarded"
#define LOG_SEMAPHORE_NAME "/log"
#define WAIT_UNBOARDED_SEMAPHORE_NAME "/wait_unboarded"

int* bus_stop_skiers;
int* ptr_log_count;

sem_t* mutex;
sem_t* boarded_semaphore;
sem_t* unboarded_semaphore;
sem_t* log_semaphore;
sem_t* wait_unboarded_semaphore;

void log_message(const char* format, ...)
{
    sem_wait(log_semaphore);

    FILE* fp = fopen("proj2.out.txt", "a");
    if(fp == NULL)
    {
        perror("Error opening file");
        return;
    }

    fprintf(fp,"%d:", ++*ptr_log_count);

    va_list va;
    va_start(va, format);
    vfprintf(fp, format, va);
    va_end(va);

    fprintf(fp, "\n");

    fclose(fp);

    sem_post(log_semaphore);
}

/// zformátuje řetězec na název zastávky
/// \param str řetězec
/// \param len délka řetězce
/// \param id id zastávky
void format_name(char* const str, const int len, const long id)
{
    snprintf(str, len, "/bus%ld", id);
}

void* create_shared(const char* name, const int length)
{
    int* p = (void*)-1;

    int fd = shm_open(name, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);

    if(fd != -1)
    {
        if(ftruncate(fd, length) != -1)
        {
            p = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        }
        close(fd); // po namapovani muzu fd zavrit
    }

    return p;
}

sem_t* create_semaphore(const char* const name, const int init)
{
    sem_t* semaphore = sem_open(name, O_CREAT, 0755, init);

    return semaphore;
}

int fn_bus(const long Z, const long K, const long TB, sem_t* stop_semaphores[])
{
    log_message("bus started");

    bool go_back;

    do
    {
        int free_seats = (int)K;

        go_back = false;

        for(int j = 0; j < Z; j++)
        {
            sem_wait(mutex);

            log_message("bus arrived to %d", j + 1);

            const int n = (bus_stop_skiers[j] > free_seats) ? free_seats: bus_stop_skiers[j];

            for (int k = 0; k < n; k++)
            {
                sem_post(stop_semaphores[j]);
                sem_wait(boarded_semaphore);
            }

            free_seats -= n;

            bus_stop_skiers[j] -= n;

            if(bus_stop_skiers[j] > 0)
            {
                go_back = true;
            }

            log_message("bus leaving %d", j + 1);

            sem_post(mutex);

            usleep(random() % TB); // uspani na ddbu cesty k dalsi zastavce
        }

        log_message("bus arrived to final");

        // TODO synchronizace vystupu na konecne
        int riders = (int)K - free_seats;

        for(int j = 0; j < riders; j++)
        {
            sem_post(unboarded_semaphore);
            sem_wait(wait_unboarded_semaphore);
        }

        log_message("bus leaving final");

    } while(go_back);

    log_message("bus finish");

    return 0;
}

int fn_rider(const int rider_id, const long Z, const long TL, sem_t* stop_semaphores[])
{
    // Nahodne priradim lyzare k zastavce

    const long bus_stop_index = random() % Z;
    sem_t* stop_semaphore = stop_semaphores[bus_stop_index];

    // Vlastni algoritmus

    log_message("L %d started", rider_id);

    usleep(random() % TL); // nahodne trvani snidane

    sem_wait(mutex);
    bus_stop_skiers[bus_stop_index]++;
    sem_post(mutex);

    log_message("L %d arrived to %ld", rider_id, bus_stop_index + 1);

    sem_wait(stop_semaphore);

    sem_post(boarded_semaphore);

    log_message("L %d boarded", rider_id);

    sem_wait(unboarded_semaphore);

    log_message("unboarded", rider_id);

    sem_post(wait_unboarded_semaphore);

    return 0;
}

int main(const int argc, char *argv[])
{
    // TODO osetreni erroru

    // Zpracovani vstupnich argumentu

    if(argc != 6)
    {
        return 1;
    }

    char* str_end;

    const long L = strtol(argv[1], &str_end, 10);

    if(str_end == argv[1])
    {
        puts("Neplatny format L.");

        return -1;
    }
    else if(errno == ERANGE)
    {
        perror("Neplatný rozsah L.");

        return -1;
    }

    const long Z = strtol(argv[2], &str_end, 10);

    if(str_end == argv[2])
    {
        puts("Neplatny format Z.");

        return 1;
    }
    else if(errno == ERANGE)
    {
        perror("Neplatný rozsah Z.");

        return -1;
    }

    const long K = strtol(argv[3], &str_end, 10);

    if(str_end == argv[3])
    {
        puts("Neplatny format K.");

        return 1;
    }
    else if(errno == ERANGE)
    {
        perror("Neplatný rozsah K.");

        return -1;
    }

    const long TL = strtol(argv[4], &str_end, 10);

    if(str_end == argv[4])
    {
        puts("Neplatny format TL.");

        return 1;
    }
    else if(errno == ERANGE)
    {
        perror("Neplatný rozsah TL.");

        return -1;
    }

    const long TB = strtol(argv[5], &str_end, 10);

    if(str_end == argv[5])
    {
        puts("Neplatny format TB.");

        return 1;
    }
    else if(errno == ERANGE)
    {
        perror("Neplatný rozsah TB.");

        return -1;
    }

    if((L < 0) || (L >= 20000) || (Z > 10) || (Z <= 0) || (K < 10) || (K > 100) || (TL < 0) || (TL > 10000) || (TB < 0) || (TB > 1000)) {
        fprintf(stderr, "Špatně zadaný argument.\n");
        return 1;
    }

    sem_t* stop_semaphores[Z];

    // Inicializace sdilene pameti

    bus_stop_skiers = create_shared(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z));
    if(bus_stop_skiers == (int*)-1) {
        perror("Error creating bus_stop_skiers");
        goto fail_stop_skiers;
    }

    for(int i = 0; i < Z; i++)
    {
        bus_stop_skiers[i] = 0;
    }

    ptr_log_count = create_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);
    if(ptr_log_count == (int*)-1) {
        perror("Error creating ptr_log_count");
        goto fail_log_count;
    }

    *ptr_log_count = 0;

    // Inicializace semaforu a jejich zavreni, protoze je jen vytvorime pro pouziti v jinych procesech

    mutex = create_semaphore(MUTEX_NAME, 1);
    if(mutex == SEM_FAILED)
    {
        perror("Error creating mutex");
        goto fail_mutex;
    }
    //sem_close(mutex);

    boarded_semaphore = create_semaphore(BOARDED_SEMAPHORE_NAME, 0);
    if(boarded_semaphore == SEM_FAILED)
    {
        perror("Error creating boarded_smaphore");
        goto fail_boarded_smaphore;
    }
    //sem_close(boarded_semaphore);

    unboarded_semaphore = create_semaphore(UNBOARDED_SEMAPHORE_NAME, 0);
    if(unboarded_semaphore == SEM_FAILED)
    {
        perror("Error creating uboarded_smaphore");
        goto fail_uboarded_smaphore;
    }
    //sem_close(unboarded_semaphore);

    log_semaphore = create_semaphore(LOG_SEMAPHORE_NAME, 1);
    if(log_semaphore == SEM_FAILED)
    {
        perror("Error creating log_smaphore");
        goto fail_log_smaphore;
    }
    //sem_close(log_semaphore);

    wait_unboarded_semaphore = create_semaphore(WAIT_UNBOARDED_SEMAPHORE_NAME, 0);
    if(wait_unboarded_semaphore == SEM_FAILED)
    {
        perror("Error creating wait_unboarded_smaphore");
        goto fail_wait_unboarded_smaphore;
    }
    //sem_close(wait_unboarded_semaphore);

    int i;
    for(i = 0; i < Z; i++)
    {
        char name[NAME_LENGTH];
        format_name(name, NAME_LENGTH, i);

        sem_t* stop_semaphore = create_semaphore(name, 0);
        if(stop_semaphore == SEM_FAILED)
        {
            perror("Error creating stop_semaphore");
            goto fail_stop_semaphore;
        }

        stop_semaphores[i] = stop_semaphore;
    }

    // Spusteni procesu vcetne inicializace nahodnych cisel
    for(int k = 0; k < L; k++)
    {
        const pid_t p = fork();

        if(p == -1)
        {
            perror("Fork error");
            goto fail_stop_semaphore;
        }
        else if(p == 0)
        {
            time_t t;
            srandom((unsigned)time(&t) ^ getpid());

            fn_rider(k + 1, Z, TL, stop_semaphores);

            exit(0);
        }
    }

    const pid_t p = fork();

    if(p == -1)
    {
        perror("Fork error");
        goto fail_stop_semaphore;
    }
    else if(p == 0)
    {
        time_t t;
        srandom((unsigned)time(&t) ^ getpid());

        fn_bus(Z, K, TB, stop_semaphores);

        exit(0);
    }

    // Cekani na dokonceni procesu

    int status;

    while((wait(&status)) > 0){}

    // Uvolneni semaforu a sdilene pameti z operacniho systemu

fail_stop_semaphore:
    for(int j = 0; j < i; j++)
    {
        char name[NAME_LENGTH];
        format_name(name, NAME_LENGTH, j);

        sem_close(stop_semaphores[j]);
        sem_unlink(name);
    }

    sem_close(wait_unboarded_semaphore);
    sem_unlink(WAIT_UNBOARDED_SEMAPHORE_NAME);
fail_wait_unboarded_smaphore:

    sem_close(log_semaphore);
    sem_unlink(LOG_SEMAPHORE_NAME);
fail_log_smaphore:

    sem_close(unboarded_semaphore);
    sem_unlink(UNBOARDED_SEMAPHORE_NAME);
fail_uboarded_smaphore:

    sem_close(boarded_semaphore);
    sem_unlink(BOARDED_SEMAPHORE_NAME);
fail_boarded_smaphore:

    sem_close(mutex);
    sem_unlink(MUTEX_NAME);
fail_mutex:

    munmap(ptr_log_count, LOG_COUNT_LENGTH);
    shm_unlink(LOG_COUNT_NAME);
fail_log_count:

    munmap(bus_stop_skiers, STOP_SKIERS_LENGTH(Z));
    shm_unlink(STOP_SKIERS_NAME);
fail_stop_skiers:

    return 0;
}