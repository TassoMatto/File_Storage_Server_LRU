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
 * @param access    Lock per accedere alla variabile di timeout
 */
typedef struct {
    int *stop;
    struct timespec timer;
    pthread_mutex_t *access;
} argTimer;


/**
 * @brief           Specifica il tempo massimo dopo quale fallisce il tentativo di connessione al server
 * @fun             timeout
 * @param argv      Argomenti per gestire il timeout
 * @return          NULL se tutto e' andato correttamente, altrimenti ritorna il codice dell'errore
 */
static void* timeout(void *argv) {
    /** Variabili **/
    int error = 0, *stop = NULL;
    struct timespec timer, remain;
    argTimer *converted = NULL;

    /** Conversione argomento e inizio timer **/
    converted = (argTimer *) argv;
    stop = (int *) converted->stop;
    timer = (struct timespec) converted->timer;
    pthread_mutex_t *access = converted->access;

    if(nanosleep(&timer, &remain) == -1)
        return &errno;
    if((error = pthread_mutex_lock(access)) != 0) {
        errno = error;
        return &errno;
    }
    *stop = 1;
    if((error = pthread_mutex_unlock(access)) != 0) {
        errno = error;
        return &errno;
    }

    return (void *) 0;
}


/**
 * @brief                   Calcola il path assoluto del file
 * @fun                     abs_path
 * @param pathname          Pathname del file su cui calcolare il path assoluto
 * @return                  In caso di successo ritorna il path assoluto del file;
 *                          altrimenti ritorna NULL [setta errno]
 */
