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
#include "library.h"

#define NAME_LENGTH 256
#define LOG_COUNT_LENGTH sizeof(int)
#define BOARDED_SEMAPHORE_NAME "/boarded"
#define UNBOARDED_SEMAPHORE_NAME "/unboarded"
#define LOG_SEMAPHORE_NAME "/log"
#define WAIT_UNBOARDED_SEMAPHORE_NAME "/wait_unboarded"



named_memory_t bus_stop_skiers;
//int* bus_stop_skiers;
named_memory_t ptr_log_count;
//int* ptr_log_count;

named_semaphore_t mutex;
sem_t* boarded_semaphore;
sem_t* unboarded_semaphore;
sem_t* log_semaphore;
sem_t* wait_unboarded_semaphore;

/// zformátuje řetězec na název zastávky
/// \param str řetězec
/// \param len délka řetězce
/// \param id id zastávky
void format_name(char* const str, const int len, const long id)
{
    snprintf(str, len, "/bus%ld", id);
}

int fn_bus(const long Z, const long K, const long TB, FILE *fp, sem_t* stop_semaphores[])
{
    int* log =  ptr_log_count.data;
    int* skiers = bus_stop_skiers.data;

    sem_wait(log_semaphore);
    fprintf(fp, "%d: bus started\n", ++*log);
    sem_post(log_semaphore);

    bool go_back;

    do
    {
        int free_seats = (int)K;

        go_back = false;

        for(int j = 0; j < Z; j++)
        {
            named_semaphore_wait(&mutex);

            sem_wait(log_semaphore);
            printf("%d: bus arrived to %d\n", ++*log, j + 1);
            sem_post(log_semaphore);

            const int n = (skiers[j] > free_seats) ? free_seats: skiers[j];

            for (int k = 0; k < n; k++)
            {
                sem_post(stop_semaphores[j]);
                sem_wait(boarded_semaphore);
            }

            free_seats -= n;

            skiers[j] -= n;

            if(skiers[j] > 0)
            {
                go_back = true;
            }

            sem_wait(log_semaphore);
            printf("%d: bus leaving %d\n", ++*log, j + 1);
            sem_post(log_semaphore);

            named_semaphore_post(&mutex);

            usleep(random() % TB); // uspani na ddbu cesty k dalsi zastavce
        }

        sem_wait(log_semaphore);
        printf("%d: bus arrived to final\n", ++*log);
        sem_post(log_semaphore);

        // TODO synchronizace vystupu na konecne
        int riders = (int)K - free_seats;

        for(int j = 0; j < riders; j++)
        {
            sem_post(unboarded_semaphore);
            sem_wait(wait_unboarded_semaphore);
        }

        sem_wait(log_semaphore);
        printf("%d: bus leaving final\n", ++*log);
        sem_post(log_semaphore);

    } while(go_back);

    sem_wait(log_semaphore);
    printf("%d: bus finish\n", ++*log);
    sem_post(log_semaphore);

    return 0;
}

int fn_rider(const int rider_id, const long Z, const long TL, sem_t* stop_semaphores[])
{
    int* log =  ptr_log_count.data;
    int* skiers = bus_stop_skiers.data;

    // Nahodne priradim lyzare k zastavce

    const long bus_stop_index = random() % Z;
    sem_t* stop_semaphore = stop_semaphores[bus_stop_index];

    // Vlastni algoritmus

    sem_wait(log_semaphore);
    printf("%d: L %d started\n", ++*log, rider_id);
    sem_post(log_semaphore);

    usleep(random() % TL); // nahodne trvani snidane

    named_semaphore_wait(&mutex);
    skiers[bus_stop_index]++;
    named_semaphore_post(&mutex);

    sem_wait(log_semaphore);
    printf("%d: L %d arrived to %ld\n", ++*log, rider_id, bus_stop_index + 1);
    sem_post(log_semaphore);

    sem_wait(stop_semaphore);

    sem_post(boarded_semaphore);

    sem_wait(log_semaphore);
    printf("%d: L %d boarded\n", ++*log, rider_id);
    sem_post(log_semaphore);

    sem_wait(unboarded_semaphore);

    sem_wait(log_semaphore);
    printf("%d: L %d unboarded\n", ++*log, rider_id);
    sem_post(log_semaphore);

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

    // Soubor pro logovani

    FILE *fp = fopen("proj2.out.txt", "a");
    if(fp == NULL)
    {
        perror("Error opening file");
        goto fail_fopen;
    }

    // Inicializace sdilene pameti

    if(named_memory_init(&bus_stop_skiers,"/waitings",  Z * sizeof(int)) == (void*)-1) {
        perror("Error creating bus_stop_skiers");
        goto fail_stop_skiers;
    }

    int* p1 = bus_stop_skiers.data;
    for(int i = 0; i < Z; i++)
    {
        p1[i] = 0;
    }

    if(named_memory_init(&log_semaphore,"/A",sizeof(int)) == (void*)-1) {
        perror("Error creating ptr_log_count");
        goto fail_log_count;
    }

    *(int*)ptr_log_count.data = 0;

    // Inicializace semaforu a jejich zavreni, protoze je jen vytvorime pro pouziti v jinych procesech

    if(named_semaphore_init(&mutex, "/mutex", 1) == SEM_FAILED)
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

        fn_bus(Z, K, TB, fp, stop_semaphores);

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
    named_semaphore_destroy(&mutex);

fail_mutex:

    munmap(ptr_log_count, LOG_COUNT_LENGTH);
    shm_unlink(LOG_COUNT_NAME);
fail_log_count:

    named_memory_destroy(&bus_stop_skiers);
fail_stop_skiers:

    fclose(fp);
fail_fopen:
    return 0;
}