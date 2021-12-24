/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Funzioni di diverse utilita'
 * @author              Simone Tassotti
 * @date                23/12/2021
 */

#ifndef FILE_STORAGE_SERVER_LRU_UTILS_H

    #define FILE_STORAGE_SERVER_LRU_UTILS_H

    #define MAX_PATHNAME 2048
    #define MAX_BUFFER_LEN 10000


    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>


    /**
     * @brief                   Data una stringa la converte in valore long
     * @fun                     isNumber
     * @return                  In caso di successo ritorna il valore numero della stringa, altrimenti -1
     */
    long isNumber(const char*);


#endif //FILE_STORAGE_SERVER_LRU_UTILS_H
