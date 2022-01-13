/**
 * @project             FILE_STORAGE_SERVER
 * @brief               API per la connessione al server da parte del client
 * @author              Simone Tassotti
 * @date                07/01/2022
 */

#ifndef FILE_STORAGE_SERVER_LRU_CLIENT_API_H


    #define FILE_STORAGE_SERVER_LRU_CLIENT_API_H


    #define _POSIX_C_SOURCE 2001112L
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
    #include <sys/un.h>
    #include <sys/socket.h>


    /**
     * @brief                   Tenta la connessione al server tramite la socket specificata
     * @fun                     openConnection
     * @return                  (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int openConnection(const char *, int, const struct timespec);


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

int lockFile(const char *);
int unlockFile(const char *);
int closeFile(const char *);


    /**
     * @brief                   Rimuovo un file dal server
     * @fun                     removeFile
     * @return                  (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int removeFile(const char *);


#endif //FILE_STORAGE_SERVER_LRU_CLIENT_API_H
