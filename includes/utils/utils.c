/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Funzioni di diverse utilita'
 * @author              Simone Tassotti
 * @date                23/12/2021
 * @finish              25/01/2022
 */

#include "utils.h"


/**
 * @brief                   Data una stringa la converte in valore long
 * @fun                     isNumber
 * @param s                 Stringa da convertire
 * @return                  In caso di successo ritorna il valore numero della stringa, altrimenti -1
 */
long isNumber(const char* s) {
    char* e = NULL;
    long val = strtol(s, &e, 0);

    return val;
}


/**
 * @brief                   Funzione che riceve i dati da fd e salva il contenuto
 *                          di dimensione n su ptr in modo completo dalla socket
 * @fun                     readn
 * @param fd                Fd da cui leggere i dati
 * @param ptr               Buffer su cui salvare i dati
 * @param n                 Dimensione del buffer
 * @return                  Ritorna la dimensione dei dati letti; altrimenti ritorna i
 *                          byte che è riuscita a leggere, 0 in caso di EOF o -1
 */
static ssize_t readn(int fd, void *ptr, size_t n) {
    size_t   nleft;
    ssize_t  nread;

    nleft = n;
    while (nleft > 0) {
        if((nread = read(fd, ptr, nleft)) < 0) {
            perror("errore");
            if (nleft == n) return -1; /* error, return -1 */
            else break; /* error, return amount read so far */
        } else if (nread == 0) break; /* EOF */
        nleft -= nread;
        ptr   += nread;
    }
    return(n - nleft); /* return >= 0 */
}


/**
 * @brief                   Funzione che scrive i dati a fd
 *                          di dimensione n da ptr in modo completo dalla socket
 * @fun                     writen
 * @param fd                Fd su cui scrivere i dati
 * @param ptr               Buffer su cui scrivere i dati
 * @param n                 Dimensione del buffer
 * @return                  Ritorna la dimensione dei dati scritti; altrimenti ritorna i
 *                          byte che è riuscita a scrivere o -1
 */
static ssize_t writen(int fd, void *ptr, size_t n) {
    size_t   nleft;
    ssize_t  nwritten;

    nleft = n;
    while (nleft > 0) {
        if((nwritten = write(fd, ptr, nleft)) < 0) {
            if (nleft == n) return -1; /* error, return -1 */
            else break; /* error, return amount written so far */
        } else if (nwritten == 0) break;
        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n - nleft); /* return >= 0 */
}


/**
 * brief                Manda un messaggio dalla server sulla socket indicata
 * @fun                 sendMSG
 * @param fd            FD su cui mandare il messaggio
 * @param msg           Messaggio da mandare
 * @param msgSize       Dimensione del messaggio da spedire
 * @return              Ritorna il numero di byte scritti o -1 in caso di errore [setta errno]
 */
ssize_t sendMSG(int fd, void *msg, size_t msgSize) {
    /** Variabili **/
    ssize_t bytesSendIt = -1, nWrites = -1;

    /** Controllo parametri **/
    errno = 0;
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Invio richiesta al server **/
    if((nWrites = writen(fd, &msgSize, sizeof(size_t))) != sizeof(size_t)) { // 1° Step: mando la dimensione del messaggio
        errno = ECOMM;
        return -1;
    }
    bytesSendIt += nWrites;
    if(msgSize != 0) {
        if((nWrites = writen(fd, msg, msgSize)) != msgSize) { // 2° Step: Invio del messaggio
            errno = ECOMM;
            return -1;
        }
        bytesSendIt += nWrites;
    }

    errno = 0;
    return bytesSendIt;
}


/**
 * @brief               Riceve un messaggio dalla socket indicata
 * @fun                 receiveMSG
 * @param fd            FD da cui riceve il messaggio
 * @param msg           Buffer che conterrà il messaggio ricevuto
 * @param msgSize       Dimensione del messaggio che ricevo
 * @return              In caso di successo ritorna il numero di bytes letti; altrimenti
 *                      -1 [setta errno]
 */
ssize_t receiveMSG(int fd, void **msg, size_t *msgSize) {
    /** Variabili **/
    int isNull = 0;
    ssize_t bytesReceiveIt = -1, nReads = -1;

    /** Controllo parametri **/
    errno = 0;
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Ricevo il messaggio **/
    if((msgSize == NULL) && ((isNull = 1, msgSize = (size_t *) malloc(sizeof(size_t))) == NULL)) {
        return -1;
    }
    if((nReads = readn(fd, msgSize, sizeof(size_t))) != sizeof(size_t)) {
        if(isNull) free(msgSize), msgSize = NULL;
        errno = ECOMM;
        if(nReads == 0) return 0;
        else return -1;
    }
    bytesReceiveIt += nReads;
    if(*msg != NULL) free(*msg);
    if((*msg = malloc(*msgSize)) == NULL) {
        if(isNull) free(msgSize), msgSize = NULL;
        return -1;
    }
    if(*msgSize != 0) {
        if((nReads = readn(fd, *msg, *msgSize)) != *msgSize) {
            errno = ECOMM;
            if(isNull) free(msgSize), msgSize = NULL;
            free(*msg);
            msg = NULL;
            if(nReads == 0) return 0;
            else return -1;
        }
        bytesReceiveIt += nReads;
    }


    if(isNull) free(msgSize);
    errno = 0;
    return bytesReceiveIt;
}