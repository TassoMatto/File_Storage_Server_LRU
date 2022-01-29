#!/bin/bash

# Test n°2: avvio 5 client che fanno alcune richieste al server

# Controllo gli argomenti
if [ ! $# = 1 ]; then
  echo "Devi specificare il PID del server"
  exit 22;
fi

# Attendo 4 secondi prima di avviare i client
sleep 4
for (( index = 1; index <= 5; index++ )); do
  ./client -f socket.sk -p -w ./test2/${index} -D ./test2/tmp &
done

sleep 2
# Mando il segnale di arresto al server
kill -1 $1

exit 0