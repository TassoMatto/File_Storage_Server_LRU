#!/bin/bash

# Test n°3: avvio client ininterrottamente per 30 sec

# Controllo gli argomenti
if [ ! $# = 1 ]; then
  echo "Devi specificare il PID del server"
  exit 22;
fi

# Per uscire dal ciclo di creazione dei client
# uso la variabile speciale SECONDS e imposto un
# timer di 30 secondi prima di uccidere il server
breakLoop=$[$SECONDS+30]
echo $1
index=1
while [ $SECONDS -le $breakLoop ]; do
    ./client -f ./socket.sk \
    -W /mnt/c/Users/Simone/CLionProjects/File_Storage_Server_LRU/test3/extra/${index} -D ./test3/kick \
    -l /mnt/c/Users/Simone/CLionProjects/File_Storage_Server_LRU/test3/extra/${index} \
    -R -d ./test3/save  \
    -u /mnt/c/Users/Simone/CLionProjects/File_Storage_Server_LRU/test3/extra/${index} \
    -c /mnt/c/Users/Simone/CLionProjects/File_Storage_Server_LRU/test3/extra/${index} &
    index=$[$index+1]
  if [ $[$index%10] = 1 ]; then
      sleep 1
  fi
done

# Mando il segnale di arresto al server
kill -2 $1

exit 0