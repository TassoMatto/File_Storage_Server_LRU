/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione di una coda
 * @author              Simone Tassotti
 * @date                24/12/2021
 */

#include "queue.h"


/**
 * @brief               Funzione che inserisce in coda un elemento
 * @fun                 insertIntoQueue
 * @param q             Coda
 * @param data          Elemento da aggiungere
 * @param size          Dimensione dell'elemento da aggiungere
 * @return              Ritorna la lista aggiornata in caso di successo; altrimenti ritorna NULL [setta errno]
 */
Queue* insertIntoQueue(Queue *q, void *data, size_t size) {
    /** Variabili **/
    Queue *new = NULL, *corr = NULL;
    void *dataCopy = NULL;

    /** Controllo Parametri **/
    if(data == NULL) { errno = EINVAL; return NULL; }
    if(size <= 0) { errno = EINVAL; return NULL; }

    /** Aggiungo l'elemento in coda **/
    if((new = (Queue *) malloc(sizeof(Queue))) == NULL) return NULL;
    if((dataCopy = malloc(size)) == NULL) { free(new); return NULL; }
    memcpy(dataCopy, data, size);
    new->next = NULL, new->data = dataCopy, new->size = size;
    if(q == NULL) return new;   // Se la lista inizialmente e' vuota ritorno il valore direttamente
    corr = q;
    while(corr->next != NULL)
        corr = corr->next;
    corr->next = new;

    return q;
}


/**
 * @brief               Funzione che elimina un determinato elemento dalla funzione
 * @fun                 deleteElementFromQueue
 * @param q             Coda
 * @param data          Elemento per cercare l'oggetto da eliminare
 * @param fun           Funzione di comparazione tra elementi
 * @return              Ritorna l'elemento della lista da cancellare; altrimenti ritorna NULL se nessun elemento e' stato
 *                      cancellato [in caso di errore setta anche errno]
 */
void* deleteElementFromQueue(Queue **q, void *data, Compare_Fun fun) {
    /** Controllo parametri **/
    if(data == NULL) { errno = EINVAL; return NULL; }
    if(fun == NULL) { errno = EINVAL; return NULL; }

    if(*q == NULL) return NULL;

    /** Cancello l'elemento se esiste **/
    if(fun(data, (*q)->data)) {
        /** Variabili **/
        Queue *del = NULL;
        void *delData = NULL;

        /** Tolgo elemento dalla coda **/
        del = (*q);
        (*q) = (*q)->next;
        delData = del->data;
        free(del);

        return delData;
    }

    return deleteElementFromQueue(&((*q)->next), data, fun);
}


/**
 * @brief               Cancella il primo elemento della coda rispettando la politica della coda FIFO
 * @fun                 deleteFirstElement
 * @param q             Lista da cui cancellare il primo elemento
 * @return              Ritorna l'elemento cancellato; in caso di errore ritorna NULL [setta errno]
 */
void* deleteFirstElement(Queue **q) {
    /** Variabili **/
    void *delData = NULL;
    Queue *del = NULL;

    /** Controllo parametri **/
    if(*q == NULL) { errno = EINVAL; return NULL; }

    /** Cancello il primo elemento **/
    del = (*q);
    (*q) = (*q)->next;
    delData = del->data;
    free(del);

    return delData;
}


/**
 * @brief               Cancella tutta la coda
 * @fun                 destroyQueue
 * @param q             Coda da cancellare
 */
void destroyQueue(Queue **q) {
    /** Controllo parametri **/
    if(*q == NULL) return;

    /** Cancello la coda in modo bottom-up **/
    destroyQueue(&((*q)->next));
    free((*q)->data);
    free(*q);
    *q = NULL;
}