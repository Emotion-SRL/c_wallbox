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
# NOTE: the binary is always fetched from GitHub ($BIN_URL), both the first time
#       (step 8) and on later updates (cron -> update_wallbox.sh). No scp needed.

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
RC_LOCAL="/etc/rc.local"

# GitHub private release config for the binary download/auto-update.
# TO BE FILLED: owner/repo from the release asset link, token created on GitHub.
# GH_TOKEN is a Personal Access Token (classic) with the 'repo' scope.
GH_OWNER="Emotion-SRL"
GH_REPO="c_wallbox"
GH_TOKEN="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"          # GitHub Personal Access Token, ghp_... (fill on the Onion)

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
else
    echo "assign-serial request failed. HTTP: $HTTP_STATUS_CODE"
    echo "JSON response: $RESPONSE_BODY"
    exit 1
fi

# Write the serial into the working dir (same folder as the binary: the client reads it via relative path)
echo -n "$SERIAL_NUMBER" > "$SERIAL_FILE"
echo "Serial number written to $SERIAL_FILE"

### ---------------------------------------------------------------------------
### 5a. START SCRIPT
### ---------------------------------------------------------------------------
# cd into the working dir so the client finds serial_number.txt via relative path
cat << EOF > "$START_SCRIPT"
#!/bin/sh
cd "$WB_DIR"
./"$BIN_NAME"
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

# GitHub private release config
GH_OWNER="$GH_OWNER"
GH_REPO="$GH_REPO"
GH_TOKEN="$GH_TOKEN"

if [ -z "\$GH_OWNER" ] || [ -z "\$GH_REPO" ] || [ -z "\$GH_TOKEN" ]; then
    echo "\$(date) GitHub config not set, skipping update" >> "\$LOG"
    exit 1
fi

API="https://api.github.com/repos/\$GH_OWNER/\$GH_REPO/releases/latest"

# asset API url of the latest release (assumes the binary is the only asset)
ASSET_URL=\$(curl -fsSL -H "Authorization: token \$GH_TOKEN" -H "Accept: application/vnd.github+json" "\$API" | grep -o 'https://api.github.com/repos/[^"]*/releases/assets/[0-9]*' | head -1)

if [ -z "\$ASSET_URL" ]; then
    echo "\$(date) no release asset found" >> "\$LOG"
    exit 1
fi

# download the asset (octet-stream is required for private release assets)
if ! curl -fsSL -H "Authorization: token \$GH_TOKEN" -H "Accept: application/octet-stream" "\$ASSET_URL" -o "\$TMP_BIN"; then
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

# restart the process (safe to interrupt: the wallbox charges on its own)
killall "\$BIN_NAME" 2>/dev/null
"\$WB_DIR/start_wallbox.sh" &
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

# Start the C client at boot
if ! grep -q "$START_SCRIPT &" "$RC_LOCAL"; then
    sed -i "/exit 0/i $START_SCRIPT &" "$RC_LOCAL"
    echo "Added client startup to $RC_LOCAL"
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
# Same mechanism as the auto-update: fetch the binary now and start it.
echo "Fetching the binary for the first time..."
"$UPDATE_SCRIPT"

echo ""
echo "============================================================"
echo "Provisioning done."
if [ -z "$GH_OWNER" ] || [ -z "$GH_REPO" ] || [ -z "$GH_TOKEN" ]; then
    echo "  WARNING: GitHub config is empty, the binary was NOT downloaded."
    echo "  Fill GH_OWNER/GH_REPO/GH_TOKEN at the top of this script and re-run,"
    echo "  or run $UPDATE_SCRIPT manually after filling them in it."
else
    echo "  Binary at: $BIN_PATH"
fi
echo "  Manual start for testing: $START_SCRIPT"
echo "============================================================"
