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

int fnBus() {
    const int fd = shm_open("/waitings", O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    int* waitings = mmap(NULL, COUNT * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    sem_t* mutex = sem_open("/mutex", O_CREAT, 0755, 1);
    sem_t* boarded = sem_open("/boarded", O_CREAT, 0755, 0);
    sem_t* buses[COUNT];

    char name[256];
    for(int i = 0; i < COUNT; i++) {
        snprintf(name, 256, "/bus%d", i);
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

            sem_post(mutex);

            printf("depart\n");
        }

        printf("goBack: %d\n", goBack  );

    } while(goBack);

    sem_close(mutex);
    sem_close(boarded);
    for(int i = 0; i < COUNT; i++) {
        sem_close(buses[i]);
    }

    close(fd);
    return 0;
}

int fnRiders() {
    time_t t;
    srand((unsigned)time(&t) ^ getpid());
    const int fd = shm_open("/waitings", O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    int* waitings = mmap(NULL, COUNT * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    sem_t* mutex = sem_open("/mutex", O_CREAT, 0755, 1);
    sem_t* boarded = sem_open("/boarded", O_CREAT, 0755, 0);

    int index = rand() % COUNT;

    char name[256];
    snprintf(name, 256, "/bus%d", index);
    //puts(name);
    sem_t* bus = sem_open(name, O_CREAT, 0755, 0);
    sem_wait(mutex);
    waitings[index]++;
    sem_post(mutex);

    sem_wait(bus);
    printf("board\n");
    sem_post(boarded);

    sem_close(mutex);
    sem_close(boarded);
    sem_close(bus);

    close(fd);
    return 0;
}

void CreateProcess(int (*func)(void)) {
    pid_t p = fork();

    if(p < 0) {
        perror("fork fails");
    }
    else if(p == 0) {
        func();

        exit(0);
    }
    else {

    }
}

int main() {

    sem_unlink("/mutex");
    sem_unlink("/boarded");
    shm_unlink("/waitings");

    const int fd = shm_open("/waitings", O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    ftruncate(fd, COUNT * sizeof(int));
    int* waitings = mmap(NULL, COUNT * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    for(int i = 0; i < COUNT; i++) {
        waitings[i] = 0;
    }
    close(fd);
    srand(time(NULL));

    for(int i = 0; i < 100; i++) {
        CreateProcess(fnRiders);
    }

    CreateProcess(fnBus);

    pid_t wpid;
    int status;

    while((wpid = wait(&status)) > 0){};

    sem_unlink("/mutex");
    sem_unlink("/boarded");
    shm_unlink("/waitings");

    char name[256];
    for(int i = 0; i < COUNT; i++) {
        snprintf(name, 256, "/bus%d", i);
        sem_unlink(name);
    }
}