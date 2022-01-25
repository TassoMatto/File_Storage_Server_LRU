/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione di una coda
 * @author              Simone Tassotti
 * @date                24/12/2021
 * @finish              25/01/2022
 */

#ifndef FILE_STORAGE_SERVER_LRU_QUEUE_H

    #define FILE_STORAGE_SERVER_LRU_QUEUE_H

    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <errno.h>


    /**
     * @brief               Struttura dati che rappresenta un elemento della coda
     * @struct              Queue
     * @param data          Contiene il dato
     * @param size          Dimensione in byte del dato
     * @param next          Puntatore all'elemento successivo
     */
    typedef struct queue_el {
        void *data;
        size_t size;
        struct queue_el *next;
    } Queue;


    /**
     * @brief               Struttura che rappresenta il prototipo di funzione di comparazione tra elementi
     * @struct              Compare_Fun
     */
    typedef int (*Compare_Fun)(const void *, const void *);


    /**
     * @brief               Struttura per rappresentare la funzione di free dei dati
     * @struct              Free_Data
     */
    typedef void (*Free_Data)(void *);


    /**
     * @brief                       Funzione che inserisce in coda un elemento
     * @fun                         insertIntoQueue
     * @return                      Ritorna la lista aggiornata in caso di successo; altrimenti ritorna NULL [setta errno]
     */
    Queue* insertIntoQueue(Queue *, void *, size_t);


    /**
     * @brief               Cerca un elemento nella coda
     * @fun                 elementExist
     * @return              (1) se l'elemento e' stato trovato, (0) se non ho trovato niente; (-1) [setta errno]
     */
    int elementExist(Queue *, void *, Compare_Fun);


    /**
     * @brief               Funzione che elimina un determinato elemento dalla funzione
     * @fun                 deleteElementFromQueue
     * @return              Ritorna l'elemento della lista da cancellare; altrimenti ritorna NULL se nessun elemento e' stato
     *                      cancellato [in caso di errore setta anche errno]
     */
    void* deleteElementFromQueue(Queue **, void *, Compare_Fun);


    /**
     * @brief               Cancella il primo elemento della coda rispettando la politica della coda FIFO
     * @fun                 deleteFirstElement
     * @return              Ritorna l'elemento cancellato; in caso di errore ritorna NULL [setta errno]
     */
    void* deleteFirstElement(Queue **);


    /**
     * @brief               Cancella tutta la coda
     * @fun                 destroyQueue
     */
    void destroyQueue(Queue **q, Free_Data);


#endif //FILE_STORAGE_SERVER_LRU_QUEUE_H
