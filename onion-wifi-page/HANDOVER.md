# Passaggio di consegne – pagina WiFi per l'Onion Omega2

Data: 2026-10-08. Documento per chi riprende il lavoro. Dice **cosa c'è sul
dispositivo, cosa è stato cambiato, cosa è stato scoperto e cosa è ancora
aperto**. Per installare i file vedi `README.md`.

## 1. Il dispositivo

- Onion **Omega2**, hostname `Omega-3932`, **OpenWrt 18.06-SNAPSHOT**
  (kernel 4.14.81, mips), OnionOS 0.3.3. Shell BusyBox `ash` (niente `bash`,
  `file`, `hostname`). Orologio in UTC.
- SSH: **Dropbear**. Questa versione **non supporta chiavi ed25519**: serve una
  chiave **RSA**. Le chiavi di root stanno in `/etc/dropbear/authorized_keys`
  (non in `~/.ssh/`).
- Web: `uhttpd` sulla porta 80, document root `/www`, CGI in `/cgi-bin`,
  `max_requests=3`, `script_timeout=60`. Altri servizi: shellinabox (4200),
  mosquitto (1883/9001), dnsmasq.
- Rete (una sola radio, modalità `apsta`):
  - **Access point** `Omega-3932` su `br-wlan`, indirizzo `192.168.3.1/24`
    (password = quella di fabbrica, mai cambiata).
  - **Client** `apcli0`, DHCP, collegato alla rete dell'ufficio
    (`192.168.1.95` al momento della stesura; cambia se cambia la rete).
  - `eth0` configurata in DHCP ma senza indirizzo (non usata).
- Le credenziali (password di root) **non sono in questo repository**:
  chiedile a Carlo.

## 2. Perché esiste questa pagina

La pagina originale di OnionOS (`/www/OnionOS`, app Vue già compilata) ha un
wizard di primo avvio che include la scelta della WiFi. Il wizard compare solo se
`uci get onion.console.setup` vale `0`; a fine wizard viene messo a `1` e non
si rivede più. L'app **Preferences**, che contiene la stessa schermata WiFi, è
commentata nell'elenco delle app (`src/store/index.js`, assente dal bundle).
Quindi dopo il primo setup non c'è modo di cambiare WiFi dall'interfaccia.
Inoltre il wizard richiede il login root del dispositivo.

Non abbiamo toccato OnionOS. Abbiamo aggiunto una pagina separata.

Per leggere i sorgenti originali di OnionOS: i file
`/www/OnionOS/static/js/*.map` contengono `sourcesContent`.

## 3. Cosa abbiamo messo sul dispositivo

| Cosa | Dove |
|---|---|
| Pagina | `/www/wifi/index.html` → `http://<ip>/wifi/` |
| CGI | `/www/cgi-bin/wifi-setup` (755) |
| Home → pagina WiFi | `/www/index.html` (prima rimandava a `/OnionOS`, che resta raggiungibile a mano) |
| Originale di `/www/index.html` | `/root/index.html.onionos` |
| Chiave RSA di Carlo | unica riga in `/etc/dropbear/authorized_keys` |

Pulizia del 2026-10-09: da `authorized_keys` sono state tolte le righe doppie e
le chiavi ed25519 (inutili su questo Dropbear); resta solo la RSA di Carlo.
Cancellato anche il backup `/root/backup-pre-wifipage-20261008-163149.tar.gz`
(conteneva `/etc/config` con le password WiFi).

Reti WiFi salvate oggi: solo `EmotionWiFi`. Per vederle senza stampare le
password: `uci show wireless | grep -E "=wifi-config$|@wifi-config.*\.ssid="`.

## 4. Come funziona (importante per modificarla)

### 4.1 Come l'Omega sceglie la WiFi

- `wifisetup` (`/usr/bin/wifisetup`) salva le reti in una **lista ordinata**
  `wireless.@wifi-config[N]`. **L'ordine è la priorità**: l'indice 0 è la più
  alta.
- `wifisetup add` aggiunge in coda (priorità più bassa). Per far scegliere la
  nuova rete bisogna spostarla in cima: `wifisetup priority -ssid X -move top`.
