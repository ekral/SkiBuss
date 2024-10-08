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

void format_name(char* const str, const int len, const long id)
{
    snprintf(str, len, "/bus%ld", id);
}

void* create_shared(const char* name, const int length)
{
    int* p = (void*)-1;

    int fd;

    if((fd = shm_open(name, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG)) != -1)
    {
        if(ftruncate(fd, length) != -1)
        {
            p = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        }

        close(fd); // po namapovani muzu fd zavrit
    }

    return p;
}

void* get_shared(const char* name, const int length)
{
    const int fd = shm_open(name, O_RDWR, S_IRWXU | S_IRWXG);

    int* p = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd); // po namapovani muzu fd zavrit

    return p;
}

sem_t* create_semaphore(const char* const name, const int init)
{
    sem_t* semaphore = sem_open(name, O_CREAT, 0755, init);

    return semaphore;
}

sem_t* get_semaphore(const char* const name)
{
    sem_t* semaphore = sem_open(name, 0);

    return semaphore;
}

int fn_bus(const int Z, const int K, const int TB)
{
    // Ziskani sdilene pameti

    int* stop_skiers = get_shared(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z));
    int* ptr_log_count = get_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);

    // Ziskani semaforu

    sem_t* mutex = get_semaphore(MUTEX_NAME);
    sem_t* boarded_semaphore = get_semaphore(BOARDED_SEMAPHORE_NAME);
    sem_t* unboarded_semaphore = get_semaphore(UNBOARDED_SEMAPHORE_NAME);

    sem_t* stop_semaphores[Z];

    for(int i = 0; i < Z; i++)
    {
        char name[NAME_LENGTH];
        format_name(name, NAME_LENGTH, i);

        stop_semaphores[i] = get_semaphore(name);
    }

    // Vlastni algoritmus

    sem_wait(mutex);
    printf("%d: bus started\n", ++*ptr_log_count);
    sem_post(mutex);

    bool go_back;

    do
    {
        int free_seats = K;

        go_back = false;

        for(int j = 0; j < Z; j++)
        {
            sem_wait(mutex);

            printf("%d: bus arrived to %d\n", ++*ptr_log_count, j + 1);

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

            printf("%d: bus leaving %d\n", ++*ptr_log_count, j + 1);

            sem_post(mutex);

            usleep(random() % TB); // uspani na ddbu cesty k dalsi zastavce
        }

        sem_wait(mutex);
        printf("%d: bus arrived to final\n", ++*ptr_log_count);
        sem_post(mutex);

        // TODO synchronizace vystupu na konecne
        int riders = K - free_seats;

        for(int j = 0; j < riders; j++)
        {
            sem_post(unboarded_semaphore);
        }

        sem_wait(mutex);
        printf("%d: bus leaving final\n", ++*ptr_log_count);
        sem_post(mutex);

    } while(go_back);

    sem_wait(mutex);
    printf("%d: bus finish\n", ++*ptr_log_count);
    sem_post(mutex);


    // Zavreni semaforu a odmapovani sdilene pameti pro tento proces
    // zavreli by se automaticky po dokonceni procesu, ale je lepsi je uzavrit explicitne

    sem_close(mutex);
    sem_close(boarded_semaphore);
    sem_close(unboarded_semaphore);

    for(int i = 0; i < Z; i++)
    {
        sem_close(stop_semaphores[i]);
    }

    munmap(LOG_COUNT_NAME, LOG_COUNT_LENGTH);
    munmap(STOP_SKIERS_NAME, STOP_SKIERS_LENGTH(Z));

    return 0;
}

int fn_rider(const int rider_id, const int Z, const int TL)
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

    char name[NAME_LENGTH];
    format_name(name, NAME_LENGTH, bus_stop_index);

    sem_t* stop_semaphore = get_semaphore(name);

    // Vlastni algoritmus

    sem_wait(mutex);
    printf("%d: L %d started\n", ++*ptr_log_count, rider_id);
    sem_post(mutex);

    usleep(random() % TL); // nahodne trvani snidane

    sem_wait(mutex);
    stop_skiers[bus_stop_index]++;
    printf("%d: L %d arrived to %ld\n", ++*ptr_log_count, rider_id, bus_stop_index + 1);
    sem_post(mutex);

    sem_wait(stop_semaphore);

    sem_post(boarded_semaphore);

    sem_wait(mutex);
    printf("%d: L %d boarded\n", ++*ptr_log_count, rider_id);
    sem_post(mutex);

    sem_wait(unboarded_semaphore);

    sem_wait(mutex);
    printf("%d: L %d unboarded\n", ++*ptr_log_count, rider_id);
    sem_post(mutex);

    // Zavreni semaforu a odmapovani sdilene pameti pro tento proces
    // zavreli by se automaticky po dokonceni procesu, ale je lepsi je uzavrit explicitne

    sem_close(mutex);
    sem_close(boarded_semaphore);
    sem_close(unboarded_semaphore);
    sem_close(stop_semaphore);

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
        return -1;
    }

    char* str_end;

    errno = 0;

    const long L = strtol(argv[1], &str_end, 10);

    if(str_end == argv[1])
    {
        puts("Neplatny format L.");

        if (errno == ERANGE)
        {
            puts("Range error occurred.");
        }

        return -1;
    }

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

    // Inicializace semaforu a jejich zavreni, protoze je jen vytvorime pro pouziti v jinych procesech

    sem_t* mutex = create_semaphore(MUTEX_NAME, 1);
    sem_close(mutex);

    sem_t* boarded_smaphore = create_semaphore(BOARDED_SEMAPHORE_NAME, 0);
    sem_close(boarded_smaphore);

    sem_t* uboarded_smaphore = create_semaphore(UNBOARDED_SEMAPHORE_NAME, 0);
    sem_close(uboarded_smaphore);

    for(int i = 0; i < Z; i++)
    {
        char name[NAME_LENGTH];
        format_name(name, NAME_LENGTH, i);

        sem_t* stop_semaphore = create_semaphore(name, 0);
        sem_close(stop_semaphore);
    }

    // Spusteni procesu vcetne inicializace nahodnych cisel

    for(int i = 0; i < L; i++)
    {
        const pid_t p = fork();

        if(p == 0)
        {
            time_t t;
            srandom((unsigned)time(&t) ^ getpid());

            fn_rider(i + 1, Z, TL);

            exit(0);
        }
    }

    const pid_t p = fork();

    if(p == 0)
    {
        time_t t;
        srandom((unsigned)time(&t) ^ getpid());

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
    sem_unlink(UNBOARDED_SEMAPHORE_NAME);

    for(int i = 0; i < Z; i++)
    {
        char name[NAME_LENGTH];
        format_name(name, NAME_LENGTH, i);

        sem_unlink(name);
    }

    return 0;
}