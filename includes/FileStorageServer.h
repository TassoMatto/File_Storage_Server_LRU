/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione della memoria cache con politica di espulsone LRU
 * @author              Simone Tassotti
 * @date                22/12/2021
 */

#ifndef FILE_STORAGE_SERVER_LRU_FILESTORAGESERVER_H

    #define FILE_STORAGE_SERVER_LRU_FILESTORAGESERVER_H


    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 2001112L
    #endif


    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <errno.h>
    #include <pthread.h>
    #include <math.h>
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


    /**
     * @brief           Struttura che rappresenta l'apertura (ma non creazione)
     *                  del file in memoria
     * @struct          ClientFile
     * @param fd_cl     Fd del client che apre il file
     * @param f         File aperto dal client
     */
    typedef struct {
        int fd_cl;
        myFile *f;
    } ClientFile;


    /**
     * @brief           Tiene informazioni dei file aperti da un client
     * @struct          userLink
     * @param fd        Client di riferimento
     * @param openFile  Nome del file che ha puntato
     */
    typedef struct {
        int fd;
        char *openFile;
    } userLink;


    /**
     * @brief                               Struttura dati per rappresentare la cache con politica LRU
     * @struct                              LRU_Memory
     * @param usersConnected                Lista degli utenti connessi al server
     * @param usersConnectedAccess          Variabile di mutex per accedere alla lista degli utenti connessi
     * @param notAdded                      Tabella dei file non realmente aggiunti ma solo aperti
     * @param notAddedAccess                Mutex per accesso concorrente ai file "solo aperti"
     * @param tabella                       Tabella Hash dove inserisco i file memorizzati nella cache
     * @param LRU                           Array di "myFile" che tiene ordinato in modo temporale
     *                                      crescente i file della tabella
     * @param log                           File di log per il tracciamento delle operazioni della cache
     * @param LRU_Access                    Mutex per l'accesso concorrente nella tabella
     * @param Files_Access                  Mutex che vengono assegnate ai file per l'accesso agli stessi in modo concorrente
     * @param maxBytesOnline                Numero massimo di bytes che posso memorizzare nella cache
     * @param maxUsersLoggedOnline          Numero massimo di connessioni nel server
     * @param maxFileOnline                 Numero massimo di file che posso caricare in memoria cache
     * @param maxUtentiPerFile              Numero massimo di utenti che posso aprire un singolo file contemporaneamente
     * @param bytesOnline                   Numero di bytes caricati in quel'istante
     * @param fileOnline                    Numero di file caricati in quel momento
     * @param usersLoggedNow                Numero di utenti connessi in questo istante
     * @param massimoNumeroDiFileOnline     Numero massimo di file che sono stati caricati
     * @param numeroMassimoBytesCaricato    Numero massimo di byte che sono stati caricati
     * @param numeroMemoryMiss              Numero di espulsioni che la cache ha fatto
     * @param numTotLogin                   Numero di login totali nel server
     */
    typedef struct {
        /** Strutture dati **/
        Queue **usersConnected;
        pthread_mutex_t *usersConnectedAccess;
        icl_hash_t *notAdded;
        pthread_mutex_t *notAddedAccess;
        icl_hash_t *tabella;
        myFile **LRU;
        serverLogFile *log;
        pthread_mutex_t *LRU_Access;
        pthread_mutex_t *Files_Access;

        /** Informazioni capacitive **/
        size_t maxBytesOnline;
        unsigned int maxUsersLoggedOnline;
        unsigned int maxFileOnline;
        unsigned int maxUtentiPerFile;

        /** Valori attuali **/
        size_t bytesOnline;
        unsigned int fileOnline;
        unsigned int usersLoggedNow;
        unsigned int usersOnlineOpenFile;

        /** Informazioni statistiche **/
        unsigned int massimoNumeroDiFileOnline;
        size_t numeroMassimoBytesCaricato;
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
     * @brief           Mi segnala il login di un client
     * @fun             loginClient
     * @return          Ritorna 0 in caso di successo; -1 altrimenti [setta errno]
     */
    int loginClient(LRU_Memory *);


    /**
     * @brief               Mi indica il numero di client online in quell'istante
     * @fun                 clientOnline
     * @return              Ritorna il numero di client connessi; [setta errno] in caso di errore
     */
    unsigned int clientOnline(LRU_Memory *);


    /**
     * @brief           Segnala la disconnessione di un client
     * @fun             logoutClient
     * @return          Ritorna (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int logoutClient(LRU_Memory *);


    /**
     * @brief                    Crea il file che successivamente verra' aggiunto alla memoria cache
     * @fun                     createFileToInsert
     * @return                  (0) in caso di successo; (-1) [setta errno] altrimenti
     */
    int createFileToInsert(LRU_Memory *, const char *, unsigned int, int, int);


    /**
     * @brief                       Apre un file da parte di un fd sulla memoria cache
     * @fun                         openFileOnCache
     * @return                      (1) se file e' gia stato aperto; (0) se e' stato aperto con successo;
     *                              (-1) in caso di errore [setta errno]
     */
    int openFileOnCache(LRU_Memory *cache, const char *pathname, int openFD);


    /**
     * @brief                   Chiude un file aperto da 'closeFD' (e lo unlocka se anche locked)
     * @fun                     closeFileOnCache
     * @return                  In caso di successo ritorna 0 o FD del client che ora detiene la lock
     *                          del file dopo 'closeFD'; -1 altrimenti e setta errno
     */
    int closeFileOnCache(LRU_Memory *cache, const char *pathname, int closeFD);


    /**
     * @brief               Funzione che aggiunge il file creato da fd nella memoria cache (gia' creato e aggiunto in Pre-Inserimento)
     * @fun                 addFileOnCache
     * @return              Ritorna i file espulsi in seguito a memory miss oppure NULL; in caso di errore nell'aggiunta del file
     *                      viene settato errno
     */
    myFile** addFileOnCache(LRU_Memory *, const char *, int, int);


    /**
     * @brief                   Rimuove un file dalla memoria cache
     * @fun                     removeFileOnCache
     * @return                  Ritorna il file cancellato; in caso di errore ritorna NULL [setta errno]
     */
    myFile* removeFileOnCache(LRU_Memory *, const char *, int);


    /**
    * @brief                       Aggiorna il contenuto di un file in modo atomico
    * @fun                         appendFile
    * @return                      Ritorna gli eventuali file espulsi; in caso di errore valutare se si setta errno
    */
    myFile** appendFile(LRU_Memory *, const char *, int, void *, size_t);


    /**
     * @brief                   Funzione che legge il contenuto del file e ne restituisce una copia
     * @fun                     readFileOnCache
     * @return                  Ritorna la dimensione del buffer; (-1) altrimenti [setta errno]
     */
    size_t readFileOnCache(LRU_Memory *, const char *, int, void **);


    /**
     * @brief               Legge N file in ordine della LRU non locked
     * @fun                 readRandFiles
     * @return              Ritorna i file letti effettivamente; se ci sono errori viene settato errno e
     *                      viene ritornato NULL
     */
    myFile **readsRandFiles(LRU_Memory *, int, int *);


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
     * @return                  In caso di successo ritorna Fd del client da sbloccare;
     *                          (-1) in caso di errore [setta errno]
     */
    int unlockFileOnCache(LRU_Memory *, const char *, int);


    /**
     * @brief               Funzione che elimina ogni pendenza di un client disconnesso
     *                      su tutti i file in gestione a lui
     * @fun                 deleteClientFromCache
     * @return              Ritorna un puntatore ad un array di fd che specifica i client che sono in attesa dei file
     *                      locked da 'fd'; altrimenti, in caso di errore, setta errno
     */
    int* deleteClientFromCache(LRU_Memory *, int);


    /**
     * @brief                   Cancella tutta la memoria e i settings della memoria cache
     * @fun                     deleteLRU
     */
    void deleteLRU(Settings **, LRU_Memory **);


#endif //FILE_STORAGE_SERVER_LRU_FILESTORAGESERVER_H
