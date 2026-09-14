#!/bin/sh
#
# Wallbox provisioning for the C client (replaces primo_script.sh / the Python system).
#
# Flow:
#   1. ask company (Barton/Emotion)
#   2. install curl
#   3. create the working dir /root/wb_client (executable + serial_number.txt together)
#   4. serial number provisioning (login + assign-serial, IDENTICAL to the old flow)
#   5. generate start_wallbox.sh and update_wallbox.sh
#   6. autostart in /etc/rc.local (+ gpioctl dirout-low 11)
#   7. cron job for the executable auto-update
#   8. first download of the binary via update_wallbox.sh, then start it
#
# NOTE: the binary is always fetched from the public GitHub release ($BIN_URL), both the
#       first time (step 8) and on later updates (cron -> update_wallbox.sh). No scp, no token.

set -u

### ---------------------------------------------------------------------------
### CONFIG
### ---------------------------------------------------------------------------
WB_DIR="/root"
BIN_NAME="ws_client"
BIN_PATH="$WB_DIR/$BIN_NAME"
SERIAL_FILE="$WB_DIR/serial_number.txt"
START_SCRIPT="$WB_DIR/start_wallbox.sh"
UPDATE_SCRIPT="$WB_DIR/update_wallbox.sh"
WATCHDOG_SCRIPT="$WB_DIR/watchdog.sh"
RC_LOCAL="/etc/rc.local"

# Boot start delay (seconds): at a full power-on the OS services the client depends on
# (br-wlan up so the MAC read succeeds, DNS/route so the WSS connect resolves, and the
# serial micro finishing its own setup() so the first status read doesn't desync) are
# NOT ready the instant rc.local fires. Starting the client too early makes it miss the
# boot handshake -> the station stays OFFLINE even though the process is alive. So we
# delay the boot start by this much. This is boot-only; crash restarts stay fast (5s).
BOOT_DELAY=60
# The watchdog must not jump in during that window and start the loop early (which would
# defeat the delay), so it stays quiet until the box has been up longer than the delay.
# Set above BOOT_DELAY with a margin so rc.local's delayed loop is already up by then and
# the watchdog only ever acts as a crash-recovery net afterwards.
WATCHDOG_MIN_UPTIME=$((BOOT_DELAY + 30))

# GitHub PUBLIC release config for the binary download/auto-update.
# The repo is public, so no token is needed: the 'releases/latest/download/<asset>'
# URL is a stable redirect to the newest release's asset, downloadable anonymously.
GH_OWNER="Emotion-SRL"
GH_REPO="c_wallbox"
BIN_URL="https://github.com/$GH_OWNER/$GH_REPO/releases/latest/download/$BIN_NAME"

# Serial provisioning APIs (unchanged from the old script)
API_LOGIN="https://emotion-projects.eu/api/auth/login/"
API_ASSIGN="https://emotion-projects.eu/api/wallbox/assign-serial/"
CREDENTIALS_FILE="$WB_DIR/credentials.txt"

### ---------------------------------------------------------------------------
### 1. COMPANY
### ---------------------------------------------------------------------------
printf "La wallbox da scriptare è di Barton? (y/n): "
read response
if [ "$response" = "y" ] || [ "$response" = "Y" ]; then
    isBarton="barton"
else
    isBarton="emotion"
fi
echo "Company set to: $isBarton"

### ---------------------------------------------------------------------------
### 2. CURL
### ---------------------------------------------------------------------------
echo "Updating packages and installing curl..."
opkg update
if ! command -v curl >/dev/null 2>&1; then
    opkg install curl
fi
curl --version

### ---------------------------------------------------------------------------
### 3. WORKING DIR
### ---------------------------------------------------------------------------
# WB_DIR is /root (always present); mkdir -p is a no-op kept for safety in case
# WB_DIR is later pointed at a subfolder.
mkdir -p "$WB_DIR"

### ---------------------------------------------------------------------------
### 4. SERIAL NUMBER PROVISIONING (unchanged)
### ---------------------------------------------------------------------------
# MAC address from br-wlan, stripped of ':'
MAC_ADDRESS=$(ifconfig br-wlan | grep 'HWaddr' | awk '{print $5}')
CLEAN_MAC_ADDRESS=$(echo "$MAC_ADDRESS" | tr -d ':')
echo "MAC address: $MAC_ADDRESS -> $CLEAN_MAC_ADDRESS"

