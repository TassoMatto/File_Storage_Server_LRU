/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione della memoria cache con politica di espulsone LRU
 * @author              Simone Tassotti
 * @date                22/12/2021
 */

#ifndef FILE_STORAGE_SERVER_LRU_FILESTORAGESERVER_H

    #define FILE_STORAGE_SERVER_LRU_FILESTORAGESERVER_H

    #define _POSIX_C_SOURCE 2001112L
    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <errno.h>
    #include <pthread.h>
    #include <icl_hash.h>
    #include <file.h>
    #include <logFile.h>
    #include <utils.h>
    #include <queue.h>


    #define DEFAULT_NUMERO_THREAD_WORKER 10
    #define DEFAULT_MAX_MB 100
    #define DEFAULT_SOCKET "./socket.sk"
    #define DEFUALT_MAX_NUMERO_FILE 20
    #define DEFUALT_MAX_NUMERO_UTENTI 15


    /**
     * @brief                           Struttura che contiene le informazioni di base del server lette nel config file
     * @struct                          Settings
     * @param socket                    Socket usato per la comunicazione client-server
     * @param maxMB                     Numero massimo di MB che posso caricare
     * @param numeroThreadWorker        Numero di thread da avviare nel pool
     * @param maxNumeroFileCaricaribili Numero massimo di file da caricare nel server
     * @param maxUtentuConnessi         Numero massimo di utenti che posso far connettere al server
     * @param maxUtentiPerFile          Numero massimo di utenti che puo' aprire il file contemporaneamente
     */
    typedef struct {
        /** Capacita' del server **/
        char *socket;
        ssize_t maxMB;
        unsigned int numeroThreadWorker;
        unsigned int maxNumeroFileCaricabili;
        unsigned int maxUtentiConnessi;
        unsigned int maxUtentiPerFile;
    } Settings;


    typedef struct {
        icl_hash_t *pI;
        pthread_mutex_t *lockPI;
        serverLogFile *log;
    } Pre_Inserimento;


    typedef struct {
        int fd_cl;
        myFile *f;
    } ClientFile;


    /**
     * @brief                               Struttura dati per rappresentare la cache con politica LRU
     * @struct                              LRU_Memory
     * @param tabella                       Tabella Hash dove inserisco i file memorizzati nella cache
     * @param LRU                           Array di "myFile" che tiene ordinato in modo temporale
     *                                      crescente i file della tabella
     * @param log                           File di log per il tracciamento delle operazioni della cache
     * @param LRU_Access                    Mutex per l'accesso concorrente nella tabella
     * @param Files_Access                  Mutex che vengono assegnate ai file per l'accesso agli stessi in modo concorrente
     * @param maxBytesOnline                Numero massimo di bytes che posso memorizzare nella cache
     * @param maxFileOnline                 Numero massimo di file che posso caricare in memoria cache
     * @param maxUtentiPerFile              Numero massimo di utenti che posso aprire un singolo file contemporaneamente
     * @param bytesOnline                   Numero di bytes caricati in quel'istante
     * @param fileOnline                    Numero di file caricati in quel momento
     * @param massimoNumeroDiFileOnline     Numero massimo di file che sono stati caricati
     * @param numeroMassimoBytesCaricato    Numero massimo di byte che sono stati caricati
     * @param numeroMemoryMiss              Numero di espulsioni che la cache ha fatto
     * @param numTotLogin                   Numero di login effettuati nel server
     */
    typedef struct {
        /** Strutture dati **/
        icl_hash_t *tabella;
        myFile **LRU;
        serverLogFile *log;
        pthread_mutex_t *LRU_Access;
        pthread_mutex_t *Files_Access;

        /** Informazioni capacitive **/
        ssize_t maxBytesOnline;
        unsigned int maxFileOnline;
        unsigned int maxUtentiPerFile;

        /** Valori attuali **/
        ssize_t bytesOnline;
        unsigned int fileOnline;

        /** Informazioni statistiche **/
        unsigned int massimoNumeroDiFileOnline;
        ssize_t numeroMassimoBytesCaricato;
        unsigned int numeroMemoryMiss;
        unsigned int numTotLogin;
    } LRU_Memory;


    /**
     * @brief                           Legge il contenuto del configFile del server e lo traduce in una struttura in memoria principale
     * @fun                             readConfigFile
     * @return                          Ritorna la struttura delle impostazioni del server; in caso di errore ritorna NULL [setta errno]
     */
    Settings* readConfigFile(const char *);


    /**
     * @brief                           Inizializza la struttura del server con politica LRU
     * @fun                             startLRUMemory
     * @return                          Ritorna la struttura della memoria in caso di successo; NULL altrimenti [setta errno]
     */
    LRU_Memory* startLRUMemory(Settings *, serverLogFile *);


    /**
     * @brief                   Controlla se un file esiste o meno nel server (per uso esterno al file FileStorageServer.c)
     * @fun                     fileExist
     * @return                  (1) se il file esiste; (0) se non esiste; (-1) in caso di errore nella ricerca
     */
    int fileExist(LRU_Memory *, const char *);


    /**
     * @brief                       Apre un file da parte di un fd sulla memoria cache
     * @fun                         openFileOnCache
     * @return                      (1) se file e' gia stato aperto; (0) se e' stato aperto con successo;
     *                              (-1) in caso di errore [setta errno]
     */
    int openFileOnCache(LRU_Memory *cache, const char *pathname, int openFD);


    /**
     * @brief                   Creo la tabella che conterranno i file vuoti
     *                          non ancora scritti e salvati nella cache
     * @fun                     createPreInserimento
     * @return                  Ritorna la struttura di Pre-Inserimento; in caso di errore [setta errno]
     */
    Pre_Inserimento* createPreInserimento(int, serverLogFile *);


    /**
     * @brief                    Crea il file che successivamente verra' aggiunto alla memoria cache
     * @fun                     createFileToInsert
     * @return                  (0) in caso di successo; (-1) [setta errno] altrimenti
     */
    int createFileToInsert(Pre_Inserimento *, const char *, unsigned int, int, int);


    /**
     * @brief               Funzione che aggiunge il file creato da fd nella memoria cache (gia' creato e aggiunto in Pre-Inserimento)
     * @fun                 addFileOnCache
     * @return              Ritorna i file espulsi in seguito a memory miss oppure NULL; in caso di errore nell'aggiunta del file
     *                      viene settato errno
     */
    myFile** addFileOnCache(LRU_Memory *, Pre_Inserimento *, const char *, int);


    /**
     * @brief                   Rimuove un file dalla memoria cache
     * @fun                     removeFileOnCache
     * @return                  Ritorna il file cancellato; in caso di errore ritorna NULL [setta errno]
     */
    myFile* removeFileOnCache(LRU_Memory *, const char *);


    /**
    * @brief                       Aggiorna il contenuto di un file in modo atomico
    * @fun                         appendFile
    * @return                      Ritorna gli eventuali file espulsi; in caso di errore valutare se si setta errno
    */
    myFile** appendFile(LRU_Memory *, const char *, void *, ssize_t);


    /**
     * @brief                   Funzione che legge il contenuto del file e ne restituisce una copia
     * @fun                     readFileOnCache
     * @return                  Ritorna la dimensione del buffer; (-1) altrimenti [setta errno]
     */
    size_t readFileOnCache(LRU_Memory *, const char *, void **);


    /**
     * @brief                   Effettua la lock su un file per quel fd
     * @fun                     lockFileOnCache
     * @return                  Ritorna (1) se il file e' locked gia'; (0) se la lock e' riuscita;
     *                          (-1) in caso di errore [setta errno]
     */
    int lockFileOnCache(LRU_Memory *, const char *, int);


    /**
     * @brief                   Effettua la unlock su un file per quel fd
     * @fun                     unlockFileOnCache
     * @return                  (0) se la unlock e' riuscita;
     *                          (-1) in caso di errore [setta errno]
     */
    int unlockFileOnCache(LRU_Memory *, const char *, int);


    /**
     * @brief                   Cancella tutta la memoria e i settings della memoria cache
     * @fun                     deleteLRU
     */
    void deleteLRU(Settings **, LRU_Memory **, Pre_Inserimento **);


#endif //FILE_STORAGE_SERVER_LRU_FILESTORAGESERVER_H
