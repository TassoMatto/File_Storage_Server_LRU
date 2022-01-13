/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Funzioni di diverse utilita'
 * @author              Simone Tassotti
 * @date                23/12/2021
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


static ssize_t readn(int fd, void *ptr, size_t n) {
    size_t   nleft;
    ssize_t  nread;

    nleft = n;
    while (nleft > 0) {
        if((nread = read(fd, ptr, nleft)) < 0) {
            if (nleft == n) return -1; /* error, return -1 */
            else break; /* error, return amount read so far */
        } else if (nread == 0) break; /* EOF */
        nleft -= nread;
        ptr   += nread;
    }
    return(n - nleft); /* return >= 0 */
}


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


ssize_t sendMSG(int fd, void *msg, size_t msgSize) {
    /** Variabili **/
    ssize_t bytesSendIt = -1, nWrites = -1;

    /** Controllo parametri **/
    if(fd <= 0) { errno = EINVAL; return -1; }
    if(msg == NULL) { errno = EINVAL; return -1; }
    if(msgSize == 0) { errno = EINVAL; return -1; }

    /** Invio richiesta al server **/
    if((nWrites = writen(fd, &msgSize, sizeof(size_t))) == -1) { // 1° Step: mando la dimensione del messaggio
        errno = ECOMM;
        return -1;
    }
    bytesSendIt += nWrites;
    if((nWrites = writen(fd, msg, msgSize)) == -1) { // 2° Step: Invio del messaggio
        errno = ECOMM;
        return -1;
    }
    bytesSendIt += nWrites;
    return bytesSendIt;
}


ssize_t receiveMSG(int fd, void **msg, size_t *msgSize) {
    /** Variabili **/
    int isNull = 0;
    ssize_t bytesReceiveIt = -1, nReads = -1;

    /** Controllo parametri **/
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Ricevo il messaggio **/
    if((msgSize == NULL) && ((isNull = 1, msgSize = (size_t *) malloc(sizeof(size_t))) == NULL)) {
        return -1;
    }
    if((nReads = readn(fd, msgSize, sizeof(size_t))) <= 0) {
        errno = ECOMM;
        if(nReads == 0) return 0;
        else return -1;
    }
    bytesReceiveIt += nReads;
    if((*msg = malloc(*msgSize)) == NULL) {
        return -1;
    }
    if((nReads = readn(fd, *msg, *msgSize)) == -1) {
        errno = ECOMM;
        free(msg);
        if(nReads == 0) return 0;
        else return -1;
    }
    bytesReceiveIt += nReads;

    if(isNull) free(msgSize);
    return bytesReceiveIt;
}