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

From a Windows checkout with `core.autocrlf=true` the files have CRLF line
endings, and the CGI will not run on the Omega with them. Strip them while copying:
`tr -d '\r' < www/cgi-bin/wifi-setup | ssh root@<omega-ip> 'cat > /www/cgi-bin/wifi-setup && chmod 755 /www/cgi-bin/wifi-setup'`.

`/www` lives on the overlay, so it survives reboots; a firmware upgrade may
overwrite it.

## How it works

- `GET  /cgi-bin/wifi-setup?action=info`  current client connection (ssid, ip) and the Omega's own AP (ap_ssid, ap_ip)
- `GET  /cgi-bin/wifi-setup?action=scan`  nearby networks (`ubus call onion wifi-scan`)
- `POST /cgi-bin/wifi-setup?action=join`  JSON `{"ssid": b64, "password": b64, "enc": "psk2|psk|wep|none"}`

`join` validates every field (base64 charset, lengths, fixed encryption list),
then runs `wifisetup add` and `wifisetup priority ... top`, keeps at most 5
saved networks and starts a background job that runs `wifi reload` and waits
up to 2 minutes for the new network. Once the Omega is on it, the other saved
client networks are forgotten and the WiFi is reloaded once more, because
`ap_client` keeps the list it was started with and would otherwise still fall
back to them. If the new network never comes up the old ones stay, as fallback. The
Omega's own access point is a separate `wifi-iface` and is never touched.
Nothing user-supplied is `eval`'d.

The page knows whether it was opened through the Omega's AP or through the
network the Omega is a client of. In the second case, changing network takes
the page's connection away: instead of an error it shows a "check" message
saying where to find the Omega (the new network, or its AP).

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
