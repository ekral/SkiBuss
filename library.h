//
// Created by Laura on 15/11/2024.
//

#ifndef UNTITLED2_LIBRARY_H
#define UNTITLED2_LIBRARY_H

typedef struct
{
    const char* name;
    sem_t* sem;
} named_semaphore_t;

sem_t* named_semaphore_init(named_semaphore_t* named_semaphore, const char* name, const int init)
{
    named_semaphore->name = name;

    named_semaphore->sem = sem_open(name, O_CREAT, 0755, init);

    return named_semaphore->sem;
}

int named_semaphore_wait(const named_semaphore_t* named_semaphore)
{
    int result = sem_wait(named_semaphore->sem);

    return result;
}

int named_semaphore_post(const named_semaphore_t* named_semaphore)
{
    int result = sem_post(named_semaphore->sem);

    return result;
}

void named_semaphore_destroy(named_semaphore_t* named_semaphore)
{
    sem_close(named_semaphore->sem);
    sem_unlink(named_semaphore->name);

    named_semaphore->sem = NULL;
}

typedef struct
{
    const char* name;
    void* data;
    int size;
} named_memory_t;

void* named_memory_init(named_memory_t* named_memory, const char* name, const int size)
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

        close(fd); // po namapovani muzu fd zavrit
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
