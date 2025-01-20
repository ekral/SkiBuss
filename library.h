//
// Created by Laura on 15/11/2024.
//

#ifndef UNTITLED2_LIBRARY_H
#define UNTITLED2_LIBRARY_H

int create_semaphore(sem_t** semaphore, const char* name, const int init)
{
    *semaphore = sem_open(name, O_CREAT, 0755, init);

    if (*semaphore == SEM_FAILED)
    {
        return -1;
    }

    return 0;
}

int acquire_semaphore(const sem_t* semaphore)
{
    const int result = sem_wait(semaphore);

    return result;
}

int release_semaphore(const sem_t* semaphore)
{
    const int result = sem_post(semaphore);

    return result;
}

void destroy_semaphore_(const sem_t* semaphore, const char* name)
{
    sem_close(semaphore);
    sem_unlink(name);
}

typedef struct
{
    const char* name;
    void* data;
    int size;
} named_memory_t;

void* named_memory_create(named_memory_t* named_memory, const char* name, const int size)
{
    named_memory->name = name;
    named_memory->size = size;

    named_memory->data = (void*)-1;

    const int fd = shm_open(name, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);

    if(fd != -1)
    {
        if(ftruncate(fd, size) != -1)
        {
            named_memory->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        }
        else
        {
            return -1;
        }

        // po namapovani muzu fd zavrit
        if (close(fd) != 0)
        {
            return -1;
        }

        return 0;
    }
    else
    {
        return -1;
    }

    return named_memory->data;
}

int named_memory_destroy(named_memory_t* named_memory)
{
    munmap(named_memory->data, named_memory->size);
    int result = shm_unlink(named_memory->name);

    named_memory->data = NULL;

    return result;
}


#endif //UNTITLED2_LIBRARY_H