# Credentials from file (line 1: username, line 2: password), then deleted
if [ ! -f "$CREDENTIALS_FILE" ]; then
    echo "Credentials file not found: $CREDENTIALS_FILE"
    echo "Create it (line 1: username, line 2: password) before running this script."
    exit 1
fi
USERNAME=$(sed -n '1p' "$CREDENTIALS_FILE" | tr -d '\r' | tr -d '\n')
PASSWORD=$(sed -n '2p' "$CREDENTIALS_FILE" | tr -d '\r' | tr -d '\n')
rm -f "$CREDENTIALS_FILE"

if [ -z "$USERNAME" ] || [ -z "$PASSWORD" ]; then
    echo "Empty username or password in $CREDENTIALS_FILE"
    exit 1
fi

# Login -> auth token
echo "Logging in to $API_LOGIN ..."
LOGIN_RESPONSE=$(curl -s -X POST "$API_LOGIN" \
    -H "Content-Type: application/json" \
    -d "{\"username\": \"$USERNAME\", \"password\": \"$PASSWORD\"}")
AUTH_TOKEN=$(echo "$LOGIN_RESPONSE" | grep -o '"key":"[^"]*"' | sed 's/"key":"\([^"]*\)"/\1/')

if [ -z "$AUTH_TOKEN" ]; then
    echo "Failed to retrieve the auth token"
    echo "Login response: $LOGIN_RESPONSE"
    exit 1
fi
echo "Auth token retrieved."

# assign-serial -> serial number
echo "Requesting serial number (assign-serial) for MAC $CLEAN_MAC_ADDRESS ..."
RESPONSE=$(curl -s -X POST "$API_ASSIGN" \
    -H "Authorization: Token $AUTH_TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"mac_address\": \"$CLEAN_MAC_ADDRESS\", \"company\": \"$isBarton\"}" \
    -w "\nHTTP_STATUS_CODE:%{http_code}")
HTTP_STATUS_CODE=$(echo "$RESPONSE" | grep "HTTP_STATUS_CODE" | cut -d':' -f2)
RESPONSE_BODY=$(echo "$RESPONSE" | sed -e 's/HTTP_STATUS_CODE:.*//g')

if [ "$HTTP_STATUS_CODE" -eq 200 ] || [ "$HTTP_STATUS_CODE" -eq 201 ]; then
    SERIAL_NUMBER=$(echo "$RESPONSE_BODY" | grep -o '"serial_number":"[^"]*"' | sed 's/"serial_number":"\([^"]*\)"/\1/')
    if [ -z "$SERIAL_NUMBER" ]; then
        echo "Failed to retrieve the SERIAL_NUMBER"
        echo "JSON response: $RESPONSE_BODY"
        exit 1
    fi
    echo "SERIAL_NUMBER: $SERIAL_NUMBER"
    # Write the serial into the working dir (same folder as the binary: the client reads it via relative path)
    echo -n "$SERIAL_NUMBER" > "$SERIAL_FILE"
    echo "Serial number written to $SERIAL_FILE"
else
    # assign-serial failed: this is expected when the MAC is already provisioned (the
    # endpoint does a blind INSERT and hits the UNIQUE(password=MAC) constraint, without
    # returning a serial). An already-provisioned wallbox already has its serial_number.txt
    # on disk, so we just keep it and carry on with the rest of the setup.
    echo "assign-serial request failed. HTTP: $HTTP_STATUS_CODE"
    echo "JSON response: $RESPONSE_BODY"
    if [ -s "$SERIAL_FILE" ]; then
        echo "Already provisioned, reusing existing serial from $SERIAL_FILE: $(cat "$SERIAL_FILE")"
    else
        echo "No serial available (assign-serial failed and $SERIAL_FILE is missing), cannot continue."
        exit 1
    fi
fi

### ---------------------------------------------------------------------------
### 5a. START SCRIPT
### ---------------------------------------------------------------------------
# cd into the working dir so the client finds serial_number.txt via relative path
# (the client writes its own ws-log.txt in this dir, so no shell redirection here).
#
# Respawn loop: the C client exits (return ERR) on any failed init and can also die
# on a crash or a boot-race (e.g. br-wlan not up yet when rc.local fires, so the MAC
# read fails). Without a supervisor a dead client stays dead until the next reboot,
# which is exactly the "goes offline and never comes back" symptom. So we keep it
# alive here: whatever kills it, we wait a few seconds and start it again.
#
# NOTE: the WebSocket-level reconnection (WiFi drop/restore) is already handled inside
# the client itself; this loop is only for the process actually terminating.
cat << EOF > "$START_SCRIPT"
#!/bin/sh
cd "$WB_DIR"
while true; do
    ./"$BIN_NAME"
    echo "\$(date) $BIN_NAME exited (\$?), restarting in 5s" >> restart.log
    sleep 5
