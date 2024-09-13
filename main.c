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

#define NAME_LEN 256

void createName(char* const str, const int len, const int id)
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

int fnBus(const int Z, const int K, const int TB)
{

    int* waitings = shared_get("/waitings", Z * sizeof(int), false);
    int* pA = shared_get("/A",1 * sizeof(int), false);

    sem_t* mutex = sem_open("/mutex", O_CREAT, 0755, 1);
    sem_t* boarded = sem_open("/boarded", O_CREAT, 0755, 0);
    sem_t* buses[Z];

    sem_wait(mutex);
    printf("%d: bus started\n", ++*pA);
    sem_post(mutex);

    for(int i = 0; i < Z; i++)
    {
        char name[NAME_LEN];
        createName(name, NAME_LEN, i);
        buses[i] = sem_open(name, O_CREAT, 0755, 0);
    }

    bool goBack;

    do {
        int freeSeats = K;

        goBack = false;

        for(int j = 0; j < Z; j++) {

            usleep(rand() % TB);

            sem_wait(mutex);
            printf("%d: bus arrived to %d\n", ++*pA, j + 1);
            const int n = (waitings[j] > freeSeats) ? freeSeats: waitings[j];
            for (int i = 0; i < n; i++) {
                sem_post(buses[j]);
                sem_wait(boarded);
            }

            freeSeats -= n;

            waitings[j] -= n;


            if(waitings[j] > 0) {
                goBack = true;
            }

            printf("%d: bus leaving  %d\n", ++*pA, j + 1);

            sem_post(mutex);

        }

        sem_wait(mutex);
        printf("%d: bus arrived to final\n", ++*pA);
        sem_post(mutex);

        // TODO synchronizace vystupu na konecne

        sem_wait(mutex);
        printf("%d: bus leaving final\n", ++*pA);
        sem_post(mutex);

    } while(goBack);

    sem_wait(mutex);
    printf("%d: bus finish\n", ++*pA);
    sem_post(mutex);

    sem_close(mutex);
    sem_close(boarded);
    for(int i = 0; i < Z; i++) {
        sem_close(buses[i]);
    }

    return 0;
}

int fnRiders(const int riderId, const int Z, const int TL) {
    time_t t;
    srand((unsigned)time(&t) ^ getpid());
    const int busStopIndex = rand() % Z;

    int* waitings = shared_get("/waitings",Z * sizeof(int), false);
    int* pA = shared_get("/A",1 * sizeof(int), false);

    sem_t* mutex = sem_open("/mutex", O_CREAT, 0755, 1);
    sem_t* boarded = sem_open("/boarded", O_CREAT, 0755, 0);

    char name[NAME_LEN];
    createName(name, NAME_LEN, busStopIndex);
    sem_t* bus = sem_open(name, O_CREAT, 0755, 0);

    sem_wait(mutex);
    printf("%d: L %d started\n", ++*pA, riderId);
    sem_post(mutex);

    usleep(rand() % TL);

    sem_wait(mutex);
    waitings[busStopIndex]++;
    printf("%d: L %d arrived to %d\n", ++*pA, riderId, busStopIndex + 1);
    sem_post(mutex);

    sem_wait(bus);
    printf("%d: L %d boarded\n", ++*pA, riderId);
    sem_post(boarded);

    sem_close(mutex);
    sem_close(boarded);
    sem_close(bus);

    return 0;
}

int main(int argc, char *argv[])
{
    srand(time(NULL));

    if(argc != 6)
    {
        return -1;
    }

    const int L = atoi(argv[1]);
    const int Z = atoi(argv[2]);
    const int K = atoi(argv[3]);
    const int TL = atoi(argv[4]);
    const int TB = atoi(argv[5]);

    int* waitings = shared_get("/waitings",Z * sizeof(int), true);

    for(int i = 0; i < Z; i++)
    {
        waitings[i] = 0;
    }

    int* pA = shared_get("/A",1 * sizeof(int), true);

    *pA = 0;


    for(int i = 0; i < L; i++) {

        pid_t p = fork();

        if(p == 0)
        {
            fnRiders(i + 1, Z, TL);

            exit(0);
        }
    }

    pid_t p = fork();

    if(p == 0)
    {
        fnBus(Z, K, TB);

        exit(0);
    }

    pid_t wpid;
    int status;

    while((wpid = wait(&status)) > 0){};

    sem_unlink("/A");
    sem_unlink("/mutex");
    sem_unlink("/boarded");
    shm_unlink("/waitings");

    char name[NAME_LEN];

    for(int i = 0; i < Z; i++)
    {
        createName(name, NAME_LEN, i);
        sem_unlink(name);
    }
}