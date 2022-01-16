/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Client per la connessione al FileStorageServer
 * @author              Simone Tassotti
 * @date                16/01/2022
 */


#define _POSIX_C_SOURCE 2001112L
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <utils.h>
#include <Client_API.h>


#define TRACE_ON_DISPLAY(SWITCH, TXT, ...)                      \
    if((SWITCH)) {                                              \
        printf(TXT, ##__VA_ARGS__);                               \
    }


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


#define DELETE_POINT \


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
    time_t time;

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
    printf(" -f filename ");
    printf(" -w dirname[,n=0] ");
    printf(" -W file1[,file2] ");
    printf(" -D dirname ");
    printf(" -r file1[,file2] ");
    printf(" -R [n=0] ");
    printf(" -d dirname ");
    printf(" -t time ");
    printf(" -l file1[,file2] ");
    printf(" -u file1[,file2] ");
    printf(" -c file1[,file2] ");
    printf(" -p");
    sleep(3);
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
    while(i < argc)
    {
        if(!strncmp(argv[i], "-h", 2))
        {
            if(i == 1) { i++; continue; }
            SHIFT(i, argv, argc, 1)
            h = 1;
            return;
        }
        else if(!strncmp(argv[i], "-t", 2))
        {
            ((i+1 < argc) && (strchr(argv[i+1], '-') == NULL)) ? (t = 1) : (t = 2);
            if(i == 1+h) { i+=t; continue; }
            SHIFT(i, argv, argc, 1+h)
        }
        else if(!strncmp(argv[i], "-p", 2))
        {
            p = 1;
            if(i == 1+h+t) { i++; continue; }
            SHIFT(i, argv, argc, 1+h+t)
        }
        else if(!strncmp(argv[i], "-f", 2))
        {
            if(i == 1+h+p+t) { ((i+1 < argc) && (strchr(argv[i+1], '-') == NULL)) ? (i+=2) : (i++); continue; }
            SHIFT(i, argv, argc, 1+h+p+t)
        }
        else { i++; }
    }
}


