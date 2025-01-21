//
// Created by Laura on 15/11/2024.
//

#ifndef UNTITLED2_LIBRARY_H
#define UNTITLED2_LIBRARY_H

#define NAME_LENGTH 256

typedef struct {
    const char* name;
    sem_t semaphore;
} named_semaphore_t;

typedef struct {
    int id;
    sem_t semaphore;
} indexed_semaphore_t;

inline int create_named_semaphore(named_semaphore_t* named_semaphore, const char* name, const int init)
{
    named_semaphore->name = name;

    named_semaphore->semaphore = sem_open(named_semaphore->name, O_CREAT, 0755, init);

    if (named_semaphore->semaphore == SEM_FAILED)
    {
        return -1;
    }

    return 0;
}

inline void format_name(char* const str, const int len, const long id)
{
    snprintf(str, len, "/bus%ld", id);
}


inline int create_indexed_semaphore(indexed_semaphore_t* indexed_semaphore, const int id, const int init)
{
    indexed_semaphore->id = id;

    char name[NAME_LENGTH];
    format_name(name, NAME_LENGTH, indexed_semaphore->id);

    indexed_semaphore->semaphore = sem_open(name, O_CREAT, 0755, init);

    if (indexed_semaphore->semaphore == SEM_FAILED)
    {
        return -1;
    }

    return 0;
}


inline int acquire_semaphore(sem_t* semaphore)
{
    const int result = sem_wait(semaphore);

    return result;
}

inline int release_semaphore(sem_t* semaphore)
{
    const int result = sem_post(semaphore);

    return result;
}

inline int destroy_named_semaphore_(named_semaphore_t* named_semaphore)
{
    int result = 0;

    if (sem_close(named_semaphore->semaphore) == -1)
    {
        result = -1;
    }

    if (sem_unlink(named_semaphore->name) == -1)
    {
        result = -1;
    }

    named_semaphore->semaphore = NULL;

    return result;
}

inline int destroy_indexed_semaphore_(indexed_semaphore_t* indexed_semaphore)
{
    int result = 0;

    if (sem_close(indexed_semaphore->semaphore) == -1)
    {
        result = -1;
    }

    char name[NAME_LENGTH];
    format_name(name, NAME_LENGTH, indexed_semaphore->id);

    if (sem_unlink(name) == -1)
    {
        result = -1;
    }

    indexed_semaphore->semaphore = NULL;

    return result;
}

typedef struct
{
    const char* name;
    void* data;
    int size;
} named_memory_t;

inline int named_memory_create(named_memory_t* named_memory, const char* name, const int size)
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

    return 0;
}

inline int named_memory_destroy(named_memory_t* named_memory)
{
    int result = 0;

    if (munmap(named_memory->data, named_memory->size) == -1)
    {
        result = -1;
    }

    if (shm_unlink(named_memory->name) == -1)
    {
        result = -1;
    }

    named_memory->data = NULL;

    return result;
}


#endif //UNTITLED2_LIBRARY_H
