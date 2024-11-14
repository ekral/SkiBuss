# Knihovna library.h

Knihovna slouží pro práci se sdílenou paměti a semafory mezi procesy.

## Práce se sdílenou pamětí

K práci se sdílenou pamětí slouží funkce `create_shared` a `get_shared`.

Nejprve se ve funkci `main` zavolá funkce `create_shared`, jméno musí začínat
znakem `/`. Pro zabránění chybám v názvu si nadefinujeme symbolickou konstantu, například `LOG_SEMAPHORE_NAME` a `LOG_COUNT_LENGTH`.

Funkce v případě chyby vrací hodnotu `(void*)-1`, jinak vrací ukazatel na sdílenou paměť.

Na konci programu uvolníme sdílenou paměť z operačního systému příkazem `shm_unlink`.


```cpp
#define LOG_COUNT_NAME "/A"
#define LOG_COUNT_LENGTH sizeof(int)

int* ptr_log_count = create_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);

if(ptr_log_count == (void*)-1)
{
    perror("Error creating ptr_log_count");
    goto fail_ptr_log_count;
}

// dalsi kod

shm_unlink(LOG_COUNT_NAME);
```

