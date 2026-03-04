#!/bin/bash

PORT="/dev/ttyACM0"
PORT_NAME=$(basename "$PORT")
BAUD_RATE="9600" # Change this if your microcontroller uses a different baud rate

# Helper function to write to the serial port safely
send_to_serial() {
    local payload="$1"
    # Check if the device exists and is a character device
    if [ -c "$PORT" ]; then
        # Configure the port (avoids blocking issues)
        stty -F "$PORT" "$BAUD_RATE" raw -echo 2>/dev/null
        # Send the payload appended with a newline
        printf "%s\n" "$payload" > "$PORT"
    fi
}

# Helper function to format and send the time
send_time() {
    local time_str
    time_str=$(date +"T%H%M%S")
    send_to_serial "$time_str"
}

echo "Starting serial hardware monitor on $PORT..."

# --- 1a. Trigger upon startup ---
send_time

# --- 1b. Trigger every hour (on the hour) ---
(
    while true; do
        # Calculate seconds remaining until the exact top of the next hour
        sleep_time=$(( 3600 - $(date +%s) % 3600 ))
        sleep "$sleep_time"
        send_time
    done
) &

# --- 1c. Trigger upon device connection ---
(
    # Monitor kernel events specifically for this TTY device being added
    stdbuf -oL udevadm monitor --kernel --subsystem-match=tty | \
    grep --line-buffered "add.*/$PORT_NAME" | \
    while read -r line; do
        # Wait 2 seconds to let the device (like an Arduino) finish its auto-reset
        sleep 2 
        send_time
    done
) &

# --- 2. Listen for PipeWire volume changes ---
(
    # pactl subscribe hooks into the PipeWire-Pulse event bus seamlessly
    stdbuf -oL pactl subscribe | \
    grep --line-buffered "Event 'change' on sink" | \
    while read -r line; do
        # Fetch current volume using PipeWire's native tool (Output e.g., "Volume: 0.50")
        vol_raw=$(wpctl get-volume @DEFAULT_AUDIO_SINK@)
        
        # Extract the float, multiply by 100, and format as an integer
        # LC_ALL=C ensures awk handles decimal points correctly regardless of system locale
        vol_int=$(echo "$vol_raw" | LC_ALL=C awk '{printf "%.0f", $2 * 100}')
        
        # Format to VNNN (zero-padded to 3 digits)
        vol_str=$(printf "V%03d" "$vol_int")
        send_to_serial "$vol_str"
    done
) &

# Keep the main script alive while background tasks run
wait