int main(int argc, const char **argv) {
    /** Variabili **/
    int i = -1, opt = -1;
    char **copyArgv = NULL, errorMessage[MAX_BUFFER_LEN];
    char *el = NULL;
    checkList *option = NULL;
    struct timespec timeout;

    /** Controllo parametri **/
    if(argc == 1) {
        usage();
        errno = EINVAL;
        exit(errno);
    }

    /** Ordino le richieste **/
    if((copyArgv = (char **) calloc(argc, sizeof(char *))) == NULL) {
        exit(errno);
    }
    while(++i < argc) {
        if((copyArgv[i] = (char *) calloc(strnlen(argv[i], MAX_BUFFER_LEN)+1, sizeof(char))) == NULL) {
            exit(errno);
        }
        strncpy(copyArgv[i], argv[i], strnlen(argv[i], MAX_BUFFER_LEN)+1);
    }
    orderRequest(argc, copyArgv);
    if((option = (checkList *) malloc(sizeof(checkList))) == NULL) {
        i = -1;
        while(++i < argc) free(copyArgv[i]);
        free(copyArgv);
        exit(errno);
    }
    memset(option, 0, sizeof(checkList));

    /** Leggo le richieste **/
    opt = getopt(argc, copyArgv, ":f:w:W:D:r:R:d:t:l:u:c:pdh");
    while(opt != -1) {
        printf("%c\n", opt);
        switch(opt) {
            /** Messaggio di help **/
            case 'h':
                sleep(option->t);
                usage();
            break;

            /** Ritardo tra una richiesta ed un'altra **/
            case 't':
                if(strncmp(optarg, "0", 1) != 0) {
                    if((option->t = (int) isNumber(optarg)) == -1) {
                        TRACE_ON_DISPLAY(option->p, "Inserire un numero per specificare il delay\n")
                        i = -1;
                        //LIVERARE ANCHE OPTION
                        while(++i < argc) free(copyArgv[i]);
                        free(copyArgv);
                        exit(errno);
                    }
                }
            break;

            /** Abilito le stampe delle operazioni sullo stdout **/
            case 'p':
                sleep(option->t);
                option->p = 1;
            break;

            /** Connessione al server **/
            case 'f':
                sleep(option->t);
                TRACE_ON_DISPLAY(option->p, "Tentativo di connessione al server...\n")
                if(option->f == 0) {
                    option->f = 1;
                    if((option->sockname = (char *) calloc(strnlen(optarg, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) != 0) {
                            i = -1;
                            //LIVERARE ANCHE OPTION
                            while(++i < argc) free(copyArgv[i]);
                            free(copyArgv);
                            exit(errno);
                        }
                        TRACE_ON_DISPLAY(option->p, "Opzione -w fallita: %s\n", errorMessage)
                        i = -1;
                        //LIVERARE ANCHE OPTION
                        while(++i < argc) free(copyArgv[i]);
                        free(copyArgv);
                        exit(errno);
                    }
                    strncpy(option->sockname, optarg,strnlen(optarg, MAX_PATHNAME)+1);
                    timeout.tv_sec = 3;
                    timeout.tv_nsec = 0;
                    if(openConnection(option->sockname, 200, timeout) == -1) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) != 0) {
                            i = -1;
                            //LIVERARE ANCHE OPTION
                            while(++i < argc) free(copyArgv[i]);
                            free(copyArgv);
                            exit(errno);
                        }
                        TRACE_ON_DISPLAY(option->p, "Tentativo di connessione fallito: %s\n", errorMessage)
                        i = -1;
                        //LIVERARE ANCHE OPTION
                        while(++i < argc) free(copyArgv[i]);
                        free(copyArgv);
                        exit(errno);
                    }
                } else {
                    TRACE_ON_DISPLAY(option->p, "Connessione al server già effettuata\n")
                }
            break;

            /** Estraggo dalla cartella indicata n file **/
            case 'w':
                sleep(option->t);
                option->w = 1;
                if((el = strstr(optarg, ",n=")) == NULL) {
                    if((option->dirname_w = (char *) calloc(strnlen(optarg, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) != 0) {
                            i = -1;
                            //LIVERARE ANCHE OPTION
                            while(++i < argc) free(copyArgv[i]);
                            free(copyArgv);
                            exit(errno);
                        }
                        TRACE_ON_DISPLAY(option->p, "Opzione -w fallita: %s\n", errorMessage)
                        i = -1;
                        //LIVERARE ANCHE OPTION
                        while(++i < argc) free(copyArgv[i]);
                        free(copyArgv);
                        exit(errno);
                    }
                    strncpy(option->dirname_w, optarg, strnlen(optarg, MAX_PATHNAME)+1);
                } else {
                    *el = '\0';
                    if((option->dirname_w = (char *) calloc(strnlen(optarg, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) != 0) {
                            i = -1;
                            //LIVERARE ANCHE OPTION
                            while(++i < argc) free(copyArgv[i]);
                            free(copyArgv);
                            exit(errno);
                        }
                        TRACE_ON_DISPLAY(option->p, "Opzione -w fallita: %s\n", errorMessage)
                        i = -1;
                        //LIVERARE ANCHE OPTION
                        while(++i < argc) free(copyArgv[i]);
                        free(copyArgv);
                        exit(errno);
                    }
                    strncpy(option->dirname_w, optarg, strnlen(optarg, MAX_PATHNAME)+1);
                    if((option->numDirectory_w = (int) isNumber(el+3)) == -1) {
                        if(strerror_r(errno, errorMessage, MAX_BUFFER_LEN) != 0) {
                            i = -1;
                            //LIVERARE ANCHE OPTION
                            while(++i < argc) free(copyArgv[i]);
                            free(copyArgv);
                            exit(errno);
                        }
                        TRACE_ON_DISPLAY(option->p, "Opzione -w fallita: %s\n", errorMessage)
                        i = -1;
                        //LIVERARE ANCHE OPTION
                        while(++i < argc) free(copyArgv[i]);
                        free(copyArgv);
                        exit(errno);
                    }
                }
                if((optind >= argc) || (strncmp(copyArgv[optind], "-D", 2) != 0)) {
                    TRACE_ON_DISPLAY(option->p, "Opzione -w\t\tCartella:%s - NumeroFile:%d senza opzione di salvataggio dei file espulsi\n", option->dirname_w, option->numDirectory_w)

                }
            break;

            /** Scrivo i file passati per argomento **/
            case 'W':
                sleep(option->t);
                option->W = 1;
                int numFile = 0;
                char **files = NULL, *save = NULL, *tok = NULL;
                tok = strtok_r(optarg, ",", &save);
                while(tok != NULL) {
                    numFile++;
                    if((files = (char **) realloc(files, sizeof(char *)*(numFile + 1))) == NULL) {
                        // PULIRE
                    }
                    if((files[numFile-1] = (char *) calloc(strnlen(tok, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
                        // PULIRE
                    }
                    strncpy(files[numFile-1], tok, strnlen(tok, MAX_PATHNAME)+1);
                    files[numFile] = NULL;
                    tok = strtok_r(NULL, ",", &save);
                }
                option->pathnameArray_W = files;
                option->numW = numFile;
                if((optind >= argc) || (strncmp(copyArgv[optind], "-D", 2) != 0)) {
                    TRACE_ON_DISPLAY(option->p, "Opzione -W\t\tNumeroFile:%d senza opzione di salvataggio dei file espulsi\n", option->numW)

                }
            break;

            default:
                fprintf(stderr, "Opzione -%c non riconosciuta\n", optopt);
        }
        opt = getopt(argc, copyArgv, ":f:w:W:D:r:R:d:t:l:u:c:pdh");
    }
    printf("%s %d\n %s\n", option->dirname_w, option->numDirectory_w, option->sockname);

    return 0;
}
