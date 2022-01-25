/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Funzioni di diverse utilita'
 * @author              Simone Tassotti
 * @date                23/12/2021
 * @finish              25/01/2022
 */

#ifndef FILE_STORAGE_SERVER_LRU_UTILS_H

    #define FILE_STORAGE_SERVER_LRU_UTILS_H

    #define MAX_PATHNAME 2048
    #define MAX_BUFFER_LEN 10000


    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <unistd.h>
    #include <errno.h>


    /**
     * @brief                   Data una stringa la converte in valore long
     * @fun                     isNumber
     * @return                  In caso di successo ritorna il valore numero della stringa, altrimenti -1
     */
    long isNumber(const char*);


    /**
     * brief                Manda un messaggio alla server sulla socket indicata
     * @fun                 sendMSG
     * @return              Ritorna il numero di byte scritti o -1 in caso di errore [setta errno]
     */
    ssize_t sendMSG(int, void *, size_t);


    /**
     * @brief               Riceve un messaggio dalla socket indicata
     * @fun                 receiveMSG
     * @return              In caso di successo ritorna il numero di bytes letti; altrimenti
     *                      -1 [setta errno]
     */
    ssize_t receiveMSG(int, void **, size_t *);



#endif //FILE_STORAGE_SERVER_LRU_UTILS_H
