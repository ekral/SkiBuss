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

///
/// \param name jméno sdílené paměti
/// \param length velikost sdílené paměti
/// \return vrací ukazatel na sdílenou paměť, v případě chyby -1
void* get_shared(const char* name, const int length)
{
    int* p = (void*)-1;
    const int fd = shm_open(name, O_RDWR, S_IRWXU | S_IRWXG);

    if(fd != -1)
    {
        p = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
    }

    return p;
}

sem_t* create_semaphore(const char* const name, const int init)
{
    sem_t* semaphore = sem_open(name, O_CREAT, 0755, init);

    return semaphore;
}

///
/// \param name název semaforu
/// \return vrátí ukazatel na semafor, v případě chyby SEM_FAILED
sem_t* get_semaphore(const char* const name)
{
    sem_t* semaphore = sem_open(name, 0);

    return semaphore;
}

int fn_bus(const long Z, const long K, const long TB)
{
    // Ziskani sdilene pameti

    sem_t* stop_semaphores[Z];

    int* stop_skiers = get_shared(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z));
    if(stop_skiers != (void*)-1)
    {
        perror("Error creating stop_skiers");
        goto fail_stop_skiers;
    }

    int* ptr_log_count = get_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);
    if(ptr_log_count != (void*)-1)
    {
        perror("Error creating ptr_log_count");
        goto fail_ptr_log_count;
    }

    // Ziskani semaforu

    sem_t* mutex = get_semaphore(MUTEX_NAME);
    if(mutex == SEM_FAILED)
    {
        perror("Error creating mutex");
        goto fail_mutex;
    }

    sem_t* boarded_semaphore = get_semaphore(BOARDED_SEMAPHORE_NAME);
    if(boarded_semaphore == SEM_FAILED)
    {
        perror("Error creating boarded_semaphore");
        goto fail_boarded_semaphore;
    }

    sem_t* unboarded_semaphore = get_semaphore(UNBOARDED_SEMAPHORE_NAME);
    if(unboarded_semaphore == SEM_FAILED)
    {
        perror("Error creating unboarded_semaphore");
        goto fail_unboarded_semaphore;
    }

    sem_t* log_semaphore = get_semaphore(LOG_SEMAPHORE_NAME);
    if(log_semaphore == SEM_FAILED)
    {
        perror("Error creating log_semaphore");
        goto fail_log_semaphore;
    }

    sem_t* wait_unboarded_semaphore = get_semaphore(WAIT_UNBOARDED_SEMAPHORE_NAME);
    if(wait_unboarded_semaphore == SEM_FAILED)
    {
        perror("Error creating wait_unboarded_semaphore");
        goto fail_wait_unboarded_semaphore;
    }

    int i;
    for(i = 0; i < Z; i++)
    {
        char name[NAME_LENGTH];
        format_name(name, NAME_LENGTH, i);

        stop_semaphores[i] = get_semaphore(name);
        if(stop_semaphores[i] == SEM_FAILED)
        {
            perror("Error creating stop_semaphores");
            goto fail_stop_semaphores;
        }
    }

    // Vlastni algoritmus

    sem_wait(log_semaphore);
    printf("%d: bus started\n", ++*ptr_log_count);
    sem_post(log_semaphore);

    bool go_back;

    do
    {
        int free_seats = K;

        go_back = false;

        for(int j = 0; j < Z; j++)
        {
            sem_wait(mutex);

            sem_wait(log_semaphore);
            printf("%d: bus arrived to %d\n", ++*ptr_log_count, j + 1);
            sem_post(log_semaphore);

            const int n = (stop_skiers[j] > free_seats) ? free_seats: stop_skiers[j];

            for (int i = 0; i < n; i++)
            {
                sem_post(stop_semaphores[j]);
                sem_wait(boarded_semaphore);
            }

            free_seats -= n;

            stop_skiers[j] -= n;

            if(stop_skiers[j] > 0)
            {
                go_back = true;
            }

            sem_wait(log_semaphore);
            printf("%d: bus leaving %d\n", ++*ptr_log_count, j + 1);
            sem_post(log_semaphore);

            sem_post(mutex);

            usleep(random() % TB); // uspani na ddbu cesty k dalsi zastavce
        }

        sem_wait(log_semaphore);
        printf("%d: bus arrived to final\n", ++*ptr_log_count);
        sem_post(log_semaphore);

        // TODO synchronizace vystupu na konecne
        int riders = K - free_seats;

        for(int j = 0; j < riders; j++)
        {
            sem_post(unboarded_semaphore);
            sem_wait(wait_unboarded_semaphore);
        }

        sem_wait(log_semaphore);
        printf("%d: bus leaving final\n", ++*ptr_log_count);
        sem_post(log_semaphore);

    } while(go_back);

    sem_wait(log_semaphore);
    printf("%d: bus finish\n", ++*ptr_log_count);
    sem_post(log_semaphore);


    // Zavreni semaforu a odmapovani sdilene pameti pro tento proces
    // zavreli by se automaticky po dokonceni procesu, ale je lepsi je uzavrit explicitne

