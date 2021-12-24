/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione della memoria cache con politica di espulsone LRU
 * @author              Simone Tassotti
 * @date                22/12/2021
 */

#include "FileStorageServer.h"


/**
 * @brief                           Legge il contenuto del configFile del server e lo traduce in una struttura in memoria principale
 * @fun                             readConfigFile
 * @param configPathname            Pathname del config file
 * @return                          Ritorna la struttura delle impostazioni del server; in caso di errore ritorna NULL [setta errno]
 */
LRU_Memory* readConfigFile(const char *configPathname) {
    /** Variabili **/
    char *buffer = NULL, *commento = NULL, *opt = NULL;
    int error = 0;
    long valueOpt = -1;
    FILE *file = NULL;
    LRU_Memory *serverMemory = NULL;

    /** Controllo parametri **/
    if(configPathname == NULL) { errno = EINVAL; return NULL; }
    if((file = fopen(configPathname, "r")) == NULL) { errno = ENOENT; return NULL; }

    /** Lettura del file **/
    if((serverMemory = (LRU_Memory *) malloc(sizeof(LRU_Memory))) == NULL) { error = errno; fclose(file); errno = error; return NULL; }
    if((buffer = (char *) calloc(MAX_BUFFER_LEN, sizeof(char))) == NULL) { error = errno; fclose(file); free(serverMemory); errno = error; return NULL; }
    memset(serverMemory, 0, sizeof(LRU_Memory));
    while((memset(buffer, 0, MAX_BUFFER_LEN*sizeof(char)), fgets(buffer, MAX_BUFFER_LEN, file)) != NULL) {

        if(strnlen(buffer, MAX_BUFFER_LEN) == 2) continue;

        if((commento = strchr(buffer, '#')) != NULL) {  // Se trovo un commento scarto tutto quello che c'e' dopo
            size_t lenCommento = strnlen(commento, MAX_BUFFER_LEN);
            memset(commento, 0, lenCommento*sizeof(char));
            continue;
        }

        // Imposto il numero di thread worker sempre "attivi"
        if((serverMemory->numeroThreadWorker == 0) && (strstr(buffer, "numeroThreadWorker") != NULL) && ((opt = strrchr(buffer, '=')) != NULL) && ((valueOpt = isNumber(opt+1)) != -1)) { serverMemory->numeroThreadWorker = valueOpt; continue; }
        else if(serverMemory->numeroThreadWorker == 0) { serverMemory->numeroThreadWorker = DEFAULT_NUMERO_THREAD_WORKER; }

        // Imposto il numero massimo di MB che il server puo' imagazzinare
        if((serverMemory->maxMB == 0) && (strstr(buffer, "maxMB") != NULL) && ((opt = strrchr(buffer, '=')) != NULL) && ((valueOpt = isNumber(opt+1)) != -1)) { serverMemory->maxMB = valueOpt; continue; }
        else if(serverMemory->maxMB == 0) serverMemory->maxMB = DEFAULT_MAX_MB;

        // Imposto il canale di comunicazione socket
        if((serverMemory->socket == NULL) && (serverMemory->socket = (char *) calloc(MAX_BUFFER_LEN, sizeof(char))) == NULL) { error = errno; free(buffer); fclose(file); free(serverMemory); errno = error; return NULL; }
        if(((opt = strstr(buffer, "socket")) != NULL) && ((opt = strrchr(opt, '=')) != NULL) && (strstr(opt+1, ".sk") != NULL)) {
            strncpy(serverMemory->socket, opt+1, strnlen(opt+1, MAX_BUFFER_LEN)+1);
            (serverMemory->socket)[strnlen(opt+1, MAX_BUFFER_LEN)+1] = '\0';
            continue;
        }
        else if(serverMemory->socket == NULL) strncpy(serverMemory->socket, DEFAULT_SOCKET, strnlen(DEFAULT_SOCKET, MAX_BUFFER_LEN)+1);

        // Imposto il numero massimo di file che si possono caricare
        if((serverMemory->maxNumeroFileCaricabili == 0) && (strstr(buffer, "maxNumeroFileCaricabili") != NULL) && ((opt = strrchr(buffer, '=')) != NULL) && ((valueOpt = isNumber(opt+1)) != -1)) { serverMemory->maxNumeroFileCaricabili = valueOpt; continue; }
        else if(serverMemory->maxNumeroFileCaricabili == 0) serverMemory->maxNumeroFileCaricabili = DEFUALT_MAX_NUMERO_FILE;

        // Imposto il numero massimo di utenti connessi al server
        if((serverMemory->maxUtentiConnessi == 0) && (strstr(buffer, "maxUtentiConnessi") != NULL) && ((opt = strrchr(buffer, '=')) != NULL) && ((valueOpt = isNumber(opt+1)) != -1)) { serverMemory->maxUtentiConnessi = valueOpt; continue; }
        else if(serverMemory->maxUtentiConnessi == 0) serverMemory->maxUtentiConnessi = DEFUALT_MAX_NUMERO_FILE;

        // Imposto il numero massimo di utenti che possono aprire un file contemporaneamente
        if((serverMemory->maxUtentiPerFile == 0) && (strstr(buffer, "maxUtentiPerFile") != NULL) && ((opt = strrchr(buffer, '=')) != NULL) && ((valueOpt = isNumber(opt+1)) != -1)) { serverMemory->maxUtentiPerFile = valueOpt; continue; }
        else if(serverMemory->maxUtentiPerFile == 0) serverMemory->maxUtentiPerFile = DEFUALT_MAX_NUMERO_UTENTI;
    }
    free(buffer);
    fclose(file);

    /** 2° Step: Inizializzazione della struttura dati **/


    return serverMemory;
}


void deleteLRU(LRU_Memory **serverMemory) {
    free((*serverMemory)->socket);
    free(*serverMemory);
}
