/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione di un file di log di un server
 * @author              Simone Tassotti
 * @date                22/12/2021
 */

#include "logFile.h"


/**
 * @brief               Effettua la lock del file di log
 * @macro               LOCK_LOG_FILE
 * @param LOG           Struttura del file di log
 */
#define LOCK_LOG_FILE(LOG)                                              \
    do {                                                                \
        int error = 0;                                                  \
        if((error = pthread_mutex_lock(LOG)) != 0) {        \
            errno = error;                                              \
            perror("Fatal Error");                                      \
            return -1;                                                  \
        }                                                               \
    } while(0);


/**
 * @brief               Effettua la UNlock del file di log
 * @macro               UNLOCK_LOG_FILE
 * @param LOG           Struttura del file di log
 */
#define UNLOCK_LOG_FILE(LOG)                                        \
    do {                                                            \
        int error = 0;                                              \
        if((error = pthread_mutex_unlock(LOG)) != 0) {  \
            errno = error;                                          \
            perror("Fatal Error");                                  \
            return -1;                                              \
        }                                                           \
    } while(0);


/**
 * @brief               Crea la struttura dati con file di log e variabile per accesso concorrente
 * @fun                 startServerTracing
 * @param pathname      Pathname del file di log da istanziare
 * @return              Ritorna un puntatore alla struttura dati che gestisce il file di log; in caso di errore ritorna (NULL) [setta errno]
 */
serverLogFile* startServerTracing(const char *pathname) {
    /** Variabili **/
    int error = 0;
    serverLogFile *myLogFile = NULL;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return NULL; }

    /** Alloco la struttura con file di log e variabile di accesso in mutua esclusione **/
    if((myLogFile = (serverLogFile *) malloc(sizeof(serverLogFile))) == NULL) return NULL;
    if(((myLogFile->lockFile) = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t))) == NULL) { free(myLogFile); return NULL; }
    if((error = pthread_mutex_init((myLogFile->lockFile), NULL)) != 0) { free(myLogFile->lockFile); free(myLogFile); errno = error; return NULL; }
    if((myLogFile->file = fopen(pathname, "w")) == NULL) { pthread_mutex_destroy(myLogFile->lockFile); free(myLogFile->lockFile); free(myLogFile); return NULL; }
    if((myLogFile->pathname = (char *) calloc(strnlen(pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) { error = errno; fclose(myLogFile->file); pthread_mutex_destroy(myLogFile->lockFile); free(myLogFile->lockFile); free(myLogFile); errno = error; return NULL; }
    strncpy(myLogFile->pathname, pathname, strnlen(pathname, MAX_PATHNAME));

    return myLogFile;
}


/**
 * @brief               Scrive una stringa sul file di log con data e ora della scrittura su file
 * @fun                 traceOnLog
 * @param serverLog     Struttura del log
 * @param format        Formato del testo da andare a scrivere
 * @param ...           Eventuali argomenti del testo
 * @return              Se non ci sono errori ritorna (0), altrimenti ritorna (-1) [setta errno]
 */
int traceOnLog(serverLogFile *serverLog, char *format, ...) {
    /** Variabili **/
    va_list string;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char *time = NULL, save[MAX_STRING_TIME], *change = NULL;

    /** Controllo parametri **/
    if(serverLog == NULL) { errno = EINVAL; return -1; }
    if(format == NULL) { errno = EINVAL; return -1; }

    /** Prendo possesso del file e scrivo il log **/
    LOCK_LOG_FILE(serverLog->lockFile)
    time = asctime_r(tm, save);
    change = (strrchr(time, '\n'));
    *change = '\0';
    fprintf(serverLog->file, "[%s]: ", time);
    va_start(string, format);
    if(vfprintf(serverLog->file, format, string) < 0) { errno = EINVAL; return -1; }
    va_end(string);
    fflush(serverLog->file);
    UNLOCK_LOG_FILE(serverLog->lockFile)

    return 0;
}


/**
 * @brief               Distrugge la struttura del file di log e lo chiude
 * @fun                 stopServerTracing
 * @param serverLog     Struttura del file di log
 * @exception           In caso di errore [setta errno]
 */
void stopServerTracing(serverLogFile **serverLog) {
    /** Variabili **/
    int error = 0;

    /** Controllo parametri **/
    if(*serverLog == NULL) { errno = EINVAL; return; }

    /** Chiudo tutto e dealloco la memoria **/
    if(fclose((*serverLog)->file) != 0) return;
    if((error = pthread_mutex_destroy((*serverLog)->lockFile)) != 0) { errno = error; return; }
    free((*serverLog)->lockFile);
    free((*serverLog)->pathname);
    free((*serverLog));

    (*serverLog) = NULL;
}
