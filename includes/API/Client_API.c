/**
 * @project             FILE_STORAGE_SERVER
 * @brief               API per la connessione al server da parte del client
 * @author              Simone Tassotti
 * @date                07/01/2022
 */


#include "Client_API.h"


/** Variabili Globali **/
static int fd_server = -1;
char socketname[MAX_PATHNAME];


/**
 * @brief           Struttura per la gestione del timer
 * @struct          argTimer
 * @param stop      Indica lo stop dei tentativi di connessione
 * @param timer     Tempo massimo dei tentativi
 */
typedef struct {
    int *stop;
    struct timespec timer;
} argTimer;


/**
 * @brief           Specifica il tempo massimo dopo quale fallisce il tentativo di connessione al server
 * @fun             timeout
 * @param argv      Argomenti per gestire il timeout
 * @return          NULL se tutto e' andato correttamente, altrimenti ritorna il codice dell'errore
 */
static void* timeout(void *argv) {
    /** Variabili **/
    int *stop = NULL;
    struct timespec timer, remain;
    argTimer *converted = NULL;

    /** Conversione argomento e inizio timer **/
    converted = (argTimer *) argv;
    stop = (int *) converted->stop;
    timer = (struct timespec) converted->timer;
    if(nanosleep(&timer, &remain) == -1)
        return &errno;
    *stop = 1;

    return (void *) 0;
}


/**
 * @brief                   Tenta la connessione al server tramite la socket specificata
 * @fun                     openConnection
 * @param sockname          Nome della socket su cui connettersi
 * @param msec              Intervallo tra un tentativo e un altro di connessione
 * @param abstime           Tempo massimo oltre il quale scade il tentativo di connessione
 * @return                  (0) in caso di successo; (-1) altrimenti [setta errno]
 */
int openConnection(const char *sockname, int msec, const struct timespec abstime) {
    /** Variabili **/
    int error = 0, stop = 0, *status = NULL, connectRes = -1;
    pthread_t *timer = NULL;
    struct sockaddr_un sock_addr;
    struct timespec repeat;
    argTimer arg;

    /** Controllo parametri **/
    if(sockname == NULL) { errno = EINVAL; return -1; }
    if(msec <= 0) { errno = EINVAL; return -1; }

    /** Tentativo di connessione al server **/
    if((status = (int *) malloc(sizeof(int))) == NULL) return -1;
    if((fd_server = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) return -1;
    strncpy(sock_addr.sun_path, sockname, strlen(sockname)+1);
    sock_addr.sun_family = AF_UNIX;
    arg.stop = &stop;
    arg.timer = abstime;
    memset(&repeat, 0, sizeof(struct timespec)), repeat.tv_nsec = msec;
    if((timer = (pthread_t *) malloc(sizeof(pthread_t))) == NULL) return -1;
    if((error = pthread_create(timer, NULL, timeout, (void *) &arg)) != 0) {
        errno = error;
        return -1;
    }
    while((!stop) && ((connectRes = connect(fd_server, (struct sockaddr *) &sock_addr, sizeof(sock_addr))) == -1)) {
        nanosleep(&repeat, NULL);
    }

    /** Termino il tentativo e riporto il risultato al client **/
    if(((error = pthread_join(*timer, (void **) &status)) != 0) || ((status != NULL) && (*status != 0))) {
        if(fd_server != -1) close(fd_server);
        if(status == NULL) errno = error;
        else errno = *status;
        free(timer);
        return -1;
    }
    free(timer);
    if(connectRes == -1) { errno = ETIMEDOUT; return -1; }
    else { strncpy(socketname, sockname, strnlen(sockname, MAX_PATHNAME)); return 0; }
}


/**
 * @brief               Termina la connessione con il server
 * @fun                 closeConnection
 * @param sockname      Socket da chiudere
 * @return              Ritorna (0) in caso di successo; (-1) altrimenti [setta errno]
 */
int closeConnection(const char *sockname) {
    /** Controllo parametri **/
    if(sockname == NULL) { errno = EINVAL; return -1; }

    /** Chiusura della connessione **/
    if(strncmp(sockname, socketname, (size_t) fmax((double) MAX_PATHNAME, (double) strnlen(sockname, MAX_PATHNAME))) == 0) {
        if(close(fd_server) == -1) { return -1; }
        memset(socketname, 0, (size_t) fmax((double) MAX_PATHNAME, (double) strnlen(sockname, MAX_PATHNAME)));
        return 0;
    }

    /** Non ho trovato la socket specificata aperta **/
    errno = ENOENT;
    return -1;
}


/**
 * @brief                   Apre/Crea un file nel server
 * @fun                     openFile
 * @param pathname          Pathname del file da aprire/creare
 * @param flags             Flags che specificano le modalita' di apertura del file
 * @return                  (0) in caso di successo; (-1) altrimenti [setta errno]
 */
int openFile(const char *pathname, int flags) {
    /** Variabili **/
    int *result = NULL;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(flags < 0) { errno = EINVAL; return -1; }

    /** Invio richiesta al server e dei dati che richiede **/
    if(sendMSG(fd_server, "openFile", 9*sizeof(char)) == -1) {
        return -1;
    }
    if(sendMSG(fd_server, (void *) pathname, (strnlen(pathname, MAX_PATHNAME)+1)*sizeof(char)) == -1) {
        return -1;
    }
    if(sendMSG(fd_server, (void *) &flags, sizeof(int)) == -1) {
        return -1;
    }

    /** APK del server per valutare l'esito della richiesta **/
    if(receiveMSG(fd_server, (void **) &result, NULL) == -1) {
        return -1;
    }
    if(*result == 0)
    {
        free(result);
        return 0;
    }

    printf("Qui soiiii");
    errno = *result;
    free(result);
    return -1;
}


/**
 * @b
 * @param pathname
 * @param buf
 * @param size
 * @return
 */
int readFile(const char *pathname, void **buf, size_t *size) {
    /** Variabili **/
    int *existFile = NULL;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(size == NULL) { errno = EINVAL; return -1; }

    /** Mando la richiesta al server **/
    if(sendMSG(fd_server, "readFile", 9*sizeof(char)) <= 0) {
        return -1;
    }

    /** Mando il pathname e ricevo risposta **/
    if(sendMSG(fd_server, (void *) pathname, (strnlen(pathname, MAX_PATHNAME)+1)*sizeof(char)) <= 0) {
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &existFile, NULL) <= 0) {
        return -1;
    }
    if((*existFile)) {
        errno = *existFile;
        return -1;
    }

    /** In caso affermativo di risposta ricevo il contenuto del file **/
    if(receiveMSG(fd_server, buf, size) <= 0) {
        return -1;
    }
    return 0;
}