char* abs_path(const char *pathname) {
    /** Variabili **/
    char *abs_p = NULL, *name = NULL, *old_cwd = NULL, *copy = NULL;

    /** Calcolo il pathname assoluto **/
    errno = 0;
    if((abs_p = (char *) calloc(MAX_PATHNAME, sizeof(char))) == NULL) {
        return NULL;
    }
    if((copy = (char *) calloc(strnlen(pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
        free(abs_p);
        return NULL;
    }
    if((old_cwd = (char *) calloc(MAX_PATHNAME, sizeof(char))) == NULL) {
        free(abs_p);
        free(copy);
        return NULL;
    }
    strncpy(copy, pathname, strnlen(pathname, MAX_PATHNAME)+1);
    if(getcwd(old_cwd, MAX_PATHNAME) == NULL) {
        free(old_cwd);
        free(abs_p);
        free(copy);
        return NULL;
    }
    name = strrchr(copy, '/');
    if(name != NULL) name[0] = '\0';
    if(chdir(copy) == -1) {
        free(old_cwd);
        free(abs_p);
        free(copy);
        return NULL;
    }
    if(getcwd(abs_p, MAX_PATHNAME) == NULL) {
        if(chdir(old_cwd)) perror("chdir");
        free(old_cwd);
        free(abs_p);
        free(copy);
        return NULL;
    }
    strncat(abs_p, "/", 2);
    strncat(abs_p, (name == NULL) ? pathname : (name+1), strnlen((name == NULL) ? pathname : (name+1), MAX_PATHNAME));
    if(chdir(old_cwd) == -1) {
        free(old_cwd);
        free(abs_p);
        free(copy);
        return NULL;
    }

    free(old_cwd);
    free(copy);
    errno = 0;
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
int readFileFromDisk(const char *pathname, void **buf, size_t *size) {
    /** Variabili **/
    FILE *readF = NULL;
    struct stat checkFile;
    size_t readBytes = -1;

    /** Controllo parametri **/
    errno = 0;
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(size == NULL) { errno = EINVAL; return -1; }

    /** Controllo che il file esista e lo leggo **/
    if(stat(pathname, &checkFile) == -1) { *size = 0; return -1; }
    if(!S_ISREG(checkFile.st_mode)) { errno = ENOENT; *size = 0; return -1; }
    if((readF = fopen(pathname, "r")) == NULL) { *size = 0; return -1; }
    if((*buf = malloc(checkFile.st_size)) == NULL) { fclose(readF); *size = 0; return -1; }
    while(!feof(readF)) {
        readBytes = fread(*buf, checkFile.st_size, 1, readF);
        if(readBytes == 0 && ferror(readF)) {
            errno = ferror(readF);
            fclose(readF);
            free(buf);
            *size = 0;
            return -1;
        }
    }
    if(fclose(readF) != 0) {
        free(*buf);
        *size = 0;
        return -1;
    }

    *size = checkFile.st_size;
    errno = 0;
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
int writeFileIntoDisk(const char *pathname, const char *dirname, void *buf, size_t size) {
    /** Variabili **/
    char *old_cwd = NULL, *name = NULL;
    FILE *writeF = NULL;
    size_t writeBytes = -1;
    struct stat checkDir;

    /** Controllo parametri **/
    errno = 0;
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
        free(old_cwd);
        return -1;
    }
    if(chdir(dirname) == -1) {
        free(old_cwd);
        return -1;
    }
    if((name = strrchr(pathname, '/')) == NULL) {
        name = (char *) pathname;
    } else name++;
    if((writeF = fopen(name, "w")) == NULL) {
        if(chdir(old_cwd) == -1) perror("chdir");
        free(old_cwd);
        return -1;
    }
    writeBytes = fwrite(buf, size, 1, writeF);
    if(writeBytes == 0 && ferror(writeF)) {
        fclose(writeF);
        if(chdir(old_cwd) == -1) perror("chdir");
        free(old_cwd);
        errno = ferror(writeF);
        return -1;
    }
    if(fclose(writeF) != 0) {
        if(chdir(old_cwd) == -1) perror("chdir");
        free(old_cwd);
        return -1;
    }
    if(chdir(old_cwd) == -1) {
        free(old_cwd);
        return -1;
    }
    free(old_cwd);

    errno = 0;
    return 0;
}


char** readNFileFromDir(const char *dirname, size_t n) {
    /** Variabili **/
    DIR *d = NULL;
    struct stat buf;
    struct dirent* dir_point = NULL;
    int quit = 0;
    size_t i = 2;
    Queue *subDir = NULL;
    char **list = NULL, **new_tmp = NULL;
    char *pathname_file = NULL, *pathname_dir = NULL;
    char *old_dirname = NULL, *thisDir = NULL, *queue_el = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(dirname == NULL) { errno = EINVAL; return NULL; }
    if(stat(dirname, &buf) == -1) { return NULL; }
    if(!S_ISDIR(buf.st_mode)) { errno = ENOENT; return NULL; }

    if((old_dirname = (char *) calloc(MAX_PATHNAME, sizeof(char))) == NULL) return NULL;
    if((pathname_dir = (char *) calloc(MAX_PATHNAME, sizeof(char))) == NULL) {
        free(old_dirname);
        return NULL;
    }
    strncpy(pathname_dir, dirname, strnlen(dirname, MAX_PATHNAME)+1);
    if(getcwd(old_dirname, MAX_PATHNAME) == NULL) {
        free(pathname_dir);
        free(old_dirname);
        return NULL;
    }
    if(n == 0) n = INT_MAX;

    while(!quit)
    {
        if(chdir(pathname_dir) == -1) {
            free(pathname_dir);
            free(old_dirname);
            return NULL;
        }
        if((d = opendir(".")) == NULL) {
            if(chdir(old_dirname) == -1) perror("chdir");
            destroyQueue(&subDir, free);
            free(pathname_dir);
            free(old_dirname);
            return NULL;
        }
        while(((errno = 0, dir_point = readdir(d)) != NULL) && (i<(n+2)))
        {
            /** Analizzo tutti i file e sottocartelle presenti ad esclusione di "." e ".." **/
            if(stat(dir_point->d_name, &buf) == -1) {
                closedir(d);
                if(chdir(old_dirname) == -1) perror("chdir");
                destroyQueue(&subDir, free);
                free(pathname_dir);
                free(old_dirname);
                return NULL;
            }
            /**
             * Se l'oggetto puntato è una sottocartella diversa da "." e ".."
             * allora la salvo per una probabile successiva sua analisi
             */
            if(S_ISDIR(buf.st_mode) && (strncmp(dir_point->d_name, ".", strnlen(dir_point->d_name, MAX_PATHNAME)) != 0) && (strncmp(dir_point->d_name, "..", strnlen(dir_point->d_name, MAX_PATHNAME)) != 0)) {
                if((queue_el = (char *) calloc(strnlen(dir_point->d_name, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
                    closedir(d);
                    destroyQueue(&subDir, free);
                    if(chdir(old_dirname) == -1) perror("chdir");
                    free(pathname_dir);
                    free(old_dirname);
                    return NULL;
                }
                strncpy(queue_el, dir_point->d_name, strnlen(dir_point->d_name, MAX_PATHNAME)+1);
                if((subDir = insertIntoQueue(subDir, queue_el, (strnlen(queue_el, MAX_PATHNAME)+1)*sizeof(char))) == NULL) {
                    free(pathname_dir);
                    free(queue_el);
                    destroyQueue(&subDir, free);
                    closedir(d);
                    if(chdir(old_dirname) == -1) perror("chdir");
                    free(old_dirname);
                    return NULL;
                }
                i++;
            } else if(S_ISREG(buf.st_mode)) { // Se l'oggetto puntato è un file allora lo salvo nella lista da inviare successivamente
                if((new_tmp = (char **) realloc(list, i*sizeof(char *))) == NULL) {
                    size_t j=0;
                    destroyQueue(&subDir, free);
                    while(list[j] != NULL)
                        free(list[j++]);
                    free(list);

                    closedir(d);
                    if(chdir(old_dirname) == -1) perror("chdir");
                    free(pathname_dir);
                    free(old_dirname);
                    return NULL;
                }
                list = new_tmp;
                list[i-1] = NULL;
                if((thisDir = (char *) calloc(strnlen(dir_point->d_name, MAX_PATHNAME) + 3, sizeof(char))) == NULL) {
                    size_t j=0;
                    destroyQueue(&subDir, free);
                    while(list[j] != NULL)
                        free(list[j++]);
                    free(list);
                    closedir(d);
                    if(chdir(old_dirname) == -1) perror("chdir");
                    free(pathname_dir);
                    free(old_dirname);
                    return NULL;
                }
                strncpy(thisDir, "./", 3);
                if((pathname_file = abs_path(strncat(thisDir, dir_point->d_name, strnlen(dir_point->d_name, MAX_PATHNAME)))) == NULL) {
                    size_t j=0;
                    destroyQueue(&subDir, free);
                    while(list[j] != NULL)
                        free(list[j++]);
                    free(list);
                    closedir(d);
                    if(chdir(old_dirname) == -1) perror("chdir");
                    free(thisDir);
                    free(pathname_dir);
                    free(old_dirname);
                    return NULL;
                }
                if((list[i-2] = (char *) calloc((strnlen(pathname_file, MAX_PATHNAME)+1), sizeof(char))) == NULL) {
                    size_t j=0;
                    destroyQueue(&subDir, free);
                    while(list[j] != NULL)
                        free(list[j++]);
                    free(list);
                    closedir(d);
                    if(chdir(old_dirname) == -1) perror("chdir");
                    free(pathname_dir);
                    free(old_dirname);
                    return NULL;
                }
                strncpy(list[i-2], pathname_file, strnlen(pathname_file, MAX_PATHNAME)+1);
                free(thisDir);
                free(pathname_file);
                i++;
            }
        }

        if(errno != 0) { //Controllo che sia uscito senza errori dal ciclo
            size_t j=i-1;
            destroyQueue(&subDir, free);
            while(list[j] != NULL)
                free(list[j++]);
            free(list);
            closedir(d);
            if(chdir(old_dirname) == -1) perror("chdir");
            free(pathname_dir);
            free(old_dirname);
            return NULL;
        }

        /** Esco che ho raggiunto il mio tetto massimo o non ho più file da visitare **/
        if((i == (n+2)) || (subDir == NULL)) {
            quit = 1;
        }
        if(closedir(d) == -1) {
            size_t j=i-1;
            destroyQueue(&subDir, free);
            while(list[j] != NULL)
                free(list[j++]);
            free(list);
            if(chdir(old_dirname) == -1) perror("chdir");
            free(pathname_dir);
            free(old_dirname);
            return NULL;
        }

        // Se non ho finito controllo le sottocartelle
        if(!quit) {
            Queue *poped = NULL;
            if((poped = deleteFirstElement(&subDir)) == NULL) {
                size_t j=i-1;
                destroyQueue(&subDir, free);
                while(list[j] != NULL)
                    free(list[j++]);
                free(list);
                if(chdir(old_dirname) == -1) perror("chdir");
                free(pathname_dir);
                free(old_dirname);
                return NULL;
            }
            strncpy(pathname_dir, (char *) poped->data, poped->size);
            free(poped->data);
            free(poped);
        }
    }

    destroyQueue(&subDir, free);
    if(chdir(old_dirname) == -1) {
        size_t j=0;
        destroyQueue(&subDir, free);
        while(list[j] != NULL)
            free(list[j++]);
        free(list);
        free(old_dirname);
        return NULL;
    }
    free(pathname_dir);
    free(old_dirname);
    errno = 0;
    return list;
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
    int error = 0, stop = 0, connectRes = -1, *status = NULL;
    pthread_t *timer = NULL;
    struct sockaddr_un sock_addr;
    struct timespec repeat;
    argTimer arg;

    /** Controllo parametri **/
    errno = 0;
    if(sockname == NULL) { errno = EINVAL; return -1; }
    if(msec <= 0) { errno = EINVAL; return -1; }

    /** Tentativo di connessione al server **/
    if((fd_server = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return -1;
    }
    strncpy(sock_addr.sun_path, sockname, strnlen(sockname, MAX_PATHNAME)+1);
    sock_addr.sun_family = AF_UNIX;
    arg.stop = &stop;
    arg.timer = abstime;
    arg.access = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if(arg.access == NULL) {
        return -1;
    }
    if((error = pthread_mutex_init(arg.access, NULL)) != 0) {
        free(arg.access);
        return -1;
    }
    memset(&repeat, 0, sizeof(struct timespec)), repeat.tv_nsec = msec;
    if((timer = (pthread_t *) malloc(sizeof(pthread_t))) == NULL) {
        pthread_mutex_destroy(arg.access);
        free(arg.access);
        return -1;
    }
    if((error = pthread_create(timer, NULL, timeout, (void *) &arg)) != 0) {
        free(timer);
        pthread_mutex_destroy(arg.access);
        free(arg.access);
        errno = error;
        return -1;
    }
    if((error = pthread_mutex_lock(arg.access)) != 0) {
        pthread_join(*timer, NULL);
        free(timer);
        pthread_mutex_destroy(arg.access);
        free(arg.access);
        errno = error;
        return -1;
    }
    while((!stop) && ((connectRes = connect(fd_server, (struct sockaddr *) &sock_addr, sizeof(sock_addr))) == -1)) {
        if((error = pthread_mutex_unlock(arg.access)) != 0) {
            pthread_join(*timer, NULL);
            free(timer);
            pthread_mutex_destroy(arg.access);
            free(arg.access);
            errno = error;
            return -1;
        }
        if(nanosleep(&repeat, NULL) == -1) {
            pthread_join(*timer, NULL);
            free(timer);
            pthread_mutex_destroy(arg.access);
            free(arg.access);
            return -1;
        }
        if((error = pthread_mutex_lock(arg.access)) != 0) {
            pthread_join(*timer, NULL);
            free(timer);
            pthread_mutex_destroy(arg.access);
            free(arg.access);
            errno = error;
            return -1;
        }
    }
    if((error = pthread_mutex_unlock(arg.access)) != 0) {
        pthread_join(*timer, NULL);
        free(timer);
        pthread_mutex_destroy(arg.access);
        free(arg.access);
        errno = error;
        return -1;
    }

    /** Termino il tentativo e riporto il risultato al client **/
    if(((error = pthread_join(*timer, (void **) &status)) != 0) || ((status != NULL) && (*status != 0) && (*status != EINTR))) {
        if(fd_server != -1) close(fd_server);
        if(status == NULL) errno = error;
        else errno = *status;
        free(timer);
        pthread_mutex_destroy(arg.access);
        free(arg.access);
        free(status);
        return -1;
    }
    free(timer);
    if((error = pthread_mutex_destroy(arg.access)) != 0) {
        errno = error;
        return -1;
    }
    free(arg.access);
    if(connectRes == -1) { errno = ETIMEDOUT; return -1; }

    strncpy(socketname, sockname, strnlen(sockname, MAX_PATHNAME));
    errno = 0;
    return 0;
}


/**
 * @brief               Termina la connessione con il server
 * @fun                 closeConnection
 * @param sockname      Socket da chiudere
 * @return              Ritorna (0) in caso di successo; (-1) altrimenti [setta errno]
 */
int closeConnection(const char *sockname) {
    /** Controllo parametri **/
    errno = 0;
    if(sockname == NULL) { errno = EINVAL; return -1; }

    /** Chiusura della connessione **/
    if(strncmp(sockname, socketname, (size_t) fmax((double) MAX_PATHNAME, (double) strnlen(sockname, MAX_PATHNAME))) == 0) {
        if(close(fd_server) == -1) { return -1; }
        memset(socketname, 0, strnlen(sockname, MAX_PATHNAME));
        errno = 0;
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
    errno = 0;
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
    if(*result == 0 || *result == 1) {
        free(result);
        errno = 0;
        return 0;
    }

    errno = *result;
    free(result);
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

    /** Controllo parametri **/
    errno = 0;
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
    if((*existFile) != 0) {
        errno = *existFile;
        free(existFile);
        return -1;
    }

    /** In caso affermativo di risposta ricevo il contenuto del file **/
    if(receiveMSG(fd_server, buf, size) <= 0) {
        free(existFile);
        return -1;
    }

    free(existFile);
    errno = 0;
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

    /** Invio richiesta **/
    errno = 0;
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
        free(res);
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &res, NULL) <= 0) {
        free(res);
        return -1;
    }
    while(*res != 0) {
        numReads++;
        if(receiveMSG(fd_server, (void **) &pathname, NULL) <= 0) {
            if(buf != NULL) free(buf);
            if(pathname != NULL) free(pathname);
            if(res != NULL) free(res);
            return -1;
        }
        if(receiveMSG(fd_server, (void **) &buf, &size) <= 0) {
            if(buf != NULL) free(buf);
            if(pathname != NULL) free(pathname);
            if(res != NULL) free(res);
            return -1;
        }
        if((dirname != NULL) && (writeFileIntoDisk(pathname, dirname, buf, size) == -1)) {
            if(buf != NULL) free(buf);
            if(pathname != NULL) free(pathname);
            if(res != NULL) free(res);
            return -1;
        }
        if(receiveMSG(fd_server, (void **) &res, NULL) <= 0) {
            if(buf != NULL) free(buf);
            if(pathname != NULL) free(pathname);
            if(res != NULL) free(res);
            return -1;
        }
    }

    if(buf != NULL) free(buf);
    if(pathname != NULL) free(pathname);
    if(res != NULL) free(res);
    errno = 0;
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
    errno = 0;
    if(pathname == NULL) { errno = EINVAL; return -1; }

    /** Invio la richiesta al server **/
    if(sendMSG(fd_server, "writeFile", 10*sizeof(char)) <= 0) {
        return -1;
    }

    /** Mando il pathname del file da aggiungere **/
    if(sendMSG(fd_server, (void *) pathname, sizeof(char)*(strnlen(pathname, MAX_PATHNAME)+1)) <= 0) {
        return -1;
    }
    if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
        return -1;
    }
    while(*result != 0) {
        if(receiveMSG(fd_server, (void **) &fileToWrite, NULL) <= 0) {
            if(buf != NULL) free(buf);
            if(fileToWrite != NULL) free(fileToWrite);
            if(result != NULL) free(result);
            return -1;
        }
        if(receiveMSG(fd_server, &buf, &dimBuf) <= 0) {
            if(buf != NULL) free(buf);
            if(fileToWrite != NULL) free(fileToWrite);
            if(result != NULL) free(result);
            return -1;
        }
        if((dirname != NULL) && (dimBuf != 0) && (writeFileIntoDisk(fileToWrite, dirname, buf, dimBuf) == -1)) {
            if(buf != NULL) free(buf);
            if(fileToWrite != NULL) free(fileToWrite);
            if(result != NULL) free(result);
            return -1;
        }
        if(buf != NULL) free(buf);
        if(fileToWrite != NULL) free(fileToWrite);
        if(result != NULL) free(result);
        if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
            if(buf != NULL) free(buf);
            if(fileToWrite != NULL) free(fileToWrite);
            if(result != NULL) free(result);
            return -1;
        }
    }
    if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
        free(buf);
        free(fileToWrite);
        free(result);
        return -1;
    }
    if(*result != 0) {
        errno = *result;
        if(buf != NULL) free(buf);
        if(fileToWrite != NULL) free(fileToWrite);
        if(result != NULL) free(result);
        return -1;
    }

    if(buf != NULL) free(buf);
    if(fileToWrite != NULL) free(fileToWrite);
    if(result != NULL) free(result);
    errno = 0;
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
    void *bufKick = NULL;
    int *result = NULL, res = 0;
    size_t dimBuf;

    /** Controllo parametri **/
    errno = 0;
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
            if(bufKick != NULL) free(bufKick);
            if(fileToWrite != NULL) free(fileToWrite);
            if(result != NULL) free(result);
            return -1;
        }
        if(receiveMSG(fd_server, &bufKick, &dimBuf) <= 0) {
            if(bufKick != NULL) free(bufKick);
            if(fileToWrite != NULL) free(fileToWrite);
            if(result != NULL) free(result);
            return -1;
        }
        if((dirname != NULL) && (dimBuf != 0) && (writeFileIntoDisk(fileToWrite, dirname, bufKick, dimBuf) == -1)) {
            if(bufKick != NULL) free(bufKick);
            if(fileToWrite != NULL) free(fileToWrite);
            if(result != NULL) free(result);
            return -1;
        }
        if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
            if(bufKick != NULL) free(bufKick), bufKick = NULL;
            if(fileToWrite != NULL) free(fileToWrite);
            if(result != NULL) free(result);
            return -1;
        }
    }
    printf("JOIOEFWFIOJWEJWEOJWEFEWIOJFEIOFEWJFEWFEWJFWEIOFEWJFEEWFWEJ\n\n\n\n\n\n\n");

    /** Ricevo la risposta e valuto l'esito **/
    if(receiveMSG(fd_server, (void **) &result, NULL) <= 0) {
        if(bufKick != NULL) free(bufKick);
        if(fileToWrite != NULL) free(fileToWrite);
        if(result != NULL) free(result);
        return -1;
    }
    printf("99999999999999999999999999999999999999999999999999999999999999999999999\n\n\n\n\n\n\n");

    res = *result;
    if(bufKick != NULL) free(bufKick), bufKick = NULL;
    if(fileToWrite != NULL) free(fileToWrite);
    if(result != NULL) free(result);
    errno = 0;
    return res;
}


/**
 * @brief               Effettua la lock di 'pathname' nel server
 * @fun                 lockFile
 * @param pathname      Pathname del file da lockare
 * @return              Ritorna 0 in caso di successo; -1 in caso di errori
 *                      e setta errno
 */
int lockFile(const char *pathname) {
    /** Variabili **/
    int *res = NULL;

    /** Controllo parametri **/
    errno = 0;
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
    if(*res == 0) {
        free(res);
        errno = 0;
        return 0;
    }

    errno = *res;
    free(res);
    return -1;
}


/**
 * @brief               Effettua la unlock di 'pathname' nel server
 * @fun                 unlockFile
 * @param pathname      Pathname del file da unlockare
 * @return              Ritorna 0 in caso di successo; -1 in caso di errori
 *                      e setta errno
 */
int unlockFile(const char *pathname) {
    /** Variabili **/
    int *res = NULL;

    /** Controllo parametri **/
    errno = 0;
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
    if(*res == 0) {
        free(res);
        errno = 0;
        return 0;
    }

    errno = *res;
    free(res);
    return -1;
}


/**
 * @brief               Chiude un file nel server
 * @fun                 closeFile
 * @param pathname      Pathname del file da chiudere
 * @return              In caso di successo ritorna 0; altrimenti
 *                      ritorna -1 e setta errno
 */
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
    }

    free(res);
    errno = 0;
    return 0;
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
    errno = 0;
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
        free(res);
        errno = 0;
        return 0;
    }

    free(res);
    errno = *res;
    return -1;
}