# Analisi flusso server — Wallbox

## Struttura del progetto

Il progetto è un OCPP manager scritto in Rust che supporta più protocolli:
- `ocpp1.6`
- `ocpp2.0.1`
- `wallbox` (protocollo proprietario)
- `modbus`

Il binario si avvia in due modalità: `client` o `server`. In modalità server espone due porte:
- Porta WebSocket: accetta connessioni dai device
- Porta HTTP (Axum): API REST per inviare comandi ai device connessi

---

## Flusso server — Wallbox

### Fase 1: TCP Accept → WebSocket Handshake
**`ServerManager::start()` → `on_connect()`** (`server_manager.rs:34-111`)

- Ascolta su `0.0.0.0:<port>` con `TcpListener`
- Per ogni connessione in ingresso, esegue il WS upgrade con `accept_hdr_async`
- Dall'header `Sec-WebSocket-Protocol` estrae il **protocollo** (es. `"wallbox"`)
- Dall'URI estrae il **connection_id** (ultimo segmento del path, es. `/ABC123` → `ABC123`)
- Riflette il protocollo nell'header della risposta (`101 Switching Protocols`)
- Se il protocollo è vuoto o non valido, chiude con `CloseCode::Unsupported`

**Cosa serve:** il client deve connettersi a `ws://<host>:<port>/<connection_id>` con header `Sec-WebSocket-Protocol: wallbox`

---

### Fase 2: Router Selection e Connection Spawn
**`create_router()` → `Connection::new()`** (`routers/mod.rs:48`, `connection.rs:29`)

- `create_router("wallbox")` istanzia `WallboxRouter::new()`, che costruisce un `Requester` puntato su `https://emotion-projects.eu/api`
- `Connection::new()` fa il split del WS stream (sender/receiver), crea un oneshot channel per la chiusura, registra il `close_callback` che rimuove la connessione dalla mappa
- Spawna un task Tokio per il loop di ricezione messaggi
- Inserisce la connessione nella mappa `clients: HashMap<String, Arc<Mutex<Connection>>>`

---

### Fase 3: Ricezione messaggi dal wallbox
**`Connection::on_message()` → `WallboxRouter::route()`** (`connection.rs:73`, `wallbox_router.rs:94`)

- Il loop legge ogni frame WS testuale dal receiver
- Passa il messaggio a `WallboxRouter::route()`
- Il router deserializza in `WallboxMessage`:

```rust
pub struct WallboxMessage {
    pub serial_number: Option<String>,
    pub status: Option<WallboxStatus>,  // oppure "state" (legacy)
    pub password: String,               // obbligatorio
    pub ip_address: Option<String>,     // presente solo al boot
}
```

- `parse_state()` normalizza `state` → `status` (gestione campo legacy)
- **Discriminante boot vs realtime:** se `ip_address` è presente → boot notification

**Cosa serve:** il wallbox deve inviare JSON valido con almeno `password`. Per il boot deve includere `ip_address`.

---

### Fase 4a: Boot Notification
**`WallboxRouter::send_boot_notification()`** (`wallbox_router.rs:127`)

- `PATCH https://emotion-projects.eu/api/wallbox/` con il body del messaggio
- Se la PATCH fallisce → risponde al wallbox con `CloseFrame { code: CloseCode::Invalid }`
- Se OK → nessuna risposta al wallbox (`None`)

**Cosa serve:** il backend deve accettare una PATCH su `/wallbox/` con i campi del wallbox (serial_number, ip_address, password, status)

---

### Fase 4b: Realtime Notification
**`WallboxRouter::send_realtime_notification()`** (`wallbox_router.rs:137`)

- `POST https://emotion-projects.eu/api/wallbox/realtime/` con il body del messaggio
- Se la POST fallisce → logga l'errore, ma non risponde nulla al wallbox
- Se OK → nessuna risposta al wallbox (`None`)

**Cosa serve:** il backend deve accettare POST su `/wallbox/realtime/` con serial_number e status

---

### Fase 5: Invio comandi al wallbox (direzione inversa)
**`POST /tower-ctl/{conn_id}` → `send_ocpp()` → `Connection::send()`** (`server_api_handler.rs:169`, `connection.rs:59`)

- L'API HTTP riceve `{ "protocol": "wallbox", "data": { "command": "setCurrent", "value": "16" } }`
- `WallboxRequestPayload::into_message()` costruisce il testo: `"setCurrent 16"` (o solo `"stop"` se senza value)
- `Connection::send()` chiama `router.prepare_to_send()` (no-op per wallbox) poi invia il frame WS
- Se l'invio fallisce → triggera il close signal

**Cosa serve:** il `conn_id` deve corrispondere esattamente alla stringa usata nell'URI di connessione

---

### Fase 6: Disconnessione
**`on_message` loop exit → `Router::on_disconnect()` → `close_callback`** (`connection.rs:113`)

- Alla chiusura WS (qualunque causa), chiama `WallboxRouter::on_disconnect()` — attualmente **non implementato** (usa il default no-op del trait `Router`)
- Poi chiama il `close_callback` che rimuove la connessione dalla mappa `clients`

---

## Gap / punti di attenzione

| # | Problema | File |
|---|----------|------|
| 1 | `WallboxRouter::on_disconnect()` non implementato — nessun segnale offline al backend | `wallbox_router.rs` |
| 2 | `password` obbligatoria nel body ma nessuna validazione/autenticazione reale | `wallbox_router.rs:31` |
| 3 | URL backend hardcoded come `static` — non configurabile via env var | `wallbox_router.rs:13` |
| 4 | Se la boot notification PATCH fallisce, il wallbox viene disconnesso ma la rimozione dalla mappa `clients` avviene solo quando arriva il close frame | `wallbox_router.rs:130` |
| 5 | Se due device si connettono con lo stesso `conn_id`, il secondo sovrascrive il primo nella mappa senza warning | `server_manager.rs:99` |

---

## connection_id: identificatore interno vs DB

**Il `connection_id` estratto dall'URI è puramente interno all'ocpp_manager.** Non viene mai inviato al backend — le chiamate HTTP usano solo i campi nel body JSON (`serial_number`, `status`, ecc.).

Il backend identifica il device tramite il campo `serial_number` nel JSON, non tramite il path WebSocket.

Conseguenze:
- Un wallbox può connettersi con qualsiasi stringa nell'URI — l'handshake non valida nulla
- Il layer di trasporto (ocpp_manager) è disaccoppiato dal dominio: tratta il `conn_id` come stringa opaca
- Per inviare comandi via `POST /tower-ctl/{conn_id}` è necessario sapere quale stringa il device ha usato alla connessione

**Convenzione consigliata:** usare il serial number del device come `conn_id` — in questo modo è deterministico e non serve tenere stato aggiuntivo per ritrovare la connessione. Non è un vincolo del codice, ma una scelta di design da documentare e rispettare.
