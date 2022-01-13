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
 * @brief                   Calcola il path assoluto del file
 * @fun                     abs_path
 * @param pathname          Pathname del file su cui calcolare il path assoluto
 * @return                  In caso di successo ritorna il path assoluto del file;
 *                          altrimenti ritorna NULL [setta errno]
 */
static char* abs_path(const char *pathname) {
    /** Variabili **/
    char *abs_p = NULL, *name = NULL, *old_cwd = NULL, *copy = NULL;

    /** Controllo variabili **/
    if(pathname == NULL) { errno = EINVAL; return NULL; }

    /** Calcolo il pathname assoluto **/
    if((abs_p = (char *) calloc(MAX_PATHNAME, sizeof(char))) == NULL) {
        return NULL;
    }
    if((copy = (char *) calloc(strnlen(pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
        return NULL;
    }
    if((old_cwd = (char *) calloc(MAX_PATHNAME, sizeof(char))) == NULL) {
        return NULL;
    }
    strncpy(copy, pathname, strnlen(pathname, MAX_PATHNAME)+1);
    if(getcwd(old_cwd, MAX_PATHNAME) == NULL) {
        free(abs_p);
        return NULL;
    }
    name = strrchr(copy, '/');
    if(name != NULL) name[0] = '\0';
    if(chdir(copy) == -1) {
        return NULL;
    }
    if(getcwd(abs_p, MAX_PATHNAME) == NULL) {
        free(abs_p);
        return NULL;
    }
    strncat(abs_p, "/", 2);
    strncat(abs_p, (name == NULL) ? pathname : (name+1), strnlen((name == NULL) ? pathname : (name+1), MAX_PATHNAME));
    if(chdir(old_cwd) == -1) {
        return NULL;
    }

    free(old_cwd);
    free(copy);
    return abs_p;
}


/**
 * @brief                   Legge il file puntato da pathname dal disco
 * @fun                     readFileFromDisk
 * @param pathname          Pathname del file da leggere
 * @param buf               Buffer del file da leggere
 * @param size              Dimensione del file da leggere
 * @return                  In caso di successo ritorna (0); (-1) altrimenti [setta errno]
 */
static int readFileFromDisk(const char *pathname, void **buf, size_t *size) {
    /** Variabili **/
    FILE *readF = NULL;
    struct stat checkFile;
    size_t readBytes = -1;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(size == NULL) { errno = EINVAL; return -1; }

    /** Controllo che il file esista e lo leggo **/
    if(stat(pathname, &checkFile) == -1) { return -1; }
    if(!S_ISREG(checkFile.st_mode)) { errno = ENOENT; return -1; }
    if((readF = fopen(pathname, "r")) == NULL) { return -1; }
    if((*buf = malloc(checkFile.st_size)) == NULL) { return -1; }
    while(!feof(readF)) {
        readBytes = fread(*buf, checkFile.st_size, 1, readF);
        if(readBytes == 0 && ferror(readF)) {
            printf("NOOO");
            errno = ferror(readF);
            return -1;
        }
    }
    if(fclose(readF) != 0) {
        free(*buf);
        return -1;
    }

    *size = checkFile.st_size;
    return 0;
}


/**
 * @brief                   Scrive un file con contenuto buf e dimensione size
 * @fun                     writeFileIntoDisk
 * @param pathname          Pathname del file da scrive
 * @param buf               Buffer da scrivere
 * @param size              Dimensione del buffer
 * @return                  (0) in caso di successo; (-1) altriment
 */
static int writeFileIntoDisk(const char *pathname, const char *dirname, void *buf, size_t size) {
    /** Variabili **/
    char *old_cwd = NULL, *name = NULL;
    FILE *writeF = NULL;
    size_t writeBytes = -1;
    struct stat checkDir;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(size <= 0) { errno = EINVAL; return -1; }
    if(dirname == NULL) { errno = EINVAL; return -1; }
    if(stat(dirname, &checkDir) == -1) { return -1; }
    if(!S_ISDIR(checkDir.st_mode)) { errno = ENOENT; return -1; }

    /** Scrivo il file sul disco **/
    if((old_cwd = (char *) calloc(MAX_PATHNAME, sizeof(char))) == NULL) {
        return -1;
    }
    if(getcwd(old_cwd, MAX_PATHNAME) == NULL) {
        return -1;
    }
    if(chdir(dirname) == -1) { free(old_cwd); return -1; }
    if((name = strrchr(pathname, '/')) == NULL) {
        name = (char *) pathname;
    } else name++;
    if((writeF = fopen(name, "w")) == NULL) {
        return -1;
    }
    writeBytes = fwrite(buf, size, 1, writeF);
    if(writeBytes == 0 && ferror(writeF)) {
        errno = ferror(writeF);
        return -1;
    }
    if(fclose(writeF) != 0) {
        return -1;
    }
    if(chdir(old_cwd) == -1) {
        return -1;
    }
    free(old_cwd);

    return 0;
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
    strncpy(sock_addr.sun_path, sockname, strnlen(sockname, MAX_PATHNAME)+1);
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
    struct stat checkFile;
    char *abs_pathname = NULL;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(flags < 0) { errno = EINVAL; return -1; }
    if(stat(pathname, &checkFile) == -1) { return -1; }
    if(!S_ISREG(checkFile.st_mode)) { errno = ENOENT; return -1; }

    /** Invio richiesta al server e dei dati che richiede **/
    if((abs_pathname = abs_path(pathname)) == NULL) {
        return -1;
    }
    if(sendMSG(fd_server, "openFile", 9*sizeof(char)) == -1) {
        return -1;
    }
    if(sendMSG(fd_server, (void *) abs_pathname, (strnlen(abs_pathname, MAX_PATHNAME)+1)*sizeof(char)) == -1) {
        return -1;
    }
    if(sendMSG(fd_server, (void *) &flags, sizeof(int)) == -1) {
        return -1;
    }

    /** APK del server per valutare l'esito della richiesta **/
    if(receiveMSG(fd_server, (void **) &result, NULL) == -1) {
        return -1;
    }
    if(*result == 0 || *result == 1)
    {
        free(result);
        return 0;
    }

    errno = *result;
    free(result);
    free(abs_pathname);
    return -1;
}


/**
 * @brief                   Chiede la lettura di un file dal server
 * @fun                     readFile
 * @param pathname          Pathname del file da leggere
 * @param buf               Buffer del file da leggere
 * @param size              Dimensione del file
 * @return                  Ritorna (0) in caso di successo; (-1) altrimenti [setta errno]
 */
int readFile(const char *pathname, void **buf, size_t *size) {
    /** Variabili **/
    int *existFile = NULL;
    char *abs_pathname = NULL;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(size == NULL) { errno = EINVAL; return -1; }

    /** Mando la richiesta al server **/
    if(sendMSG(fd_server, "readFile", 9*sizeof(char)) <= 0) {
        return -1;
    }

    /** Mando il pathname e ricevo risposta **/
    if((abs_pathname = abs_path(pathname)) == NULL) {
        return -1;
    }
    if(sendMSG(fd_server, (void *) abs_pathname, (strnlen(abs_pathname, MAX_PATHNAME)+1)*sizeof(char)) <= 0) {
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &existFile, NULL) <= 0) {
        return -1;
    }
    if((*existFile) != 0) {
        errno = *existFile;
        return -1;
    }

    /** In caso affermativo di risposta ricevo il contenuto del file **/
    if(receiveMSG(fd_server, buf, size) <= 0) {
        return -1;
    }

    free(abs_pathname);
    return 0;
}


/**
 * @brief               Legge N file random e li scrive nella dirname
 * @fun                 readNFiles
 * @param N             Numero di file che il client ha intenzione di leggere
 * @param dirname       Cartella in cui salvare i file
 * @return              Ritorna il numero effettivo di file letti dal server;
 *                      in caso di errore ritorna (-1) [setta errno]
 */
int readNFiles(int N, const char *dirname) {
    /** Variabili **/
    int *res = NULL, numReads = 0;
    void *buf = NULL;
    char *pathname = NULL;
    size_t size = -1;

    /** Controllo parametri **/
    if(dirname == NULL) { errno = EINVAL; return -1; }

    /** Invio richiesta **/
    if(sendMSG(fd_server, "readNFiles", 11* sizeof(char)) <= 0) {
        return -1;
    }

    /** Mando il numero di file che voglio leggere **/
    if(sendMSG(fd_server, (void *) &N, sizeof(int)) <= 0) {
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &res, NULL) <= 0) {
        return -1;
    }
    if(*res != 0) {
        errno = *res;
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &res, NULL) <= 0) {
        return -1;
    }
    printf("--> %d\n", *res);
    while(*res != 0) {
        printf("AOOOO");
        numReads++;
        if(receiveMSG(fd_server, (void **) &pathname, NULL) <= 0) {
            return -1;
        }
        if(receiveMSG(fd_server, (void **) &buf, &size) <= 0) {
            return -1;
        }
        printf("%s\n", pathname);
        if(writeFileIntoDisk(pathname, dirname, buf, size) == -1) {
            printf("Eccolo\n");
            return -1;
        }
        printf("qaaa");
        if(receiveMSG(fd_server, (void **) &res, NULL) <= 0) {
            return -1;
        };
    }

    return numReads;
}


