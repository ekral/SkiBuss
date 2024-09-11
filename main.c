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

#define COUNT 5
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

// diky atributu nemame error nepouzite id
int fnBus(const int __attribute__((__unused__)) id) {

    int* waitings = shared_get("/waitings", COUNT * sizeof(int), false);
    //int* pA = shared_get("/A",1 * sizeof(int), false);

    sem_t* mutex = sem_open("/mutex", O_CREAT, 0755, 1);
    sem_t* boarded = sem_open("/boarded", O_CREAT, 0755, 0);
    sem_t* buses[COUNT];

    for(int i = 0; i < COUNT; i++)
    {
        char name[NAME_LEN];
        createName(name, NAME_LEN, i);
        buses[i] = sem_open(name, O_CREAT, 0755, 0);
    }

    bool goBack;

    do {
        int freeSeats = 50;

        goBack = false;
        puts("Nova cesta______________________________");
        for(int j = 0; j < COUNT; j++) {
            printf("BUS arrived to %d\n", j + 1);

            sem_wait(mutex);
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

            printf( "nastoupilo: %d\n", n);
            printf("zustava na zastavce: %d\n", waitings[j]);
            printf("volnych mist v autobuse: %d\n", freeSeats);

            printf("depart\n");

            sem_post(mutex);

        }

        printf("goBack: %d\n", goBack  );

    } while(goBack);

    sem_close(mutex);
    sem_close(boarded);
    for(int i = 0; i < COUNT; i++) {
        sem_close(buses[i]);
    }

    return 0;
}

int fnRiders(const int id) {
    time_t t;
    srand((unsigned)time(&t) ^ getpid());
    const int busStopIndex = rand() % COUNT;

    int* waitings = shared_get("/waitings",COUNT * sizeof(int), false);
    int* pA = shared_get("/A",1 * sizeof(int), false);

    sem_t* mutex = sem_open("/mutex", O_CREAT, 0755, 1);
    sem_t* boarded = sem_open("/boarded", O_CREAT, 0755, 0);
    char name[NAME_LEN];
    createName(name, NAME_LEN, busStopIndex);
    sem_t* bus = sem_open(name, O_CREAT, 0755, 0);

    sem_wait(mutex);
    ++*pA;
    printf("%d: L %d started\n", *pA, id);
    sem_post(mutex);

    usleep(1000 * 1);

    sem_wait(mutex);
    waitings[busStopIndex]++;
    sem_post(mutex);

    sem_wait(bus);
    printf("board\n");
    sem_post(boarded);

    sem_close(mutex);
    sem_close(boarded);
    sem_close(bus);

    return 0;
}

void CreateProcess(int (*func)(int), int arg) {
    pid_t p = fork();

    if(p < 0) {
        perror("fork fails");
    }
    else if(p == 0) {
        func(arg);

        exit(0);
    }
    else {

    }
}

int main() {

    int* waitings = shared_get("/waitings",COUNT * sizeof(int), true);
    int* pA = shared_get("/A",1 * sizeof(int), true);

    for(int i = 0; i < COUNT; i++)
    {
        waitings[i] = 0;
    }

    *pA = 0;

    srand(time(NULL));

    for(int i = 0; i < 100; i++) {
        CreateProcess(fnRiders, i + 1);
    }

    CreateProcess(fnBus, 0);

    pid_t wpid;
    int status;

    while((wpid = wait(&status)) > 0){};

    sem_unlink("/A");
    sem_unlink("/mutex");
    sem_unlink("/boarded");
    shm_unlink("/waitings");

    char name[NAME_LEN];

    for(int i = 0; i < COUNT; i++)
    {
        createName(name, NAME_LEN, i);
        sem_unlink(name);
    }
}