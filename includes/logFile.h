/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione di un file di log di un server
 * @author              Simone Tassotti
 * @date                22/12/2021
 */

#ifndef FILE_STORAGE_SERVER_LRU_LOGFILE_H

    #define FILE_STORAGE_SERVER_LRU_LOGFILE_H

    #define MAX_STRING_TIME 26

    #define _POSIX_C_SOURCE 2001112L
    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <time.h>
    #include <errno.h>
    #include <stdarg.h>
    #include <pthread.h>
    #include <utils.h>


    /**
     * @brief               Struttura dati che gestisce un file di log di un server
     * @struct              serverLogFile
     * @param pathname      Pathname del file di log
     * @param file          Puntatore al file di log
     * @param lockFile      Mutex per l'accesso concorrente al file
     */
    typedef struct {
        char *pathname;
        FILE *file;
        pthread_mutex_t *lockFile;
    } serverLogFile;


    /**
     * @brief               Crea la struttura dati con file di log e variabile per accesso concorrente
     * @fun                 startServerTracing
     * @return              Ritorna un puntatore alla struttura dati che gestisce il file di log; in caso di errore ritorna (-1) [setta errno]
     */
    serverLogFile* startServerTracing(const char *);


    /**
    * @brief               Scrive una stringa sul file di log con data e ora della scrittura su file
    * @fun                 traceOnLog
    * @return              Se non ci sono errori ritorna (0), altrimenti ritorna (-1) [setta errno]
    */
    int traceOnLog(serverLogFile *, char* , ...);


    /**
    * @brief               Distrugge la struttura del file di log e lo chiude
    * @fun                 stopServerTracing
    * @exception           In caso di errore [setta errno]
    */
    void stopServerTracing(serverLogFile **);



#endif //FILE_STORAGE_SERVER_LRU_LOGFILE_H