/**
 * brief                Scrive tutto il file puntato da pathname nel server
 * @fun                 writeFile
 * @param pathname      Pathname del file da scrivere
 * @param dirname       Directoru dove verranno salvati gli eventuali file cacciati dal server
 * @return              (0) in caso di successo; (-1) altrimenti
 */
int writeFile(const char *pathname, const char *dirname) {
    /** Variabili **/
    int *result = NULL;
    char *fileToWrite = NULL /**abs_pathname = NULL*/;
    void *buf = NULL;
    size_t dimBuf = -1;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return -1; }

    /** Invio la richiesta al server **/
    if(sendMSG(fd_server, "writeFile", 10*sizeof(char)) <= 0) {
        return -1;
    }

    /** Mando il pathname del file da aggiungere **/
    // Prima mando il pathname, controllo che l'aggiunta del file vuoto non
    // abbia causato memorymiss; infine controllo la correttezza dell'aggiunta
    if(sendMSG(fd_server, (void *) pathname, sizeof(char)*(strnlen(pathname, MAX_PATHNAME)+1)) <= 0) {
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
        return -1;
    }
    printf("rwsult: %d\n", *result);
    while(*result != 0) {
        printf("Ricevo\n");
        if(receiveMSG(fd_server, (void **) &fileToWrite, NULL) <= 0) {
            return -1;
        }
        if(receiveMSG(fd_server, &buf, &dimBuf) <= 0) {
            return -1;
        }
        if((dirname != NULL) && (writeFileIntoDisk(fileToWrite, dirname, buf, dimBuf) == -1)) {
            return -1;
        }
        if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
            return -1;
        }
    }
    printf("rwsult2222: %d\n", *result);
    if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
        return -1;
    }
    if(*result != 0) {
        errno = *result;
        return -1;
    }
    printf("33332: %d\n", *result);
    /** Se tutto ok scrivo il contenuto del file **/
    if(readFileFromDisk(pathname, &buf, &dimBuf) == -1) {
        return -1;
    }
    printf("Mando\n");
    if(sendMSG(fd_server, (void *) buf, dimBuf) <= 0) {
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
        return -1;
    }
    printf("aaa");
    while(*result != 0) {
        if(receiveMSG(fd_server, (void **) &fileToWrite, NULL) <= 0) {
            return -1;
        }
        if(receiveMSG(fd_server, &buf, &dimBuf) <= 0) {
            return -1;
        }
        if((dirname != NULL) && (writeFileIntoDisk(fileToWrite, dirname, buf, dimBuf) == -1)) {
            return -1;
        }
        if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
            return -1;
        }
    }

    if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
        return -1;
    }
    if(*result != 0) {
        errno = *result;
        return -1;
    }

    return 0;
}


