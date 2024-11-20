# Knihovna library.h

Knihovna slouží pro práci se sdílenou paměti a semafory mezi procesy.

## Práce se sdílenou pamětí

K práci se sdílenou pamětí slouží funkce `create_shared` a `get_shared`.

1. Vytvoření sdílené paměti

Nejprve se ve funkci `main` zavolá funkce `create_shared`, jméno musí začínat
znakem `/`. Pro zabránění překlepům v názvu si nadefinujeme symbolickou konstantu, například `LOG_SEMAPHORE_NAME` a `LOG_COUNT_LENGTH`.

Funkce v případě chyby vrací hodnotu `MAP_FAILED` (což je `(void*)-1`), jinak vrací ukazatel na sdílenou paměť.

Funkce `create_shared` zavře shared memory object, proto na konci programu už jen uvolníme uvolníme sdílenou paměť z operačního systému příkazem `shm_unlink`.


```cpp
#define LOG_COUNT_NAME "/A"
#define LOG_COUNT_LENGTH sizeof(int)

int* ptr_log_count = create_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);

if(ptr_log_count == MAP_FAILED)
{
    perror("Error creating ptr_log_count");
    goto fail_ptr_log_count;
}

// dalsi kod

shm_unlink(LOG_COUNT_NAME);
```

2. Použití sdílené paměti

Pro použití sdílené paměti se zavolá funkce `get_shared`, která má stejné parametry jako funkce `create_shared`.

Funkce v případě chyby opět vrací hodnotu `MAP_FAILED` (což je `(void*)-1`), jinak vrací ukazatel na sdílenou paměť.

Na konci uvolníme sdílenou paměť příkazem munmap, ale sdílená paměť zůstává v operačním systému pro další použití.
```cpp
int* ptr_log_count = get_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);
if(ptr_log_count == (void*)-1)
{
    perror("Error creating ptr_log_count");
    goto fail_ptr_log_count;
}

// dalsi kod

munmap(LOG_COUNT_NAME, LOG_COUNT_LENGTH);
```

## Práce se semafory

1. Vytvoření semaforu

Nejprve se ve funkci `main` zavolá funkce `create_semaphore`, jméno musí začínat
znakem `/`. Pro zabránění chybám v názvu si nadefinujeme symbolickou konstantu, například `MUTEX_NAME`.

Funkce v případě chyby vrací hodnotu `SEM_FAILED` (což je `(sem_t*)0`), jinak vrací ukazatel na sdílenou paměť.

Na konci programu uvolníme sdílenou paměť z operačního systému příkazem `sem_unlink`.

Všechny otevřené semafory jsou automaticky uzavřené (closed), ale je dobrá praxe zavolat explicitně `sem_close`.
```cpp
sem_t* mutex = create_semaphore(MUTEX_NAME, 1);
if(mutex == SEM_FAILED)
{
    perror("Error creating mutex");
    goto fail_mutex;
}
sem_close(mutex);   // vytvořili jsme jej, ale už s ním nepracujeme

// dalsi kod

sem_unlink(MUTEX_NAME);
```
2. Použití semaforu
