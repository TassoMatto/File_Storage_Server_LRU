/**
 * @project             FILE_STORAGE_SERVER
 * @brief               API per la connessione al server da parte del client
 * @author              Simone Tassotti
 * @date                07/01/2022
 */

#ifndef FILE_STORAGE_SERVER_LRU_CLIENT_API_H


    #define FILE_STORAGE_SERVER_LRU_CLIENT_API_H
    #define _POSIX_C_SOURCE 2001112L
    #define O_CREATE 127
    #define O_LOCK 128


    #include <stdlib.h>
    #include <stdio.h>
    #include <unistd.h>
    #include <string.h>
    #include <errno.h>
    #include <pthread.h>
    #include <math.h>
    #include <utils.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <signal.h>
    #include <sys/un.h>
    #include <sys/socket.h>
    #include <dirent.h>
    #include <limits.h>
    #include <queue.h>


    /**
     * @brief                   Calcola il path assoluto del file
     * @fun                     abs_path
     * @return                  In caso di successo ritorna il path assoluto del file;
     *                          altrimenti ritorna NULL [setta errno]
     */
    char* abs_path(const char *);


    /**
     * @brief                   Legge il file puntato da pathname dal disco
     * @fun                     readFileFromDisk
     * @return                  In caso di successo ritorna (0); (-1) altrimenti [setta errno]
     */
    int readFileFromDisk(const char *, void **, size_t *);


    /**
     * @brief                   Scrive un file con contenuto buf e dimensione size
     * @fun                     writeFileIntoDisk
     * @return                  (0) in caso di successo; (-1) altriment
     */
    int writeFileIntoDisk(const char *, const char *, void *, size_t);


    char** readNFileFromDir(const char *, size_t);


    /**
     * @brief                   Tenta la connessione al server tramite la socket specificata
     * @fun                     openConnection
     * @return                  (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int openConnection(const char *, int, struct timespec);


    /**
     * @brief               Termina la connessione con il server
     * @fun                 closeConnection
     * @return              Ritorna (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int closeConnection(const char *);


    /**
     * @brief                   Apre/Crea un file nel server
     * @fun                     openFile
     * @return                  (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int openFile(const char *, int);


    /**
     * @brief                   Chiede la lettura di un file dal server
     * @fun                     readFile
     * @return                  Ritorna (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int readFile(const char *, void **, size_t *);


    /**
     * @brief               Legge N file random e li scrive nella dirname
     * @fun                 readNFiles
     * @return              Ritorna il numero effettivo di file letti dal server;
     *                      in caso di errore ritorna (-1) [setta errno]
     */
    int readNFiles(int, const char *);


    /**
     * brief                Scrive tutto il file puntato da pathname nel server
     * @fun                 writeFile
     * @return              (0) in caso di successo; (-1) altrimenti
     */
    int writeFile(const char *, const char *);


    /**
     * @brief               Aggiungo il contenuto di buf, di dimensione size nel server
     * @fun                 appendToFile
     * @return              Ritorna (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int appendToFile(const char *, void *, size_t, const char *);


    /**
     * @brief               Effettua la lock di 'pathname' nel server
     * @fun                 lockFile
     * @return              Ritorna 0 in caso di successo; -1 in caso di errori
     *                      e setta errno
     */
    int lockFile(const char *);


    /**
     * @brief               Effettua la unlock di 'pathname' nel server
     * @fun                 unlockFile
     * @return              Ritorna 0 in caso di successo; -1 in caso di errori
     *                      e setta errno
     */
    int unlockFile(const char *);


    /**
     * @brief               Chiude un file nel server
     * @fun                 closeFile
     * @return              In caso di successo ritorna 0; altrimenti
     *                      ritorna -1 e setta errno
     */
    int closeFile(const char *);


    /**
     * @brief                   Rimuovo un file dal server
     * @fun                     removeFile
     * @return                  (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int removeFile(const char *);


#endif //FILE_STORAGE_SERVER_LRU_CLIENT_API_H