fail_stop_semaphores:
    for(int j = 0; j < i; j++)
    {
        sem_close(stop_semaphores[i]);
    }

    sem_close(wait_unboarded_semaphore);
fail_wait_unboarded_semaphore:

    sem_close(log_semaphore);
fail_log_semaphore:

    sem_close(unboarded_semaphore);
fail_unboarded_semaphore:

    sem_close(boarded_semaphore);
fail_boarded_semaphore:

    sem_close(mutex);
fail_mutex:

    munmap(LOG_COUNT_NAME, LOG_COUNT_LENGTH);
fail_ptr_log_count:

    munmap(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z));
fail_stop_skiers:

    return 0;
}

int fn_rider(const int rider_id, const long Z, const long TL)
{
    // Nahodne priradim lyzare k zastavce

    const long bus_stop_index = random() % Z;

    // Ziskani sdilene pameti

    int* stop_skiers = get_shared(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z));
    int* ptr_log_count = get_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);

    // Ziskani semaforu

    sem_t* mutex = get_semaphore(MUTEX_NAME);
    sem_t* boarded_semaphore = get_semaphore(BOARDED_SEMAPHORE_NAME);
    sem_t* unboarded_semaphore = get_semaphore(UNBOARDED_SEMAPHORE_NAME);
    sem_t* log_semaphore = get_semaphore(LOG_SEMAPHORE_NAME);
    sem_t* wait_unboarded_semaphore = get_semaphore(WAIT_UNBOARDED_SEMAPHORE_NAME);

    char name[NAME_LENGTH];
    format_name(name, NAME_LENGTH, bus_stop_index);

    sem_t* stop_semaphore = get_semaphore(name);

    // Vlastni algoritmus

    sem_wait(log_semaphore);
    printf("%d: L %d started\n", ++*ptr_log_count, rider_id);
    sem_post(log_semaphore);

    usleep(random() % TL); // nahodne trvani snidane

    sem_wait(mutex);
    stop_skiers[bus_stop_index]++;
    sem_post(mutex);

    sem_wait(log_semaphore);
    printf("%d: L %d arrived to %ld\n", ++*ptr_log_count, rider_id, bus_stop_index + 1);
    sem_post(log_semaphore);

    sem_wait(stop_semaphore);

    sem_post(boarded_semaphore);

    sem_wait(log_semaphore);
    printf("%d: L %d boarded\n", ++*ptr_log_count, rider_id);
    sem_post(log_semaphore);

    sem_wait(unboarded_semaphore);

    sem_wait(log_semaphore);
    printf("%d: L %d unboarded\n", ++*ptr_log_count, rider_id);
    sem_post(log_semaphore);

    sem_post(wait_unboarded_semaphore);

    // Zavreni semaforu a odmapovani sdilene pameti pro tento proces
    // zavreli by se automaticky po dokonceni procesu, ale je lepsi je uzavrit explicitne

    sem_close(mutex);
    sem_close(boarded_semaphore);
    sem_close(unboarded_semaphore);
    sem_close(stop_semaphore);
    sem_close(log_semaphore);
    sem_close(wait_unboarded_semaphore);

    munmap(LOG_COUNT_NAME, LOG_COUNT_LENGTH);
    munmap(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z));

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

    // Inicializace sdilene pameti

    int* bus_stop_skiers = create_shared(/*STOP_SKIERS_NAME*/NULL, STOP_SKIERS_LENGTH(Z));
    if(bus_stop_skiers == (int*)-1) {
        perror("Error creating bus_stop_skiers");
        goto fail_stop_skiers;
    }

    for(int i = 0; i < Z; i++)
    {
        bus_stop_skiers[i] = 0;
    }

    int* ptr_log_count = create_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);
    if(ptr_log_count == (int*)-1) {
        perror("Error creating ptr_log_count");
        goto fail_log_count;
    }

    *ptr_log_count = 0;

    // Inicializace semaforu a jejich zavreni, protoze je jen vytvorime pro pouziti v jinych procesech

    sem_t* mutex = create_semaphore(MUTEX_NAME, 1);
    if(mutex == SEM_FAILED)
    {
        perror("Error creating mutex");
        goto fail_mutex;
    }
    sem_close(mutex);

    sem_t* boarded_smaphore = create_semaphore(BOARDED_SEMAPHORE_NAME, 0);
    if(boarded_smaphore == SEM_FAILED)
    {
        perror("Error creating boarded_smaphore");
        goto fail_boarded_smaphore;
    }
    sem_close(boarded_smaphore);

    sem_t* uboarded_smaphore = create_semaphore(UNBOARDED_SEMAPHORE_NAME, 0);
    if(uboarded_smaphore == SEM_FAILED)
    {
        perror("Error creating uboarded_smaphore");
        goto fail_uboarded_smaphore;
    }
    sem_close(uboarded_smaphore);

    sem_t* log_smaphore = create_semaphore(LOG_SEMAPHORE_NAME, 1);
    if(log_smaphore == SEM_FAILED)
    {
        perror("Error creating log_smaphore");
        goto fail_log_smaphore;
    }
    sem_close(log_smaphore);

    sem_t* wait_unboarded_smaphore = create_semaphore(WAIT_UNBOARDED_SEMAPHORE_NAME, 0);
    if(wait_unboarded_smaphore == SEM_FAILED)
    {
        perror("Error creating wait_unboarded_smaphore");
        goto fail_wait_unboarded_smaphore;
    }
    sem_close(wait_unboarded_smaphore);

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
        sem_close(stop_semaphore);
    }

    // Spusteni procesu vcetne inicializace nahodnych cisel

    for(int k = 0; k < L; k++)
    {
        const pid_t p = fork();

        if(p == -1)
        {
            perror("Error creating stop_semaphore");
            goto fail_stop_semaphore;
        }
        else if(p == 0)
        {
            time_t t;
            srandom((unsigned)time(&t) ^ getpid());

            fn_rider(k + 1, Z, TL);

            exit(0);
        }
    }

    const pid_t p = fork();

    if(p == -1)
    {
        goto fail_stop_semaphore;
    }
    else if(p == 0)
    {
        time_t t;
        srandom((unsigned)time(&t) ^ getpid());

        fn_bus(Z, K, TB);

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

        sem_unlink(name);
    }

    sem_unlink(WAIT_UNBOARDED_SEMAPHORE_NAME);
fail_wait_unboarded_smaphore:

    sem_unlink(LOG_SEMAPHORE_NAME);
fail_log_smaphore:

    sem_unlink(UNBOARDED_SEMAPHORE_NAME);
fail_uboarded_smaphore:

    sem_unlink(BOARDED_SEMAPHORE_NAME);
fail_boarded_smaphore:

    sem_unlink(MUTEX_NAME);
fail_mutex:

    shm_unlink(LOG_COUNT_NAME);
fail_log_count:

    shm_unlink(STOP_SKIERS_NAME);
fail_stop_skiers:
    return 0;
}