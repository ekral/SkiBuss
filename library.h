//
// Created by Laura on 15/11/2024.
//

#ifndef UNTITLED2_LIBRARY_H
#define UNTITLED2_LIBRARY_H


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

sem_t* get_semaphore(const char* const name)
{
    sem_t* semaphore = sem_open(name, 0);

    return semaphore;
}

#endif //UNTITLED2_LIBRARY_H
