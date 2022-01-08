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


#endif //FILE_STORAGE_SERVER_LRU_CLIENT_API_H
