#!/bin/bash


### CHIEDERE SE LA WALLBOX È DI BARTON ###
read -p "La wallbox da scriptare è di Barton? (y/n): " response
if [[ "$response" == "y" || "$response" == "Y" ]]; then
    isBarton="barton"
else
    isBarton="emotion"
fi
echo "La variabile isBarton è stata impostata a: $isBarton"


### INSTALLARE PYTHON ###

# Aggiornare il gestore dei pacchetti
echo "Aggiornamento del gestore dei pacchetti..."
opkg update

# Installare python3-light
# echo "Installazione di python3-light..."
# opkg install python3-light

# Verificare se python3 è già installato
if ! command -v python3 &> /dev/null; then
    echo "Installazione della versione completa di python3..."
    opkg install python3

    # Verificare se l'installazione è riuscita
    if [ $? -ne 0 ]; then
        echo "Errore durante l'installazione di python3."
        exit 1
    fi
else
    echo "python3 è già installato."
fi

### INSTALLARE PIP ###
# Installare pip
echo "Installazione di pip..."
opkg update
opkg install python3-pip

# Verificare se l'installazione è riuscita
if [ $? -ne 0 ]; then
    echo "Errore durante l'installazione di pip."
    exit 1
fi


### RISOLVERE L'ERRORE DI SETUPTOOLS ###
# Funzione per confrontare le versioni
version_gt() {
    [ "$(printf '%s\n' "$@" | sort -V | head -n 1)" != "$1" ]
}

# Ottieni la versione attuale di setuptools
CURRENT_VERSION=$(pip3 show setuptools | grep Version | awk '{print $2}')
echo "Versione attuale di setuptools: $CURRENT_VERSION"

