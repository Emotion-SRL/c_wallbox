# c_wallbox

Client C della wallbox. Gira su un Onion Omega2 (OpenWrt 18.06). Legge lo stato
dal micro via seriale e lo manda al server via WebSocket (`emotion-projects.eu:443`).
Riceve comandi dal server (es. `setCurrent 16`) e li gira al micro.

La ricarica la gestisce il micro: se il client si ferma, la wallbox continua a caricare.

## 1. Preparazione

- **SSH**: Dropbear non supporta chiavi ed25519. Usa una chiave RSA, in
  `/etc/dropbear/authorized_keys`. La password di root non è nel repo.
- **WiFi**: collegati all'access point dell'Omega (`Omega-3932`) e apri
  `http://192.168.3.1/wifi/`. La pagina va installata prima: vedi
  `onion-wifi-page/README.md`.

## 2. Credenziali

Crea `/root/credentials.txt` sulla wallbox:

```
<username>
<password>
```

Sono le credenziali dell'account su `emotion-projects.eu`. Lo script le legge e poi
**cancella il file**. Il file è in `.gitignore`: non va mai committato.

## 3. Provisioning

Copia `wb_provision.sh` sulla wallbox ed eseguilo come root:

```sh
sh wb_provision.sh
```

Lo script:

1. chiede se la wallbox è Barton o Emotion;
2. installa `curl`;
3. legge il MAC di `br-wlan`, fa login e chiama `wallbox/assign-serial/`;
   scrive il seriale in `/root/serial_number.txt`;
4. crea `/root/start_wallbox.sh`: un loop che riavvia il client 5s dopo ogni uscita;
5. crea `/root/update_wallbox.sh`: scarica il binario dall'ultima release pubblica
   di `Emotion-SRL/c_wallbox` (nessun token);
6. aggiunge a `/etc/rc.local` `gpioctl dirout-low 11` e l'avvio del client
   ritardato di 60s;
7. aggiunge al cron l'update (04:00 e 17:00) e il watchdog (ogni minuto);
8. imposta il fuso orario CET/CEST, scarica il binario e avvia il client.

Lo script si può rieseguire. Se il MAC è già registrato, `assign-serial` fallisce
e lo script riusa il `serial_number.txt` esistente.

### File sulla wallbox (`/root`)

| File | Cosa |
|---|---|
| `ws_client` | il binario |
| `serial_number.txt` | il seriale, letto all'avvio del client |
| `start_wallbox.sh`, `update_wallbox.sh`, `watchdog.sh` | avvio, update, watchdog |
| `ws-log.txt` | log del client |
| `restart.log`, `update.log` | log dei riavvii e degli update |

## 4. Cambiare il seriale

Il client legge il seriale **solo all'avvio**, da `/root/serial_number.txt`.
Non c'è sincronizzazione col backend: se cambi il seriale solo sul backend, la
wallbox continua a mandare quello vecchio e risulta offline.

Quindi cambialo in tutti e due i posti. Prima sul backend, poi sulla wallbox:

```sh
echo -n "NUOVO_SERIAL" > /root/serial_number.txt
killall ws_client   # il loop lo riavvia in 5s
```

Rieseguire `wb_provision.sh` non assegna un seriale nuovo: il MAC è già registrato.

## 5. Legacy: `primo_script.sh`

Il vecchio flusso, basato su Python e sul repo privato `Emotion-SRL/wallbox_smart`.
Sostituito da `wb_provision.sh`. Vale solo per le wallbox ancora su quel flusso.

Il token GitHub non è più scritto nello script. Va messo nella **riga 3** di
`/root/credentials.txt`:

```
<username>
<password>
<github_token>
```

Se manca, lo script esce con un errore.

Lo script cancella `credentials.txt`, ma il token **resta in chiaro** sulla wallbox in:

- `/root/wallbox_smart/src/reboot_with_git_pull.sh`;
- `/root/wallbox_smart/.git/config`, nell'URL di `origin` (il clone usa
  `https://<token>@github.com/...`).

Lo script stampa anche username e password a schermo. Usa un token con permessi
di sola lettura sul repo.
