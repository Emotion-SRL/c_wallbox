# Onion Omega2 – standalone WiFi page

A small page to change the WiFi network the Omega2 connects to, without going
through the OnionOS setup wizard (which only runs once, on first boot).

Tested on OpenWrt 18.06 (OnionOS 0.3.3), `uhttpd`, Dropbear SSH.

## Files

| Path                     | Install to on the Omega           | Mode |
|--------------------------|-----------------------------------|------|
| `www/wifi/index.html`    | `/www/wifi/index.html`            | 644  |
| `www/cgi-bin/wifi-setup` | `/www/cgi-bin/wifi-setup`         | 755  |

The page is then reachable at `http://<omega-ip>/wifi/`, or at
`http://192.168.3.1/wifi/` when connected to the Omega's own access point.

## Install

Dropbear in this OpenWrt release does not support ed25519 keys: use an RSA key
(`/etc/dropbear/authorized_keys`).

```sh
ssh root@<omega-ip> 'mkdir -p /www/wifi'
ssh root@<omega-ip> 'cat > /www/wifi/index.html'        < www/wifi/index.html
ssh root@<omega-ip> 'cat > /www/cgi-bin/wifi-setup && chmod 755 /www/cgi-bin/wifi-setup' < www/cgi-bin/wifi-setup
```

`/www` lives on the overlay, so it survives reboots; a firmware upgrade may
overwrite it.

## How it works

- `GET  /cgi-bin/wifi-setup?action=info`  current client connection (ssid, ip)
- `GET  /cgi-bin/wifi-setup?action=scan`  nearby networks (`ubus call onion wifi-scan`)
- `POST /cgi-bin/wifi-setup?action=join`  JSON `{"ssid": b64, "password": b64, "enc": "psk2|psk|wep|none"}`

`join` validates every field (base64 charset, lengths, fixed encryption list),
then runs `wifisetup add` and `wifisetup priority ... top`, keeps at most 5
saved networks (previous ones stay as fallback) and finally runs
`wifi reload` in the background. Nothing user-supplied is `eval`'d.

## Known limitations

- **No authentication.** Anyone who can reach the page can change the network.
  The access point password is the factory default unless changed.
- **The scan is flaky.** About 1 request in 5–20 comes back from `uhttpd` with
  an empty body (the `wifi-scan` command itself is fine when run directly). The
  page retries quietly and always offers manual entry of the network name.
- **Single radio.** Applying a new network restarts the access point: clients
  are dropped for a few seconds. If the new network does not work, reconnect to
  the Omega's own AP (`Omega-3932`) and open `192.168.3.1/wifi/`.
- Once the Omega joins a network with a different subnet it is no longer
  reachable from the previous one.