/**
 * @brief               Aggiungo il contenuto di buf, di dimensione size nel server
 * @fun                 appendToFile
 * @param pathname      Pathname del file da aggiornare
 * @param buf           Buffer da aggiungere
 * @param size          Dimensione del buffer
 * @param dirname       Cartella dove salvo i file espulsi
 * @return              Ritorna (0) in caso di successo; (-1) altrimenti [setta errno]
 */
int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname) {
    /** Variabili **/
    char *fileToWrite = NULL;
    int *result = NULL;
    size_t dimBuf;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(buf == NULL) { errno = EINVAL; return -1; }
    if(size <= 0) { errno = EINVAL; return -1; }

    /** Invio richiesta al server **/
    if(sendMSG(fd_server, "appendToFile", 13) <= 0) {
        return -1;
    }

    /** Invio il pathname del file e il contenuto da aggiungere **/
    if(sendMSG(fd_server, (void *) pathname, strnlen(pathname, MAX_PATHNAME)+1) <= 0) {
        return -1;
    }
    if(sendMSG(fd_server, (void *) buf, size) <= 0) {
        return -1;
    }

    /** Controllo la presenza di eventuali memorymiss **/
    if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
        return -1;
    }
    while(*result != 0) {
        if(receiveMSG(fd_server, (void **) &fileToWrite, NULL) <= 0) {
            return -1;
        }
        if(receiveMSG(fd_server, &buf, &dimBuf) <= 0) {
            return -1;
        }
        if((dirname != NULL) && (writeFileIntoDisk(fileToWrite, dirname, buf, dimBuf) == -1)) {
            return -1;
        }
        if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
            return -1;
        }
    }

    /** Ricevo la risposta e valuto l'esito **/
    if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
        return -1;
    }

    return *result;
}


