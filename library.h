//
// Created by erik on 14.11.2024.
//

#ifndef LIBRARY_H
#define LIBRARY_H

#endif //LIBRARY_H

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
        puts(name);
        p = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        if(p == MAP_FAILED)
        {
            perror("mmap");
        }

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