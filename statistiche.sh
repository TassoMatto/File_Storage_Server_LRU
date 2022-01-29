#!/bin/bash

# ------------------------------------------------------------
#
# Script che effettua il parsing del file di log del server
# e riporta le statistiche analizzate
#
# Simone Tassotti
# 25/01/2022
#
# ------------------------------------------------------------


# Controllo che il file esista
if [ ! -r "FileStorageServer.log" ]; then
  echo "File di log non presente"
  exit 22;
fi

# Punto il file con fd = 3
exec 3<"FileStorageServer.log"

nLocks=0
nOpenlocks=0
nUnlock=0
nClose=0
nMiss=0
countConnected=0
maxConnected=0
maxNThread=0
maxDimBytes=0
nowByte=0
maxNumFile=0
nowFile=0
nRead=0
dimRead=0
nWrite=0
dimWrite=0
while read -u 3 line; do

  # Numero di read fatte
  echo $line | grep -q -e "Spedisco dati al client"
  if [ $? = 0 ]; then
      nWrite=$[$nWrite+1]
  fi

  # Numero di write fatte
  echo $line | grep -q -e "Ricevuto dati dal client"
  if [ $? = 0 ]; then
      nRead=$[$nRead+1]
  fi

  # Somma delle dimensioni delle write
  saveB=$(echo $line | grep -e "INVIATI" | cut -d ' ' -f 13 | cut -dB -f 1)
  if [ "$saveB" != "" ]; then
      dimWrite=$(echo "$dimWrite+$saveB" | bc -q)
  fi

  # Somma delle dimensione delle read fatte
  saveB=$(echo $line | grep -e "RICEVUTI" | cut -d ' ' -f 16 | cut -dB -f 1)
  if [ "$saveB" != "" ]; then
      dimRead=$(echo "$dimRead+$saveB" | bc -q)
  fi

  # Calcolo numero richieste fatte da ogni thread
  echo $line | grep -o ".* RICHIESTA: .*" | grep -q -v "ESITO"
  if [ $? -eq 0 ]; then
    index=$(echo $line | cut -f 2 -d '[' | cut -f 1 -d ']' | cut -f 2 -d ' ')
    if [ $maxNThread -lt $index ]; then
        maxNThread=$[$index+1]
    fi
    clientRequest[$index]=$[clientRequest[$index]+1]
  fi

  # Numero di close
  echo $line | grep -q -e "closeFile.*eseguita correttamente"
  if [ $? -eq 0 ]; then
      nClose=$[$nClose+1]
  fi

  # Numero di lock
  echo $line | grep -q -e " lockFile.*eseguita correttamente"
  if [ $? -eq 0 ]; then
        nLocks=$[$nLocks+1]
  fi

  # Numero di unlock
  echo $line | grep -q -e "unlockFile.*eseguita correttamente"
  if [ $? -eq 0 ]; then
        nUnlock=$[$nUnlock+1]
  fi

  # Numero di openLock
  echo $line | grep -q -e "openFile.* MODALITA': O_CREATE | O_LOCK - ESITO: eseguita correttamente"
  if [ $? = 0 ]; then
        nOpenlocks=$[$nOpenlocks+1]
  fi

  # Numero di Capacity Miss
  echo $line | grep -q -e ".*ATTENZIONE.* File"
  if [ $? = 0 ]; then
      nowFile=$[$nowFile-1]
      nMiss=$[$nMiss+1]
  fi

  # Numero di connessioni massimo contemporanee
  echo $line | grep -q -e "Accept(): client con fd:"
  if [ $? = 0 ]; then
    countConnected=$[$countConnected+1]
    if [ $maxConnected -lt $countConnected ]; then
      maxConnected=$countConnected
    fi
  fi
  echo $line | grep -q -e "Chiusura della connessione con il client"
  if [ $? = 0 ]; then
    countConnected=$[$countConnected-1]
  fi

  # Massima capacità raggiunta dal server
  saveB=$(echo $line | grep -e "RIMOSSI" | cut -d ' ' -f 13 | cut -dB -f 1)
  if [ "$saveB" != "" ]; then
    nowByte=$(echo "$nowByte-$saveB" | bc -q)
  fi
  saveB=$(echo $line | grep -e "SCRITTI" | cut -d ' ' -f 16 | cut -dB -f 1)
  if [ "$saveB" != "" ]; then
    nowByte=$(echo "$nowByte+$saveB" | bc -q)
  fi
  if [ $maxDimBytes -lt $nowByte ]; then
      maxDimBytes=$nowByte
  fi

  # Numero massimo di file raggiunto dal server
  echo $line | grep -q -e "writeFile.*ESITO: eseguita correttamente"
  if [ $? = 0 ]; then
    nowFile=$[$nowFile+1]
  fi
  echo $line | grep -q -e "removeFile.*ESITO: eseguita correttamente"
  if [ $? = 0 ]; then
    nowFile=$[$nowFile-1]
  fi
  if [ $maxNumFile -lt $nowFile ]; then
    maxNumFile=$nowFile
  fi
done


# Output statistiche
echo "Richieste di write:                   $nWrite"
echo "Dimensione media richieste:           $(echo "scale=4;$dimWrite/$nWrite" | bc -q | awk '{printf "%.4f\n", $0}')B"
echo "Richieste di read:                    $nWrite"
echo "Dimensione media richieste:           $(echo "scale=4;$dimRead/$nRead" | bc -q | awk '{printf "%.4f\n", $0}')B"
echo "Numero di open-lock:                  $nOpenlocks"
echo "Numero di close:                      $nClose"
echo "Numero di lock:                       $nLocks"
echo "Numero di unlock:                     $nUnlock"
echo "Dimensione massima:                   $(echo "scale=4;$maxDimBytes/1000000" | bc -q | awk '{printf "%.4f\n", $0}')MB"
echo "Numero massimo di file memorizzato:   $maxNumFile"
echo "Numero di memoryMiss:                 $nMiss"
echo "Richieste servite da ogni thread:"
for (( i = 1; i < maxNThread; i++ )); do
  echo "Thread $i:                            ${clientRequest[$i]}"
done
echo "Picco di connessioni contemporanee:   $maxConnected"


exit 0