# Ottieni l'ultima versione disponibile di setuptools da PyPI
LATEST_VERSION=$(curl -s https://pypi.org/pypi/setuptools/json | grep -o '"version":"[^"]*"' | sed 's/"version":"\([^"]*\)"/\1/')
echo "Ultima versione disponibile di setuptools: $LATEST_VERSION"

# Confronta le versioni e aggiorna se necessario
if version_gt "$LATEST_VERSION" "$CURRENT_VERSION"; then
    echo "Aggiornamento di setuptools..."
    pip3 install --upgrade setuptools

    # Verificare se l'aggiornamento è riuscito
    if [ $? -ne 0 ]; then
        echo "Errore durante l'aggiornamento di setuptools."
        exit 1
    else
        echo "Aggiornamento di setuptools completato con successo."
    fi
else
    echo "setuptools è già aggiornato all'ultima versione."
fi

echo "Packages update..."
opkg update

echo "Installazione di moduli Python..."
echo "Installazione del modulo asyncio"
pip3 install asyncio

echo "Installazione del modulo datetime"  
pip3 install datetime

echo "Installazione del modulo json" 
pip3 install json

echo "Installazione del modulo os" 
pip3 install os

echo "Installazione del modulo select"
pip3 install select

echo "Installazione del modulo socket"
pip3 install socket

echo "Installazione del modulo subprocess"
pip3 install subprocess

echo "Installazione del modulo sys"
pip3 install sys

echo "Installazione del modulo time"
pip3 install time

echo "Installazione del modulo pytz"
pip3 install pytz

echo "Installazione del modulo pyserial"
pip3 install pyserial

echo "Installazione del modulo websockets"
pip3 install websockets

echo "Installazione del modulo requests"
pip3 install requests

echo "Installazione moduli completata!"



### INSTALLARE GIT ###

# Verificare se git è già installato
if ! command -v git &> /dev/null; then
    echo "Installazione di git..."
    opkg install git git-http ca-bundle
    opkg update
else
    echo "git è già installato."
fi

### CLONARE LA CARTELLA DEL PROGETTO DA GITHUB ###

# Definire il token e l'URL del repository
GITHUB_TOKEN=$(sed -n '3p' /root/credentials.txt | tr -d '\r\n')
if [ -z "$GITHUB_TOKEN" ]; then
    echo "Manca il token GitHub (terza riga di /root/credentials.txt)."
    exit 1
fi
REPO_URL="https://$GITHUB_TOKEN@github.com/Emotion-SRL/wallbox_smart.git"
CLONE_DIR="wallbox_smart"

echo "Clonando il repository..."
git clone $REPO_URL $CLONE_DIR

# Verificare se la cartella del repository esiste già
# if [ ! -d "$CLONE_DIR" ]; then
#     echo "Clonando il repository..."
#     git clone $REPO_URL $CLONE_DIR

#     # Verificare se il clone è riuscito
#     if [ $? -ne 0 ]; then
#         echo "Errore durante la clonazione del repository."
#         exit 1
#     fi
# else
#     echo "La cartella $CLONE_DIR esiste già. Clonazione saltata."
# fi
# Navigare nella directory del progetto clonato
cd $CLONE_DIR

echo "Progetto clonato e configurato con successo."

echo "Impostazione dei permessi per la cartella $CLONE_DIR..."
chmod -R 777 .


# Sostituire la cartella OnionOS in /www con quella in wallbox_smart solo se esiste

echo "Sostituzione della cartella OnionOS in /www con quella in wallbox_smart..."
rm -rf /www/OnionOS/
mv /root/wallbox_smart/OnionOS /www/
echo "Cartella OnionOS sostituita con successo."


# Recuperare il MAC address dell'Onion Omega 2
MAC_ADDRESS=$(ifconfig br-wlan | grep 'HWaddr' | awk '{print $5}')
echo "Il MAC address dell'Onion Omega 2 è: $MAC_ADDRESS"
# Ripulire il MAC address dai :
CLEAN_MAC_ADDRESS=$(echo $MAC_ADDRESS | tr -d ':')
echo "Il MAC address ripulito è: $CLEAN_MAC_ADDRESS"

# Aggiorna l'elenco dei pacchetti
opkg update

# Installa curl
opkg install curl

# Verifica l'installazione di curl
curl --version

# Leggi le credenziali dal file /root/credentials.txt
# Leggi le credenziali dal file /root/credentials.txt e rimuovi eventuali spazi o caratteri indesiderati
USERNAME=$(sed -n '1p' /root/credentials.txt | tr -d '\r' | tr -d '\n')
PASSWORD=$(sed -n '2p' /root/credentials.txt | tr -d '\r' | tr -d '\n')

# Cancella il file /root/credentials.txt
rm /root/credentials.txt

# Stampa le credenziali per il debug (opzionale)
echo "USERNAME: $USERNAME"
echo "PASSWORD: $PASSWORD"

# Definisci l'URL dell'API di login
API_LOGIN="https://emotion-projects.eu/api/auth/login/"


# Effettua la richiesta POST all'API di login per ottenere il token
LOGIN_RESPONSE=$(curl -s -X POST "$API_LOGIN" \
    -H "Content-Type: application/json" \
    -d "{\"username\": \"$USERNAME\", \"password\": \"$PASSWORD\"}")

# Stampa la risposta per il debug
echo "Risposta login: $LOGIN_RESPONSE"

# Estrai il token di autenticazione dalla risposta JSON senza usare jq
AUTH_TOKEN=$(echo "$LOGIN_RESPONSE" | grep -o '"key":"[^"]*"' | sed 's/"key":"\([^"]*\)"/\1/')

# Verifica se il token è stato recuperato correttamente
if [ -n "$AUTH_TOKEN" ]; then
    echo "Token di autenticazione recuperato con successo: $AUTH_TOKEN"
else
    echo "Errore durante il recupero del token di autenticazione"
    exit 1
fi

echo "Token di autenticazione recuperato con successo: $AUTH_TOKEN"


# Effettua la richiesta POST all'API per ottenere il SERIAL_NUMBER
RESPONSE=$(curl -s -X POST "https://emotion-projects.eu/api/wallbox/assign-serial/" \
    -H "Authorization: Token $AUTH_TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"mac_address\": \"$CLEAN_MAC_ADDRESS\", \"company\": \"$isBarton\"}" \
    -w "\nHTTP_STATUS_CODE:%{http_code}")

# Estrai il codice di stato HTTP dalla risposta
HTTP_STATUS_CODE=$(echo "$RESPONSE" | grep "HTTP_STATUS_CODE" | cut -d':' -f2)

# Estrai il corpo della risposta
RESPONSE_BODY=$(echo "$RESPONSE" | sed -e 's/HTTP_STATUS_CODE:.*//g')

# Stampa il codice di stato HTTP e il corpo della risposta per il debug
echo "Codice di stato HTTP: $HTTP_STATUS_CODE"
echo "Corpo della risposta: $RESPONSE_BODY"

# Verifica se la richiesta è stata eseguita con successo (codice di stato 200 o 201)
if [ "$HTTP_STATUS_CODE" -eq 200 ] || [ "$HTTP_STATUS_CODE" -eq 201 ]; then
    # Estrai il SERIAL_NUMBER dalla risposta JSON senza usare jq
    SERIAL_NUMBER=$(echo "$RESPONSE_BODY" | grep -o '"serial_number":"[^"]*"' | sed 's/"serial_number":"\([^"]*\)"/\1/')

    # Verifica se il SERIAL_NUMBER è stato recuperato correttamente
    if [ -n "$SERIAL_NUMBER" ]; then
        echo "SERIAL_NUMBER recuperato con successo: $SERIAL_NUMBER"
    else
        echo "Errore durante il recupero del SERIAL_NUMBER"
        echo "Risposta JSON: $RESPONSE_BODY"
    fi
else
    echo "Errore durante la richiesta all'API. Codice di stato HTTP: $HTTP_STATUS_CODE"
    echo "Risposta JSON: $RESPONSE_BODY"
fi

# Stampa il valore di SERIAL_NUMBER per il debug
echo "Valore di SERIAL_NUMBER: $SERIAL_NUMBER"



# Scrivere il serial number nel file wallbox_smart/serial_number.txt, cancellando eventuali scritte e spazi vuoti
echo -n "$SERIAL_NUMBER" > /root/wallbox_smart/serial_number.txt

# Verificare se il comando echo è stato eseguito correttamente
if [ $? -eq 0 ]; then
    echo "Serial number scritto con successo in wallbox_smart/serial_number.txt"
else
    echo "Errore durante la scrittura del serial number in wallbox_smart/serial_number.txt"
fi




# Creare lo script di avvio per onionG.py
cat << 'EOF' > /root/wallbox_smart/src/start_onion.sh
#!/bin/sh
python3 /root/wallbox_smart/src/onion_G.py
EOF

# Verificare se lo script di avvio è stato creato correttamente
if [ $? -eq 0 ]; then
    echo "Script di avvio creato con successo: /root/wallbox_smart/src/start_onion.sh"
else
    echo "Errore durante la creazione dello script di avvio: /root/wallbox_smart/src/start_onion.sh"
fi

# Rendere eseguibile lo script di avvio
chmod +x /root/wallbox_smart/src/start_onion.sh

# Verificare se il comando chmod è stato eseguito correttamente
if [ $? -eq 0 ]; then
    echo "Script di avvio reso eseguibile: /root/wallbox_smart/src/start_onion.sh"
else
    echo "Errore durante la modifica dei permessi dello script di avvio: /root/wallbox_smart/src/start_onion.sh"
fi

RC_LOCAL="/etc/rc.local"

# Aggiungere la riga gpioctl dirout-low 11 a /etc/rc.local
if ! grep -q "gpioctl dirout-low 11" "$RC_LOCAL"; then
    echo "Aggiungendo 'gpioctl dirout-low 11' a $RC_LOCAL..."
    sed -i '/exit 0/i gpioctl dirout-low 11' "$RC_LOCAL"
    echo "Riga aggiunta con successo."
else
    echo "La riga 'gpioctl dirout-low 11' è già presente in $RC_LOCAL."
fi

# Aggiungere lo script di avvio a /etc/rc.local
if ! grep -q "/root/wallbox_smart/src/start_onion.sh &" "$RC_LOCAL"; then
    echo "Aggiungendo lo script di avvio a $RC_LOCAL..."
    sed -i '/exit 0/i /root/wallbox_smart/src/start_onion.sh &' "$RC_LOCAL"
    if [ $? -eq 0 ]; then
        echo "Script di avvio aggiunto a $RC_LOCAL"
    else
        echo "Errore durante l'aggiunta dello script di avvio a $RC_LOCAL"
    fi
else
    echo "Lo script di avvio è già presente in $RC_LOCAL"
fi

# Creare lo script per il reboot con git pull
cat << 'EOF' > /root/wallbox_smart/src/reboot_with_git_pull.sh

# Definizione del token e dell'URL del repository
GITHUB_TOKEN="__GITHUB_TOKEN__"
REPO_URL="https://$GITHUB_TOKEN@github.com/Emotion-SRL/wallbox_smart.git"

# Definisci il percorso del file di log
LOG_FILE="/root/update_log.txt"

# Naviga nella directory del repository
cd /root/wallbox_smart

# Forza il reset dello stato locale per sovrascrivere le modifiche
echo "Reset locale allo stato dell'ultimo commit..." | tee -a $LOG_FILE
git reset --hard HEAD 2>&1 | tee -a $LOG_FILE
reset_status=$?

# Assicurati di essere sul ramo 'main'
echo "Cambio al ramo main..." | tee -a $LOG_FILE
git checkout main 2>&1 | tee -a $LOG_FILE
checkout_status=$?

# Esegui il git pull forzato dal ramo 'main' e salva l'output sul file di log
echo "Esecuzione di git pull forzato dal ramo main..." | tee -a $LOG_FILE
git pull origin main 2>&1 | tee -a $LOG_FILE
pull_status=$?

# Controlla se il pull è stato eseguito correttamente
if [ $reset_status -eq 0 ] && [ $checkout_status -eq 0 ] && [ $pull_status -eq 0 ]; then
    echo "git pull eseguito con successo." | tee -a $LOG_FILE
    echo "Esecuzione di reboot..." | tee -a $LOG_FILE
    reboot
else
    echo "Errore durante l'esecuzione di git pull." | tee -a $LOG_FILE
fi
EOF
# Inserisce il token reale nel reboot script (l'heredoc è quotato, quindi non espande le variabili)
sed -i "s|__GITHUB_TOKEN__|$GITHUB_TOKEN|" /root/wallbox_smart/src/reboot_with_git_pull.sh

# Verificare se lo script di reboot è stato creato correttamente
if [ $? -eq 0 ]; then
    echo "Script di reboot creato con successo: /root/wallbox_smart/src/reboot_with_git_pull.sh"
else
    echo "Errore durante la creazione dello script di reboot: /root/wallbox_smart/src/reboot_with_git_pull.sh"
fi

# Rendere eseguibile lo script di reboot
chmod +x /root/wallbox_smart/src/reboot_with_git_pull.sh

# Verificare se il comando chmod è stato eseguito correttamente
if [ $? -eq 0 ]; then
    echo "Script di reboot reso eseguibile: /root/wallbox_smart/src/reboot_with_git_pull.sh"
else
    echo "Errore durante la modifica dei permessi dello script di reboot: /root/wallbox_smart/src/reboot_with_git_pull.sh"
fi

#Modifica TimeZone
rm /etc/TZ
echo "CET-1CEST,M3.5.0,M10.5.0/3" > /etc/TZ
echo "Time Zone Aggiornato!" 
date

# Aggiungere i cron job per il reboot programmato
(crontab -l ; echo "0 17 * * * /root/wallbox_smart/src/reboot_with_git_pull.sh") | crontab -
if [ $? -eq 0 ]; then
    echo "Cron job aggiunto per il reboot alle 17:00"
else
    echo "Errore durante l'aggiunta del cron job per il reboot alle 17:00"
fi

(crontab -l ; echo "0 4 * * * /root/wallbox_smart/src/reboot_with_git_pull.sh") | crontab -
if [ $? -eq 0 ]; then
    echo "Cron job aggiunto per il reboot alle 04:00"
else
    echo "Errore durante l'aggiunta del cron job per il reboot alle 04:00"
fi

# da fare rebooting
reboot -f