done
EOF
chmod +x "$START_SCRIPT"
echo "Start script created: $START_SCRIPT"

### ---------------------------------------------------------------------------
### 5b. AUTO-UPDATE SCRIPT
### ---------------------------------------------------------------------------
# Fetch the latest binary from a GitHub private release; replace and restart only
# if it changed. Charging is independent from the C client, so restarting is safe.
cat << EOF > "$UPDATE_SCRIPT"
#!/bin/sh
WB_DIR="$WB_DIR"
BIN_PATH="$BIN_PATH"
BIN_NAME="$BIN_NAME"
TMP_BIN="\$WB_DIR/\$BIN_NAME.new"
LOG="\$WB_DIR/update.log"

# GitHub public release: stable redirect to the latest release asset, no token/API needed
BIN_URL="$BIN_URL"

if [ -z "\$BIN_URL" ]; then
    echo "\$(date) BIN_URL not set, skipping update" >> "\$LOG"
    exit 1
fi

# download the binary (-L follows the releases/latest/download -> asset redirect)
if ! curl -fsSL "\$BIN_URL" -o "\$TMP_BIN"; then
    echo "\$(date) download failed" >> "\$LOG"
    rm -f "\$TMP_BIN"
    exit 1
fi

# if identical to the current binary, nothing to do
if [ -f "\$BIN_PATH" ] && cmp -s "\$TMP_BIN" "\$BIN_PATH"; then
    rm -f "\$TMP_BIN"
    exit 0
fi

# atomic replace + permissions
chmod +x "\$TMP_BIN"
mv "\$TMP_BIN" "\$BIN_PATH"
echo "\$(date) binary updated, restarting process" >> "\$LOG"

# restart the process (safe to interrupt: the wallbox charges on its own).
# We only kill the binary: start_wallbox.sh is a respawn loop, so it picks the
# new binary up by itself. Do NOT relaunch start_wallbox.sh here, or we'd spawn a
# second loop and end up with two ws_client processes fighting over the serial line.
killall "\$BIN_NAME" 2>/dev/null

# explicit success: killall exits non-zero when there's no process to kill (e.g. the
# loop is between restarts), which would otherwise make this script report failure
# even though the binary was updated fine.
exit 0
EOF
chmod +x "$UPDATE_SCRIPT"
echo "Update script created: $UPDATE_SCRIPT"

### ---------------------------------------------------------------------------
### 6. AUTOSTART in rc.local
### ---------------------------------------------------------------------------
# GPIO init (same as the old script)
if ! grep -q "gpioctl dirout-low 11" "$RC_LOCAL"; then
    sed -i '/exit 0/i gpioctl dirout-low 11' "$RC_LOCAL"
    echo "Added 'gpioctl dirout-low 11' to $RC_LOCAL"
fi

# Start the C client at boot, DELAYED, so the OS services it needs are up first (see
# BOOT_DELAY above). The subshell backgrounds the sleep+start so rc.local itself is not
# blocked and the rest of the boot proceeds normally.
BOOT_START_LINE="(sleep $BOOT_DELAY; $START_SCRIPT) &"
# migration: replace the old immediate-start line ('start_wallbox.sh &') if present, so
# re-provisioning an already-set-up box switches it to the delayed start instead of
# leaving both lines (which would start two loops).
if grep -q "^$START_SCRIPT &" "$RC_LOCAL"; then
    sed -i "\|^$START_SCRIPT &|d" "$RC_LOCAL"
    echo "Removed old immediate client startup from $RC_LOCAL"
fi
if ! grep -qF "$BOOT_START_LINE" "$RC_LOCAL"; then
    sed -i "/exit 0/i $BOOT_START_LINE" "$RC_LOCAL"
    echo "Added delayed client startup (${BOOT_DELAY}s) to $RC_LOCAL"
fi

### ---------------------------------------------------------------------------
### 7. AUTO-UPDATE CRON
### ---------------------------------------------------------------------------
# Check for a new binary twice a day (04:00 and 17:00), same cadence as the old script.
# Change the times/frequency below for a different interval.
if ! crontab -l 2>/dev/null | grep -q "$UPDATE_SCRIPT"; then
    (crontab -l 2>/dev/null; echo "0 4 * * * $UPDATE_SCRIPT") | crontab -
    (crontab -l 2>/dev/null; echo "0 17 * * * $UPDATE_SCRIPT") | crontab -
    echo "Auto-update cron added (04:00 and 17:00)"
