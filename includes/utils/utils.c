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