- La lista viene letta da `/lib/netifd/wireless/ralink.sh`
  (`ralink_setup_sta`), che la passa a `/sbin/ap_client`.
- `uci commit` non basta: serve `wifi reload` per riapplicare.
- Con `-b64` `wifisetup` si aspetta ssid, password, encr e move **in base64**.

### 4.2 Il CGI `wifi-setup`

- `?action=info` → `{ssid, ip, preferred, connected, ap_ssid, ap_ip}` (da
  `iwinfo apcli0`, `ip` e, per l'AP, dalla `wifi-iface` con `mode='ap'` in uci).
- `?action=scan` → output di `ubus call onion wifi-scan '{"device":"ra0"}'`
  (la scansione dura 4–8 s).
- `?action=join` (POST, JSON con ssid/password in base64 e `enc`): valida
  tutto (charset base64, lunghezze, `enc` in lista fissa), poi `wifisetup add`,
  `wifisetup priority … top`, taglia la lista a 5 reti e avvia un job in
  background (`apply_and_forget`) che:
  - lancia `wifi reload` **dopo 3 s** (così la risposta HTTP parte prima che
    l'AP si riavvii);
  - controlla ogni 3 s, per 2 min, se `apcli0` è sulla nuova rete con un IP;
  - quando lo è, **dimentica le altre reti salvate** (cancella le
    `wifi-config` dalla seconda in poi). L'AP dell'Omega è una `wifi-iface`
    e non viene toccato. Se la nuova rete non arriva, le vecchie restano come
    riserva.
  - dopo averle cancellate, attende 5 s e lancia **un secondo `wifi reload`**:
    `ap_client` viene avviato da `ralink.sh` con la lista delle reti e la
    tiene in memoria (si vede con `ps w | grep ap_client`), quindi senza
    reload continuerebbe a ricollegarsi da solo alle reti dimenticate.
    Verificato sul dispositivo il 2026-10-09. Il reload stacca anche l'AP per
    qualche secondo (radio unica); `ap_client` è registrato in netifd
    (`wireless_add_process`), quindi non si può riavviare da solo.
  Il token in `/tmp/wifi-setup.pending` fa sì che, se arriva un altro cambio,
  il job precedente si fermi (e salti il secondo reload).
- Un lock `/tmp/wifi-setup.lock` evita due cambi contemporanei (scade dopo 2 min);
  il job lo prende anche lui mentre cancella le reti.
- **Non usare** il percorso standard `ubus call onion wifi-setup`: `rpcd`
  costruisce il comando con `eval`, quindi input libero è pericoloso. Il CGI
  chiama `wifisetup` direttamente.

### 4.3 La pagina

Form principale (nome rete, password, "Mostra"), sotto l'elenco delle reti
trovate, **già aperto**: la ricerca parte all'apertura della pagina, con 2
ritentativi silenziosi; si può sempre scrivere il nome a mano. In fondo
"Opzioni avanzate" con il tipo di sicurezza. Dopo l'invio fa polling di `?action=info`
ogni 3 s fino a 90 s (ogni richiesta scade dopo 5 s). Se la connessione cade
perché l'AP si riavvia, continua a riprovare.

La pagina distingue come è stata aperta (`location.hostname` uguale a `ap_ip`
o no):
- **dall'AP dell'Omega**: se a fine attesa non risponde più, mostra "Controlla
  il collegamento" (ricollegati all'AP, poi "Controlla di nuovo");
- **dalla rete a cui è collegato l'Omega** (casa/ufficio): cambiando rete
  l'Omega sparisce da lì. Dopo 15 s senza risposta mostra "Controlla il
  collegamento" con dove cercarlo (nuova rete o AP), in giallo e non come
  errore, e intanto continua a controllare.

"Non riuscito" (rosso) compare solo se l'Omega risponde ma non è sulla nuova
rete (es. password sbagliata e ritorno alla rete precedente).

## 5. Cose da sapere prima di toccare il dispositivo

1. **La connessione SSH passa dalla WiFi client.** Cambiare rete la interrompe.
   Per provare il flusso senza rischi, ricollegati alla **stessa** rete.
2. Se la nuova rete non funziona, l'**AP `Omega-3932` resta attivo**: collegati
   lì e apri `http://192.168.3.1/wifi/`.
3. Dopo il cambio su una rete con altra numerazione l'Omega non è più
   raggiungibile dalla vecchia rete.
4. Un aggiornamento firmware Onion può sovrascrivere `/www`.
5. **Nessuna autenticazione** sulla pagina: chiunque la raggiunga cambia la
   WiFi. Mitigazioni possibili: PIN nel CGI, cambiare la password dell'AP.

## 6. Problema aperto: la scansione a volte risponde vuota

Sintomo: 1 richiesta su 5–20 a `?action=scan` torna `HTTP 200`, header corretti
(`Content-Type: application/json`, `Transfer-Encoding: chunked`) ma **corpo di 0
byte**. La pagina mostrava "Errore nella ricerca".

Cosa è stato verificato:
- Il comando `ubus call onion wifi-scan` da shell funziona **sempre** (10/10
  prove, 4–8 s, `rc=0`, JSON valido).
- Con una traccia temporanea dentro il CGI, nei casi falliti **non compare la
  riga di log** che il CGI scrive subito dopo la scansione: lo script si ferma
  prima di arrivarci, oppure l'output non arriva a `uhttpd`. Nei casi riusciti
  la riga c'è sempre.
- Memoria libera ok (~69 MB liberi), nessun errore in `dmesg`.
- Una chiamata sola alla volta fallisce comunque; la concorrenza di
  `info + scan` insieme non l'ha peggiorato nei test fatti.
- Tracce rimosse; il CGI nel repo è la versione senza debug.

Non ancora verificato (idee, in ordine di costo):
1. `uhttpd`: `max_requests=3`, `script_timeout`, versione `2018-06-26`; provare
   a cambiarli e/o aggiornare, e guardare `logread` durante i fallimenti.
2. Eseguire la scansione **in background** e salvarla in `/tmp/wifi-scan.json`:
   il CGI risponde subito con l'ultimo risultato e la pagina lo rilegge. Evita
   di tenere aperta una richiesta per 5+ s.
3. Controllare se il blocco coincide con `ap_client`/netifd che ricontrolla
   l'associazione (nel log compare `ap_client … assoc: yes` ogni ~8 s).

Non è bloccante: la pagina funziona anche senza scansione.

## 7. Non fatto (decisioni rinviate)

- **PIN** di protezione: rinviato su richiesta.
- **Cambio password dell'AP**: rinviato su richiesta.
- **Indirizzo `192.168.1.4` raggiungibile dall'AP**: idea valutata, **non
  applicata**. L'approccio sicuro è un indirizzo *aggiuntivo* `192.168.1.4/32`
  solo su `br-wlan` (alias uci), **senza** cambiare `192.168.3.1`, perché la
  rete dell'ufficio è già `192.168.1.x`. Attenzione all'ARP: `arp_ignore` è già
  `1` su tutte le interfacce, quindi l'Omega non risponderebbe per `.4` sul
  lato ufficio. Va verificato che nessun altro dispositivo usi `.4`, e provato
  da un telefono collegato all'AP.

## 8. Come tornare indietro

```sh
# sul dispositivo
rm -rf /www/wifi /www/cgi-bin/wifi-setup
cp /root/index.html.onionos /www/index.html     # la home torna a OnionOS
```

Il backup completo di `/www` e `/etc/config` fatto prima della pagina è stato
cancellato: non c'è più un ripristino "tutto com'era".

## 9. Comandi utili

```sh
ssh root@<ip> 'iwinfo apcli0 info; ip -4 -o addr'          # stato client
ssh root@<ip> 'uci show wireless | grep wifi-config'        # reti salvate (+ password!)
ssh root@<ip> 'ubus call onion wifi-scan "{\"device\":\"ra0\"}"'
ssh root@<ip> 'logread | grep -E "ap_client|netifd"'
ssh root@<ip> 'gpioctl get 11'                              # esempio lettura GPIO
curl -s "http://<ip>/cgi-bin/wifi-setup?action=info"
```

`uci show wireless` stampa le password WiFi in chiaro: non incollarlo in chat o
ticket senza oscurarle. Lo stesso vale per `ps`: `ap_client` riceve nomi **e
password** delle reti come argomenti.