/**
 * @brief
 * @fun
 * @param pathname
 * @return
 */
int lockFile(const char *pathname) {
    /** Variabili **/
    int *res = NULL;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return -1; }

    /** Invio richiesta al server **/
    if(sendMSG(fd_server, "lockFile", sizeof(char)*9) <= 0) {
        return -1;
    }

    /** Mando la candidatura di lock al server per quel file **/
    if(sendMSG(fd_server, (void *) pathname, sizeof(char)*(strnlen(pathname, MAX_PATHNAME)+1)) <= 0) {
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &res, NULL) <= 0) {
        return -1;
    }
    if(*res == 0)
        return 0;

    errno = *res;
    return -1;
}


int unlockFile(const char *pathname) {
    /** Variabili **/
    int *res = NULL;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return -1; }

    /** Invio richiesta al server **/
    if(sendMSG(fd_server, (void *) "unlockFile", sizeof(char)*11) <= 0) {
        return -1;
    }

    /** Effettuo unlock **/
    if(sendMSG(fd_server, (void *) pathname, sizeof(char)*(strnlen(pathname, MAX_PATHNAME)+1)) <= 0) {
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &res, NULL) <= 0) {
        return -1;
    }
    if(*res == 0)
        return 0;

    errno = *res;
    return -1;
}


int closeFile(const char *pathname) {
    /** Variabili **/
    int *res = NULL;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return -1; }

    /** Invio richiesta al server **/
    if(sendMSG(fd_server, "closeFile", sizeof(char)*10) <= 0) {
        return -1;
    }
    if(sendMSG(fd_server, (void *) pathname, sizeof(char)*(strnlen(pathname, MAX_PATHNAME)+1)) <= 0) {
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &res, NULL) <= 0) {
        return -1;
    }

    /** Ritorno il risultato dell'operazione **/
    if(*res != 0) {
        errno = *res;
        free(res);
        return -1;
    } else {
        free(res);
        return 0;
    }
}


/**
 * @brief                   Rimuovo un file dal server
 * @fun                     removeFile
 * @param pathname          Pathname del file da rimuovere
 * @return                  (0) in caso di successo; (-1) altrimenti [setta errno]
 */
int removeFile(const char *pathname) {
    /** Variabili **/
    int *res = NULL;

    /** Controllo variabili **/
    if(pathname == NULL) { errno = EINVAL; return -1; }

    /** Invio richiesta al server **/
    if(sendMSG(fd_server, "removeFile", sizeof(char)*11) <= 0) {
        return -1;
    }

    /** Invio pathname e attesa della risposta del server **/
    if(sendMSG(fd_server, (void *) pathname, (strnlen(pathname, MAX_PATHNAME)+1)*sizeof(char)) <= 0) {
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &res, NULL) <= 0) {
        return -1;
    }
    if(*res == 0) {
        return 0;
    } else {
        errno = *res;
        return -1;
    }
}