fi

# Watchdog: every minute make sure the supervisor loop (start_wallbox.sh) is alive,
# and relaunch it if not (it can be killed, and rc.local only starts it once at boot).
#
# We detect the loop by PROCESS NAME, not by a pidfile: a recorded PID only tells us
# "some process with this PID exists", which is a false positive as soon as that PID is
# reused by an unrelated process (exactly the bug that left the box with no client).
# The check lives in a dedicated watchdog.sh so that 'ps | grep' is safe: the only
# process whose command line contains 'start_wallbox.sh' is the real loop (the grep
# itself is excluded by the '[s]' bracket trick, and watchdog.sh's own name doesn't
# match). An inline cron command couldn't do this — its own text contains the string.
cat << EOF > "$WATCHDOG_SCRIPT"
#!/bin/sh
# Stay quiet during the boot window: rc.local owns the (delayed) initial start, and
# jumping in here early would defeat that delay. Only act once the box has been up long
# enough, after which this is purely a crash-recovery net for the supervisor loop.
read up _ < /proc/uptime
up=\${up%.*}
[ "\$up" -lt $WATCHDOG_MIN_UPTIME ] && exit 0

# relaunch the supervisor loop if it isn't currently running
if ! ps w | grep -q '[s]tart_wallbox.sh'; then
    $START_SCRIPT >/dev/null 2>&1 &
fi
EOF
chmod +x "$WATCHDOG_SCRIPT"
echo "Watchdog script created: $WATCHDOG_SCRIPT"

# migration: drop any stale watchdog line from older versions of this script (they were
# pidfile-based, marked by 'wallbox.pid') so we don't end up with two watchdogs -> two loops
if crontab -l 2>/dev/null | grep -q "wallbox.pid"; then
    crontab -l 2>/dev/null | grep -v "wallbox.pid" | crontab -
    echo "Removed stale (pidfile-based) watchdog cron"
fi
if ! crontab -l 2>/dev/null | grep -q "$WATCHDOG_SCRIPT"; then
    (crontab -l 2>/dev/null; echo "* * * * * $WATCHDOG_SCRIPT") | crontab -
    echo "Watchdog cron added (every minute)"
fi

### ---------------------------------------------------------------------------
### TimeZone
### ---------------------------------------------------------------------------
rm -f /etc/TZ
echo "CET-1CEST,M3.5.0,M10.5.0/3" > /etc/TZ
echo "Time Zone updated"
date

### ---------------------------------------------------------------------------
### 8. FIRST DOWNLOAD + START
### ---------------------------------------------------------------------------
# Fetch the binary now. NOTE: update_wallbox.sh only downloads/replaces the binary;
# it no longer (re)launches the client — starting is the supervisor loop's job.
echo "Fetching the binary for the first time (may take 1-2 minutes, downloading silently)..."
if "$UPDATE_SCRIPT"; then
    echo "Binary fetch/update step done (see $WB_DIR/update.log for details)."
else
    echo "Binary fetch/update step failed (HTTP/token/release issue), see $WB_DIR/update.log."
fi

# Start the client now via the supervisor loop, but only if it isn't already up
# (e.g. a previous provisioning run left it running). setsid detaches it so it
# survives this provisioning script exiting; if setsid is missing, plain '&' is fine.
if [ -x "$BIN_PATH" ]; then
    if ps w | grep -q '[s]tart_wallbox.sh'; then
        echo "Client supervisor already running, leaving it as is."
    else
        # detach so the loop survives this provisioning script exiting
        if command -v setsid >/dev/null 2>&1; then
            setsid "$START_SCRIPT" >/dev/null 2>&1 &
        else
            "$START_SCRIPT" >/dev/null 2>&1 &
        fi
        echo "Client started (supervisor loop)."
    fi
else
    echo "Binary not present, client not started (will start at next boot / after a successful update)."
fi

echo ""
echo "============================================================"
echo "Provisioning done."
if [ -z "$BIN_URL" ]; then
    echo "  WARNING: BIN_URL is empty, the binary was NOT downloaded."
    echo "  Fill GH_OWNER/GH_REPO at the top of this script and re-run,"
    echo "  or run $UPDATE_SCRIPT manually after fixing it."
else
    echo "  Binary at: $BIN_PATH"
fi
echo "  Manual start for testing: $START_SCRIPT"
echo "============================================================"
