# Knihovna library.h

Knihovna slouží pro práci se sdílenou paměti a semafory mezi procesy.

K práci se sdílenou pamětí slouží funkce `create_shared` a `get_shared`.

Nejprve se ve funkci `main` zavolá funkce `create_shared`, jméno musí začínat
znakem `/`. Pro zabránění chybám v názvu si nadefinujeme symbolickou konstantu, například `LOG_SEMAPHORE_NAME`.


```cpp
#define LOG_SEMAPHORE_NAME "/log"

int* ptr_log_count = create_shared(LOG_COUNT_NAME,LOG_COUNT_LENGTH);

if(ptr_log_count == (void*)-1)
{
    perror("Error creating ptr_log_count");
    goto fail_ptr_log_count;
}

// dalsi kod

shm_unlink(LOG_COUNT_NAME);
```