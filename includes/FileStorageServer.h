/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione della memoria cache con politica di espulsone LRU
 * @author              Simone Tassotti
 * @date                22/12/2021
 */

#ifndef FILE_STORAGE_SERVER_LRU_FILESTORAGESERVER_H

    #define FILE_STORAGE_SERVER_LRU_FILESTORAGESERVER_H

    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <errno.h>
    #include <icl_hash.h>
    #include <utils.h>


    #define DEFAULT_NUMERO_THREAD_WORKER 10
    #define DEFAULT_MAX_MB 100
    #define DEFAULT_SOCKET "./socket.sk"
    #define DEFUALT_MAX_NUMERO_FILE 20
    #define DEFUALT_MAX_NUMERO_UTENTI 15


    /**
     * @brief                           Struttura che contiene le informazioni di base del server lette nel config file
     * @struct                          LRU_Memory
     * @param socket                    Socket usato per la comunicazione client-server
     * @param maxMB                     Numero massimo di MB che posso caricare
     */
    typedef struct {
        /** Capacita' del server **/
        char *socket;
        size_t maxMB;
        unsigned int numeroThreadWorker;
        unsigned int maxNumeroFileCaricabili;
        unsigned int maxUtentiConnessi;
        unsigned int maxUtentiPerFile;

        /** Data Center **/
        icl_hash_t *tabellaDeiFile;
        int *LRU;
        // Array che gestisce la politica LRU
        // Coda che gestisce i file che dovranno essere aggiunti

        /** Valori attuali del server **/
        size_t onlineBytes;
        unsigned int onlineFile;
        unsigned int utentiOnline;

        /** Valori statistici del server **/
    } LRU_Memory;


    /**
     * @brief                           Legge il contenuto del configFile del server e lo traduce in una struttura in memoria principale
     * @fun                             readConfigFile
     * @return                          Ritorna la struttura delle impostazioni del server; in caso di errore ritorna NULL [setta errno]
     */
    LRU_Memory* readConfigFile(const char *);


    void deleteLRU(LRU_Memory **);


#endif //FILE_STORAGE_SERVER_LRU_FILESTORAGESERVER_H
