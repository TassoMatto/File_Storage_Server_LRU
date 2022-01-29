#!/bin/bash

# Test n°1: avvio client che fa alcune richieste al server

# Controllo gli argomenti
if [ ! $# = 1 ]; then
  echo "Devi specificare il PID del server"
  exit 0;
fi

# Attendo 4 secondi prima di avviare i client
sleep 4
./client -f ./socket.sk -p -t 200 -w ./test1/client/send -W ./test1/client/test1_file1.txt -D ./test1/tmp -R n=3 -d ./test1/tmp1 -r $PWD/test1/client/test1_file1.txt -d ./test1/tmp \
-l $PWD/test1/client/test1_file1.txt -u $PWD/test1/client/test1_file1.txt \
-c $PWD/test1/client/send/1

sleep 2
# Mando il segnale di arresto al server
kill -1 $1

exit 0