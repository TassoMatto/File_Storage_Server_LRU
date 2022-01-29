/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Client per la connessione al FileStorageServer
 * @author              Simone Tassotti
 * @date                16/01/2022
 * @finish
 */


#define _POSIX_C_SOURCE 2001112L


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <utils.h>
#include <Client_API.h>


/**
 * @brief               Stampa (se abilitata) sullo stdout delle
 *                      richieste effettuate al server
 * @macro               TRACE_ON_DISPLAY
 * @param TXT           Testo da stampare ed eventuali argomenti
 */
#define TRACE_ON_DISPLAY(TXT, ...)                                \
    if(option->p) {                                               \
        printf(TXT, ##__VA_ARGS__);                               \
        fflush(stdout);                                           \
    }


/**
 * @brief               Sposta gli argomenti della linea di comando
 *                      in ordine di importanza
 * @macro               SHIFT
 * @param START         Argomento di partenza
 * @param ARGV          Argomenti da ordinare
 * @param ARGC          Numero di argomenti da ordinare
 * @param OFFSET        Posizione dell'argomento da ordinare
 */
#define SHIFT(START, ARGV, ARGC, OFFSET)                                                    \
    do {                                                                                    \
        size_t j = (START);                                                                 \
        char tmp[MAX_BUFFER_LEN];                                                           \
        if(((START)+1 < (ARGC)) && (strchr((ARGV)[(START)+1], '-') == NULL)) j++;           \
        strncpy(tmp, (ARGV)[(j)], strnlen((ARGV)[j], MAX_BUFFER_LEN)+1);                    \
        while(j > OFFSET)                                                                   \
        {                                                                                   \
            strncpy((ARGV)[j],  (ARGV)[j-1], strnlen((ARGV)[j-1], MAX_BUFFER_LEN)+1);       \
            (j)--;                                                                          \
        }                                                                                   \
        strncpy((ARGV)[(j)], tmp, strnlen(tmp, MAX_BUFFER_LEN)+1);                          \
    }while(0);


/**
 * @brief               Traduce gli argomenti di tipo file1[,file2,...]
 * @macro               DELETE_POINT
 */
#define DELETE_POINT                                                                                        \
    do {                                                                                                    \
        numFile = 0;                                                                                        \
        files = NULL;                                                                                       \
        tok = strtok_r(optarg, ",", &save);                                                                 \
        while(tok != NULL) {                                                                                \
            numFile++;                                                                                      \
            if((files = (char **) realloc(files, sizeof(char *)*(numFile + 1))) == NULL) {                  \
                DESTROY_ALL;                                                                                \
                exit(errno);                                                                                \
            }                                                                                               \
            if((files[numFile-1] = (char *) calloc(strnlen(tok, MAX_PATHNAME)+1, sizeof(char))) == NULL) {  \
                DESTROY_ALL;                                                                                \
                exit(errno);                                                                                \
            }                                                                                               \
            strncpy(files[numFile-1], tok, strnlen(tok, MAX_PATHNAME)+1);                                   \
            files[numFile] = NULL;                                                                          \
            tok = strtok_r(NULL, ",", &save);                                                               \
        }                                                                                                   \
    } while(0)


/**
 * @brief               Applica un ritardo tra l'esecuzione di una
 *                      richiesta e l'altra
 * @macro               DELAY
 */
#define DELAY                               \
    if(option->t == 1) {                    \
        struct timespec timer;              \
        timer = (option->time);             \
        saveT = timer;                      \
        nanosleep(&timer, &saveT);          \
    }

#define DESTROY_ALL                                                 \
    do {                                                            \
        i = -1;                                                     \
        while(++i < argc) {                                         \
            free(copyArgv[i]);                                      \
        }                                                           \
        free(copyArgv);                                             \
        closeConnection(option->sockname);                          \
        if(option->sockname != NULL) { free(option->sockname); }    \
        if(option->pathnameArray_W != NULL) { free(option->pathnameArray_W); }  \
        if(option->dirname_w != NULL) { free(option->dirname_w); }  \
        if(option->dirname_D != NULL) { free(option->dirname_D); }  \
        if(option->pathnameArray_r != NULL) { free(option->pathnameArray_r); }  \
        if(option->dirname_d != NULL) { free(option->dirname_d); }  \
        if(option->pathnameArray_l != NULL) { free(option->pathnameArray_l); }  \
        if(option->pathnameArray_c != NULL) { free(option->pathnameArray_c); }  \
        if(option->pathnameArray_u != NULL) { free(option->pathnameArray_u); }  \
        free(option);                                               \
    }while(0)


/**
 * @brief                   Struttura dati che mi contiene tutte le info sulle richieste fatte
 * @struct                  checkList
 * @param f                 Flag di controllo dell'avvenuta esecuzione della richiesta di connessione
 *                          al server
 * @param sockname          Nome della socket usata per la comunicazione
 * @param w                 Flag di controllo dell'avvenuta esecuzione della richiesta di scrittura di
 *                          n file da una cartella specifica
 * @param dirname_w         Nome della cartella da dove andare a pescare i file
 * @param numDirectory_w    Numero di file da andare a leggere dalla cartella ed eventuali sottocartelle
 * @param W                 Flag di controllo dell'avvenuta esecuzione della richiesta di scrittura dei file
 *                          passati come argomento
 * @param pathnameArray_W   Lista dei file da scrivere nel server
 * @param numW              Numero di file scritti nel server
 * @param D                 Flag di controllo dell'avvenuta esecuzione della richiesta di salvataggio di eventuali
 *                          file espulsi dal server nella cartella specificata
 * @param dirname_D         Nome della cartella dove andare a salvare i file
 * @param r                 Flag di controllo dell'avvenuta esecuzione della richiesta di lettura dei file
 *                          passati al server
 * @param pathnameArray_r   Lista dei file da andare a leggere
 * @param numr              Numero dei file letti
 * @param R                 Flag di controllo dell'avvenuta esecuzione della richiesta di leggere in modo casuale
 *                          N file dal server senza richiesta di aprirli e non bloccati
 * @param numFile_R         Numero di file da andare a leggere
 * @param d                 Flag di controllo dell'avvenuta esecuzione della richiesta di salvare i file letti
 *                          nella cartella specificata
 * @param dirname_d         Nome della cartella dove andare a salvare i file espulsi
 * @param t                 Flag di controllo dell'avvenuta esecuzione della richiesta di applicare un ritardo
 *                          tra una richiesta e l'altra
 * @param time              Ritardo da applicare
 * @param l                 Flag di controllo dell'avvenuta esecuzione della richiesta di lock dei file passati per
 *                          argomento
 * @param pathnameArray_l   Lista dei file su cui applicare la lock
 * @param numl              Numero di file su cui fare la lock
 * @param u                 Flag di controllo dell'avvenuta esecuzione della richiesta di unlock dei file passati per
 *                          argomento
 * @param pathnameArray_u   Lista dei file su cui applicare la unlock
 * @param numu              Numero di file su cui fare la unlock
 * @param c                 Flag di controllo dell'avvenuta esecuzione della richiesta di cancellare un file dal server
 * @param pathnameArray_c   Lista dei file da cancellare
 * @param numc              Numero dei file da cancellare
 * @param p                 Flag di controllo dell'avvenuta esecuzione della richiesta di stampare ogni richiesta sullo
 *                          stdout
 */
typedef struct {
    /** Opzione -f **/
    int f;
    char *sockname;

    /** Opzione -w **/
    int w;
    char *dirname_w;
    unsigned int numDirectory_w;

    /** Opzione -W **/
    int W;
    char** pathnameArray_W;
    unsigned int numW;

    /** Opzione -D **/
    int D;
    char *dirname_D;

    /** Opzione -r **/
    int r;
    char** pathnameArray_r;
    unsigned int numr;

    /** Opzione -R **/
    int R;
    unsigned int numFile_R;

    /** Opzione -d **/
    int d;
    char *dirname_d;

    /** Opzione -t **/
    int t;
    struct timespec time;

    /** Opzione -l **/
    int l;
    char** pathnameArray_l;
    unsigned int numl;

    /** Opzione -u **/
    int u;
    char** pathnameArray_u;
    unsigned int numu;

    /** Opzione -c **/
    int c;
    char** pathnameArray_c;
    unsigned int numc;

    /** Opzione -p **/
    int p;
}checkList;


/**
 * @brief       Messaggio di help
 * @fun         usage
 */
static void usage() {
    printf("./client ");
    printf("-f filename ");
    printf("-w dirname[,n=0] ");
    printf("-W file1[,file2] ");
    printf("-D dirname ");
    printf("-r file1[,file2] ");
    printf("-R [n=0] ");
    printf("-d dirname ");
    printf("-t time ");
    printf("-l file1[,file2] ");
    printf("-u file1[,file2] ");
    printf("-c file1[,file2] ");
    printf("-p");
    exit(EXIT_SUCCESS);
}


/**
 * @brief               Ordina i parametri per importanza
 * @fun                 orderRequest
 * @param argc          Numero di parametri
 * @param argv          Parametri da ordinare
 */
static void orderRequest(int argc, char **argv) {
    /** Variabili **/
    size_t i = 1;
    int p = 0, t = 0, h = 0;

    /** Controllo parametri **/
    if(argv == NULL) return;

    /** Ordino le richieste in ordine di importanza **/
    while(i < argc) {
        if(!strncmp(argv[i], "-h", 2)) {
            if(i == 1) { i++; continue; }
            SHIFT(i, argv, argc, 1)
            h = 1;
            return;
        } else if(!strncmp(argv[i], "-t", 2)) {
            ((i+1 < argc) && (strchr(argv[i+1], '-') == NULL)) ? (t = 1) : (t = 2);
            if(i == 1+h+p) { i+=t; continue; }
            SHIFT(i, argv, argc, 1+h)
        } else if(!strncmp(argv[i], "-p", 2)) {
            p = 1;
            if(i == 1+h) { i++; continue; }
            SHIFT(i, argv, argc, 1+h+t)
        } else if(!strncmp(argv[i], "-f", 2)) {
            if(i == 1+h+p+t) { ((i+1 < argc) && (strchr(argv[i+1], '-') == NULL)) ? (i+=2) : (i++); continue; }
            SHIFT(i, argv, argc, 1+h+p+t)
        } else { i++; }
    }
}


/** Main **/
int main(int argc, const char **argv) {
    /** Variabili **/
    long time = -1;
    int i = -1, opt = -1, numFile = 0, reads = 0, error = 0;
    char **files = NULL, *save = NULL, *tok = NULL, *abs_pathname = NULL;
    char **copyArgv = NULL, errorMessage[MAX_BUFFER_LEN];
    char *el = NULL;
    void *buf = NULL;
    size_t dimBuf = -1;
    checkList *option = NULL;
    struct stat checkFile;
    struct timespec timeout, saveT;
    sigset_t set;

    /** Controllo parametri **/
    if(argc == 1) {
        usage();
        errno = EINVAL;
        exit(errno);
    }
    if(sigemptyset(&set) == -1) {
        exit(errno);
    }
    if(sigaddset(&set, SIGPIPE) == -1) {
        exit(errno);
    }
    if((error = pthread_sigmask(SIG_BLOCK, &set, NULL)) == -1) {
        exit(errno);
    }

    /** Ordino le richieste **/
    if((copyArgv = (char **) calloc(argc, sizeof(char *))) == NULL) {
        exit(errno);
    }
    while(++i < argc) {
        if((copyArgv[i] = (char *) calloc(MAX_BUFFER_LEN, sizeof(char))) == NULL) {
            while(--i >= 0)
                free(copyArgv[i]);
            free(argv);
            exit(errno);
        }
        strncpy(copyArgv[i], argv[i], strnlen(argv[i], MAX_BUFFER_LEN)+1);
    }
    orderRequest(argc, copyArgv);
    i = -1;
    if((option = (checkList *) malloc(sizeof(checkList))) == NULL) {
        DESTROY_ALL;
        exit(errno);
    }
    memset(option, 0, sizeof(checkList));

    /** Leggo le richieste **/
    opt = getopt(argc, copyArgv, ":f:w:W:D:r:R:d:t:l:u:c:pdh");
    while(opt != -1) {
        switch(opt) {
            /** Messaggio di help **/
            case 'h':
                usage();
            break;

            /** Ritardo tra una richiesta ed un'altra **/
            case 't':
                if(strncmp(optarg, "0", 1) != 0) {
                    option->t = 1;
                    if((time = isNumber(optarg)) == -1) {
                        DESTROY_ALL;
                        fprintf(stderr, "Argomento per impostare il timer errato\n");
                        usage();
                    }
                    (option->time).tv_nsec = 0;
                    (option->time).tv_sec = time/1000;
                    DELAY
                }
            break;

            /** Abilito le stampe delle operazioni sullo stdout **/
            case 'p':
                option->p = 1;
                TRACE_ON_DISPLAY("Stampa su schermo abilitata\n")
                DELAY
            break;

            /** Connessione al server **/
            case 'f':
                TRACE_ON_DISPLAY("Tentativo di connessione al server...\n")
                DELAY
                if(option->f) {
                    TRACE_ON_DISPLAY("Connessione al server sul canale %s gia' effettuata\n", option->sockname)
                    break;
                }
                option->f = 1;
                if((option->sockname = (char *) calloc(strnlen(optarg, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
                    if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) != 0) {
                        DESTROY_ALL;
                        exit(errno);
                    }
                    TRACE_ON_DISPLAY("Errore fatale allocazione di memoria:%s\n", errorMessage)
                    DESTROY_ALL;
                    exit(errno);
                }
                strncpy(option->sockname, optarg, strnlen(optarg, MAX_PATHNAME)+1);
                timeout.tv_sec = 1;
                timeout.tv_nsec = 0;

                /** Tentativo di connessione al server fino a 10 sec dalla richiesta ogni 500 msec **/
                if(openConnection(option->sockname, 500, timeout) == -1) {
                    if(errno == ETIMEDOUT) {
                        TRACE_ON_DISPLAY("Tentativo di connessione fallita: timeout scaduto\n")
                    } else if(errno == EINVAL) {
                        TRACE_ON_DISPLAY("Tentativo di connessione fallito: sockname passata non valida\n")
                    } else {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Tentativo di connessione fallito - errore generico:%s\n", errorMessage)
                        }
                    }
                    DESTROY_ALL;
                    exit(errno);
                }
                TRACE_ON_DISPLAY("Connessione a \"%s\" riuscita\n", option->sockname)
            break;

            /** Scrivo al server i file letti dalla directory passata **/
            case 'w':
                if(!(option->f)) {
                    TRACE_ON_DISPLAY("Attenzione: non è stata effettuata nessuna connessione al server\n"   \
                                        "Richiesta rifiutata\n")
                    DESTROY_ALL;
                    errno = ECANCELED;
                    exit(errno);
                }
                if((el = strstr(optarg, ",n=")) == NULL) {
                    if((option->dirname_w = (char *) calloc(strnlen(optarg, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Errore fatale di allocazione memoria:%s\n", errorMessage)
                        }
                        DESTROY_ALL;
                        exit(errno);
                    }
                    strncpy(option->dirname_w, optarg, strnlen(optarg, MAX_PATHNAME)+1);
                } else {
                    *el = '\0';
                    if((option->dirname_w = (char *) calloc(strnlen(optarg, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Errore fatale di allocazione memoria:%s\n", errorMessage)
                        }
                        DESTROY_ALL;
                        exit(errno);
                    }
                    strncpy(option->dirname_w, optarg, strnlen(optarg, MAX_PATHNAME)+1);
                    if((option->numDirectory_w = (int) isNumber(el+3)) == -1) {
                        TRACE_ON_DISPLAY("Richiesta di scrittura - Impossibile decrifare il numero di file da leggere\n")
                        free(option->dirname_w);
                        option->numDirectory_w = 0;
                        option->w = 0;
                        errno = 0;
                        break;
                    }
                }
                option->w = 1;
                if((optind >= argc) || (strncmp(copyArgv[optind], "-D", 2) != 0)) {
                    DELAY
                    option->dirname_D = NULL;
                    TRACE_ON_DISPLAY("Richiesta di scrittura dei file da %s senza opzione di salvataggio dei file espulsi\n", option->dirname_w)
                    TRACE_ON_DISPLAY("Numero di file da leggere dalla cartella al massimo: %d\n", (option->numDirectory_w == 0) ? INT_MAX : option->numDirectory_w)
                    if((files = readNFileFromDir(option->dirname_w, option->numDirectory_w)) == NULL) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Lettura dei file da %s fallito\n", option->dirname_w)
                        }
                        if(option->dirname_w != NULL) free(option->dirname_w), option->dirname_w = NULL, option->w = 0, option->numDirectory_w = 0;
                        errno = 0;
                        break;
                    }
                    i = -1;
                    buf = NULL;
                    while((files != NULL) && (files[++i] != NULL)) {
                        if(stat(files[i], &checkFile) == -1) {
                            TRACE_ON_DISPLAY("Impossibile identificare %s\n", files[i])
                        } else if(!S_ISREG(checkFile.st_mode)) {
                            TRACE_ON_DISPLAY("%s non è un file\n", files[i])
                        } else if((abs_pathname = abs_path(files[i])) == NULL) {
                            TRACE_ON_DISPLAY("Calcolo path assoluto fallito\n")
                        } else if(readFileFromDisk(abs_pathname, &buf, &dimBuf) == -1) {
                            if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Lettura di %s dal disco fallita\n", files[i])
                            }
                        } else if(openFile(abs_pathname, (O_CREATE | O_LOCK)) == -1) {
                            if(errno == EPERM) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - File già presente nel server\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(writeFile(abs_pathname, option->dirname_D) == -1) {
                            if(errno == EACCES) {
                                TRACE_ON_DISPLAY("File:%s - Aggiunta non riuscita per mancanza di diritti su di esso\n", files[i])
                            } else if(errno == EAGAIN) {
                                TRACE_ON_DISPLAY("File:%s già presente in memoria, scrittura fallita\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile scrivere %s - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(appendToFile(abs_pathname, buf, dimBuf, option->dirname_D) == -1) {
                            if(errno == EACCES) {
                                TRACE_ON_DISPLAY("File:%s - Aggiunta contenuto non consentita\n", files[i])
                            } else if(errno == EPERM) {
                                TRACE_ON_DISPLAY("File:%s - File in fase di lock da un altro utente - Impossibile aggiornarlo\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile aggiornare il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(unlockFile(abs_pathname) == -1) {
                            if(errno == EBADF) {
                                TRACE_ON_DISPLAY("File:%s - Impossibile eseguire la unlock su un file non locked in precedenza\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile eseguire la unlock sul file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(closeFile(abs_pathname) == -1) {
                            if(errno == EBADF) {
                                TRACE_ON_DISPLAY("File:%s - Impossibile eseguire la close su un file non aperto in precedenza\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile eseguire la close sul file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else {
                            TRACE_ON_DISPLAY("File:%s scritto sul server\n", files[i])
                        }
                        if(buf != NULL) free(buf), buf = NULL;
                        if(abs_pathname != NULL) free(abs_pathname), abs_pathname = NULL;
                        free(files[i]);
                    }
                    if(files != NULL) free(files), files = NULL;
                    if(option->dirname_w != NULL) free(option->dirname_w), option->dirname_w = NULL;
                    option->w = 0;
                }
            break;

            /** Scrivo i file passati per argomento **/
            case 'W':
                if(!(option->f)) {
                    TRACE_ON_DISPLAY("Attenzione: non è stata effettuata nessuna connessione al server\n"   \
                                        "Richiesta rifiutata\n")
                    DESTROY_ALL;
                    errno = ECANCELED;
                    exit(errno);
                }
                DELETE_POINT;
                option->pathnameArray_W = files;
                option->numW = numFile;
                option->W = 1;
                if((optind >= argc) || (strncmp(copyArgv[optind], "-D", 2) != 0)) {
                    DELAY
                    TRACE_ON_DISPLAY("Richiesta di scrittura dei file passati per argomento senza opzione di salvataggio dei file espulsi\n")
                    i = -1;
                    option->dirname_D = NULL;
                    while((files != NULL) && (files[++i] != NULL)) {
                        if(stat(files[i], &checkFile) == -1) {
                            TRACE_ON_DISPLAY("Impossibile identificare il file %s\n", files[i])
                            errno = 0;
                        } else if(!S_ISREG(checkFile.st_mode)) {
                            TRACE_ON_DISPLAY("%s non è un file\n", files[i])
                            errno = 0;
                        } else if((abs_pathname = abs_path(files[i])) == NULL) {
                            TRACE_ON_DISPLAY("Calcolo path assoluto fallito\n")
                            errno = 0;
                        } else if(readFileFromDisk(abs_pathname, &buf, &dimBuf) == -1) {
                            TRACE_ON_DISPLAY("Lettura di %s dal disco fallita\n", files[i])
                            errno = 0;
                        } else if(openFile(abs_pathname, (O_CREATE | O_LOCK)) == -1) {
                            if(errno == EPERM) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - File già presente nel server\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(writeFile(abs_pathname, option->dirname_D) == -1) {
                            if(errno == EACCES) {
                                TRACE_ON_DISPLAY("File:%s - Aggiunta non riuscita per mancanza di diritti su di esso\n", files[i])
                            } else if(errno == EAGAIN) {
                                TRACE_ON_DISPLAY("File:%s già presente in memoria, scrittura fallità\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile scrivere il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(appendToFile(abs_pathname, buf, dimBuf, option->dirname_D) == -1) {
                            if(errno == EACCES) {
                                TRACE_ON_DISPLAY("File:%s - Aggiunta contenuto non consentita\n", files[i])
                            } else if(errno == EPERM) {
                                TRACE_ON_DISPLAY("File:%s - File in fase di lock da un altro utente - Impossibile aggiornarlo\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile aggiornare il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(unlockFile(abs_pathname) == -1) {
                            if(errno == EBADF) {
                                TRACE_ON_DISPLAY("File:%s - Impossibile eseguire la unlock su un file non locked in precedenza\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile eseguire la unlock sul file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(closeFile(abs_pathname) == -1) {
                            if(errno == EBADF) {
                                TRACE_ON_DISPLAY("File:%s - Impossibile eseguire la close su un file non aperto in precedenza\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile eseguire la close sul file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else {
                            TRACE_ON_DISPLAY("File:%s scritto sul server\n", files[i])
                        }
                        if(buf != NULL) free(buf), buf = NULL;
                        if(abs_pathname != NULL) free(abs_pathname), abs_pathname = NULL;
                        free(files[i]);
                    }
                    if(files != NULL) free(files), files = NULL;
                    option->pathnameArray_W = NULL, option->numW = 0;
                    option->W = 0;
                }
            break;

            /** Attivo il salvataggio dei file espulsi nella cartella specificata **/
            case 'D':
                if(!(option->f)) {
                    TRACE_ON_DISPLAY("Attenzione: non è stata effettuata nessuna connessione al server\n"   \
                                        "Richiesta rifiutata\n")
                    DESTROY_ALL;
                    errno = ECANCELED;
                    exit(errno);
                }
                if(!option->w && !option->W) {
                    TRACE_ON_DISPLAY("Nessuna richiesta di scrittura nel server è stata trovata\n" \
                                     "Richiesta di salvataggio file espulsi rigettata\n")
                    break;
                }
                if((option->dirname_D = (char *) calloc((strnlen(optarg, MAX_PATHNAME)+1), sizeof(char))) == NULL) {
                    if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                        TRACE_ON_DISPLAY("Errore fatale di allocazione memoria:%s\n", errorMessage)
                    }
                    DESTROY_ALL;
                    exit(errno);
                }
                strncpy(option->dirname_D, optarg, (strnlen(optarg, MAX_PATHNAME)+1));
                if(option->W) {
                    DELAY
                    files = option->pathnameArray_W;
                    TRACE_ON_DISPLAY("Richiesta di scrittura dei file passati per argomento con opzione di salvataggio dei file espulsi dentro \"%s\"\n", option->dirname_D)
                    i = -1;
                    while((files != NULL) && (files[++i] != NULL)) {
                        if(stat(files[i], &checkFile) == -1) {
                            TRACE_ON_DISPLAY("Impossibile identificare il file %s\n", files[i])
                        } else if(!S_ISREG(checkFile.st_mode)) {
                            TRACE_ON_DISPLAY("%s non è un file\n", files[i])
                        } else if((abs_pathname = abs_path(files[i])) == NULL) {
                            TRACE_ON_DISPLAY("Calcolo path assoluto fallito\n")
                        } else if(readFileFromDisk(abs_pathname, &buf, &dimBuf) == -1) {
                            TRACE_ON_DISPLAY("Lettura del file %s fallita\n", files[i])
                        } else if(openFile(abs_pathname, (O_CREATE | O_LOCK)) == -1) {
                            if(errno == EPERM) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - File già presente nel server\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(writeFile(abs_pathname, option->dirname_D) == -1) {
                            if(errno == EACCES) {
                                TRACE_ON_DISPLAY("File:%s - Aggiunta non riuscita per mancanza di diritti su di esso\n", files[i])
                            } else if(errno == EAGAIN) {
                                TRACE_ON_DISPLAY("File:%s già presente in memoria, scrittura fallità\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile scrivere il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(appendToFile(abs_pathname, buf, dimBuf, option->dirname_D) == -1) {
                            if(errno == EACCES) {
                                TRACE_ON_DISPLAY("File:%s - Aggiunta contenuto non consentita\n", files[i])
                            } else if(errno == EPERM) {
                                TRACE_ON_DISPLAY("File:%s - File in fase di lock da un altro utente - Impossibile aggiornarlo\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile aggiornare il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(unlockFile(abs_pathname) == -1) {
                            if(errno == EBADF) {
                                TRACE_ON_DISPLAY("File:%s - Impossibile eseguire la unlock su un file non locked in precedenza\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile eseguire la unlock sul file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(closeFile(abs_pathname) == -1) {
                            if(errno == EBADF) {
                                TRACE_ON_DISPLAY("File:%s - Impossibile eseguire la close su un file non aperto in precedenza\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile eseguire la close sul file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else {
                            TRACE_ON_DISPLAY("File:%s scritto sul server\n", files[i])
                        }

                        if(buf != NULL) free(buf), buf = NULL;
                        if(abs_pathname != NULL) free(abs_pathname), abs_pathname = NULL;
                        free(files[i]);
                    }
                    if(files != NULL) free(files), files = NULL;
                    option->pathnameArray_W = NULL, option->numW = 0;
                    option->W = 0;
                }
                if(option->w) {
                    DELAY
                    TRACE_ON_DISPLAY("Richiesta di scrittura dei file da \"%s\" con opzione di salvataggio dei file espulsi dentro \"%s\"\n", option->dirname_w, option->dirname_D)
                    TRACE_ON_DISPLAY("Numero di file da leggere dalla cartella al massimo: %d\n", (option->numDirectory_w == 0) ? INT_MAX : option->numDirectory_w)
                    if((files = readNFileFromDir(option->dirname_w, option->numDirectory_w)) == NULL) {
                        TRACE_ON_DISPLAY("Lettura dei file da %s fallito\n", option->dirname_w)
                        if(errno == ENOENT) {
                            TRACE_ON_DISPLAY("Cartella non trovata\n")
                        }
                        errno = 0;
                        break;
                    }
                    i = -1;
                    while((files != NULL) && (files[++i] != NULL)) {
                        if(stat(files[i], &checkFile) == -1) {
                            TRACE_ON_DISPLAY("Impossibile identificare il file %s\n", files[i])
                        } else if(!S_ISREG(checkFile.st_mode)) {
                            TRACE_ON_DISPLAY("%s non è un file\n", files[i])
                        } else if((abs_pathname = abs_path(files[i])) == NULL) {
                            TRACE_ON_DISPLAY("Calcolo path assoluto fallito\n")
                        } else if(readFileFromDisk(abs_pathname, &buf, &dimBuf) == -1) {
                            if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Lettura del file %s fallita\n", files[i])
                            }
                        } else if(openFile(abs_pathname, (O_CREATE | O_LOCK)) == -1) {
                            if(errno == EPERM) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - File già presente nel server\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(writeFile(abs_pathname, option->dirname_D) == -1) {
                            if(errno == EACCES) {
                                TRACE_ON_DISPLAY("File:%s - Aggiunta non riuscita per mancanza di diritti su di esso\n", files[i])
                            } else if(errno == EAGAIN) {
                                TRACE_ON_DISPLAY("File:%s già presente in memoria, scrittura fallità\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile scrivere il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(appendToFile(abs_pathname, buf, dimBuf, option->dirname_D) == -1) {
                            if(errno == EACCES) {
                                TRACE_ON_DISPLAY("File:%s - Aggiunta contenuto non consentita\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(errno == EPERM) {
                                TRACE_ON_DISPLAY("File:%s - File in fase di lock da un altro utente - Impossibile aggiornarlo\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile aggiornare il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(unlockFile(abs_pathname) == -1) {
                            if(errno == EBADF) {
                                TRACE_ON_DISPLAY("File:%s - Impossibile eseguire la unlock su un file non locked in precedenza\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile eseguire la unlock sul file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(closeFile(abs_pathname) == -1) {
                            if(errno == EBADF) {
                                TRACE_ON_DISPLAY("File:%s - Impossibile eseguire la close su un file non aperto in precedenza\n", files[i])
                            } else if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("File:%s non presente in memoria/espulso in precedenza\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile eseguire la close sul file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else {
                            TRACE_ON_DISPLAY("File:%s scritto sul server\n", files[i])
                        }

                        if(buf != NULL) free(buf), buf = NULL;
                        if(abs_pathname != NULL) free(abs_pathname), abs_pathname = NULL;
                        free(files[i]);
                    }
                    if(files != NULL) free(files), files = NULL;
                    if(option->dirname_w != NULL) free(option->dirname_w), option->dirname_w = NULL;
                    option->w = 0;
                }
                if(option->dirname_D != NULL) free(option->dirname_D), option->dirname_D = NULL;
                option->D = 0;
            break;

            /** Richiedo la lettura dei file passati per argomento al server **/
            case 'r':
                if(!(option->f)) {
                    TRACE_ON_DISPLAY("Attenzione: non è stata effettuata nessuna connessione al server\n"   \
                                        "Richiesta rifiutata\n")
                    DESTROY_ALL;
                    errno = ECANCELED;
                    exit(errno);
                }
                DELETE_POINT;
                option->pathnameArray_r = files;
                option->numr = numFile;
                option->r = 1;
                if((optind >= argc) || (strncmp(copyArgv[optind], "-d", 2) != 0)) {
                    DELAY
                    TRACE_ON_DISPLAY("Richiesta di lettura dei file passati senza opzione di salvataggio dei file\n")
                    i = -1;
                    while((files != NULL) && (files[++i] != NULL)) {
                        if(openFile(files[i], 0) == -1) {
                            if(errno == EMLINK) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Troppi utenti hanno richiesto di aprire il file\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(readFile(files[i], &buf, &dimBuf) == -1) {
                            if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("Impossibile leggere il file \"%s\" - Rimosso o espulso dal server\n", files[i])
                            } if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile leggere il file \"%s\" - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(closeFile(files[i]) == -1) {
                            if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile chiudere il file \"%s\" - Errore: %s\n", files[i], errorMessage)
                            }
                        } else {
                            TRACE_ON_DISPLAY("File:%s letto dal server\n", files[i])
                        }

                        if(buf != NULL) free(buf);
                        buf = NULL;
                        free(files[i]);
                    }
                    if(files != NULL) free(files), files = NULL;
                    option->pathnameArray_r = NULL;
                    option->r = 0;
                    option->numr = 0;
                }
            break;

            /** Richiedo la lettura di N file random dal server **/
            case 'R':
                if(!(option->f)) {
                    TRACE_ON_DISPLAY("Attenzione: non è stata effettuata nessuna connessione al server\n"   \
                                        "Richiesta rifiutata\n")
                    DESTROY_ALL;
                    errno = ECANCELED;
                    exit(errno);
                }
                if((optarg != NULL) && (strchr(optarg, '-') == NULL) && ((tok = strstr(optarg, "n=")) != NULL)) {
                    if((option->numFile_R = isNumber(optarg+2)) == -1) option->numFile_R = 0;
                } else {
                    if((optarg != NULL) && (strchr(optarg, '-') != NULL)) {
                        optind--;
                    }
                    option->numFile_R = 0;
                }
                option->R = 1;
                if((optind >= argc) || (strncmp(copyArgv[optind], "-d", 2) != 0)) {
                    DELAY
                    reads = 0;
                    TRACE_ON_DISPLAY("Lettura di N file random dal server senza opzione di salvataggio\n")
                    option->dirname_d = NULL;
                    if((reads = readNFiles((int) option->numFile_R, option->dirname_d)) == -1) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Errore nella lettura random dei file dal server - Error: %s\n", errorMessage)
                        }
                    }
                    TRACE_ON_DISPLAY("Letti dal server %d file\n", reads)
                    option->numFile_R = 0, option->R = 0;
                }
            break;

            /** Richiesta di salvataggio dei file letti dal server **/
            case 'd':
                if(!(option->f)) {
                    TRACE_ON_DISPLAY("Attenzione: non è stata effettuata nessuna connessione al server\n"   \
                                        "Richiesta rifiutata\n")
                    DESTROY_ALL;
                    errno = ECANCELED;
                    exit(errno);
                }
                if(!option->r && !option->R) {
                    TRACE_ON_DISPLAY("Nessuna richiesta di lettura dal server è stata trovata\n" \
                                     "Richiesta di salvataggio file letti rigettata\n")
                    break;
                }
                if((option->dirname_d = (char *) calloc((strnlen(optarg, MAX_PATHNAME)+1), sizeof(char))) == NULL) {
                    if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                        TRACE_ON_DISPLAY("Errore fatale di allocazione memoria:%s\n", errorMessage)
                    }
                    DESTROY_ALL;
                    exit(errno);
                }
                strncpy(option->dirname_d, optarg, (strnlen(optarg, MAX_PATHNAME)+1));
                if(option->r) {
                    DELAY
                    TRACE_ON_DISPLAY("Richiesta di lettura dei file passati con opzione di salvataggio dei file dentro \"%s\"\n", option->dirname_d)
                    i = -1;
                    while((files != NULL) && (files[++i] != NULL)) {
                        if(openFile(files[i], 0) == -1) {
                            if(errno == EMLINK) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Troppi utenti hanno richiesto di aprire il file\n", files[i])
                            } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(readFile(files[i], &buf, &dimBuf) == -1) {
                            if(errno == ENOENT) {
                                TRACE_ON_DISPLAY("Impossibile leggere il file \"%s\" - Rimosso o espulso dal server\n", files[i])
                            } if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile leggere il file \"%s\" - Errore: %s\n", files[i], errorMessage)
                            }
                        } else if(closeFile(files[i]) == -1) {
                            if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                TRACE_ON_DISPLAY("Impossibile chiudere il file \"%s\" - Errore: %s\n", files[i], errorMessage)
                            }
                        } else {
                            TRACE_ON_DISPLAY("File:%s letto dal server\n", files[i])
                            if(writeFileIntoDisk(files[i], option->dirname_d, buf, dimBuf) == -1) {
                                if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                                    TRACE_ON_DISPLAY("Salvatagio del file dentro \"%s\" non riuscita - Errore: %s\n", option->dirname_d, errorMessage)
                                }
                            }
                        }

                        if(buf != NULL) free(buf);
                        buf = NULL;
                        free(files[i]);
                    }
                    if(files != NULL) free(files), files = NULL;
                    option->pathnameArray_r = NULL;
                    option->r = 0;
                    option->numr = 0;
                }
                if(option->R) {
                    DELAY
                    reads = 0;
                    TRACE_ON_DISPLAY("Lettura di N file random dal server con opzione di salvataggio dentro \"%s\"\n", option->dirname_d)
                    if((reads = readNFiles((int) option->numFile_R, option->dirname_d)) == -1) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Errore nella lettura random dei file dal server - Error: %s\n", errorMessage)
                        }
                    }
                    TRACE_ON_DISPLAY("Letti dal server %d file\n", reads)
                    option->numFile_R = 0, option->R = 0;
                }
                free(option->dirname_d), option->dirname_d = NULL;
                option->d = 0;
            break;

            /** Richiesta di lock di file nel server **/
            case 'l':
                if(!(option->f)) {
                    TRACE_ON_DISPLAY("Attenzione: non è stata effettuata nessuna connessione al server\n"   \
                                        "Richiesta rifiutata\n")
                    DESTROY_ALL;
                    errno = ECANCELED;
                    exit(errno);
                }
                DELETE_POINT;
                option->pathnameArray_l = files;
                option->numl = numFile;
                option->l = 1;
                i = -1;
                DELAY
                while((files != NULL) && (files[++i] != NULL)) {
                    if(openFile(files[i], 0) == -1) {
                        if(errno == EMLINK) {
                            TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Troppi utenti hanno richiesto di aprire il file\n", files[i])
                        } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Errore: %s\n", files[i], errorMessage)
                        }
                    } else if(lockFile(files[i]) == -1) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Impossibile effettuare la lock del file \"%s\" - Errore: %s\n", files[i], errorMessage)
                        }
                    } else {
                        TRACE_ON_DISPLAY("Lock del file \"%s\" effettuata correttamente\n", files[i])
                    }
                    free(files[i]);
                }
                if(files != NULL) free(files), files = NULL;
                option->l = 0;
                option->numl = 0;
                option->pathnameArray_l = NULL;
            break;

            case 'u':
                if(!(option->f)) {
                    TRACE_ON_DISPLAY("Attenzione: non è stata effettuata nessuna connessione al server\n"   \
                                        "Richiesta rifiutata\n")
                    DESTROY_ALL;
                    errno = ECANCELED;
                    exit(errno);
                }
                DELETE_POINT;
                option->pathnameArray_u = files;
                option->numu = numFile;
                option->u = 1;
                i = -1;
                DELAY
                while((files != NULL) && (files[++i] != NULL)) {
                    if(unlockFile(files[i]) == -1) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Impossibile effettuare la unlock del file \"%s\" - Errore: %s\n", files[i], errorMessage)
                        }
                    } else if(closeFile(files[i])) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Impossibile chiudere il file \"%s\" - Errore: %s\n", files[i], errorMessage)
                        }
                    } else {
                        TRACE_ON_DISPLAY("Unlock del file \"%s\" effettuata correttamente\n", files[i])
                    }
                    free(files[i]);
                }
                if(files != NULL) free(files), files = NULL;
                option->u = 0;
                option->numu = 0;
                option->pathnameArray_u = NULL;
            break;

            case 'c':
                if(!(option->f)) {
                    TRACE_ON_DISPLAY("Attenzione: non è stata effettuata nessuna connessione al server\n"   \
                                        "Richiesta rifiutata\n")
                    DESTROY_ALL;
                    errno = ECANCELED;
                    exit(errno);
                }
                DELETE_POINT;
                option->pathnameArray_c = files;
                option->numc = numFile;
                option->c = 1;
                i = -1;
                DELAY
                while((files != NULL) && (files[++i] != NULL)) {
                    TRACE_ON_DISPLAY("Tentativo di cancellare il file %s\n", files[i])
                    if(openFile(files[i], 0) == -1) {
                        if(errno == EMLINK) {
                            TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Troppi utenti hanno richiesto di aprire il file\n", files[i])
                        } else if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Errore: %s\n", files[i], errorMessage)
                        }
                    } else if(removeFile(files[i]) == -1) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) == 0) {
                            TRACE_ON_DISPLAY("Impossibile aprire il file '%s' - Errore: %s\n", files[i], errorMessage)
                        }
                    } else {
                        TRACE_ON_DISPLAY("File \"%s\" cancellato correttamente\n", files[i])
                    }
                    free(files[i]);
                }
                if(files != NULL) free(files), files = NULL;
                option->c = 0;
                option->numc = 0;
                option->pathnameArray_c = NULL;
            break;

            default:
                if(optopt == 'R') {
                    opt = 'R';
                    continue;
                }
                fprintf(stderr, "Opzione -%c non riconosciuta\n", optopt);
        }
        opt = getopt(argc, copyArgv, ":f:w:W:D:r:R:d:t:l:u:c:pdh");
    }
    DESTROY_ALL;

    return 0;
}
