#!/bin/bash
# Test script for URL-encoded POST commands
# This tests backward compatibility with the original Motion POST API

HOST="${1:-http://localhost:8080}"
CAMID="${2:-1}"

echo "Testing URL-encoded POST commands on $HOST for camera $CAMID"
echo "================================================================"
echo ""

# Function to test a command
test_command() {
    local cmd=$1
    local desc=$2
    echo -n "Testing $desc ($cmd)... "
    response=$(curl -s -w "\n%{http_code}" -d "command=$cmd&camid=$CAMID" "$HOST")
    http_code=$(echo "$response" | tail -n 1)
    body=$(echo "$response" | head -n -1)

    if [ "$http_code" = "200" ]; then
        echo "✓ OK (HTTP $http_code)"
        if [ -n "$body" ]; then
            echo "   Response: $body"
        fi
    else
        echo "✗ FAILED (HTTP $http_code)"
        echo "   Response: $body"
    fi
    echo ""
}

# Test basic commands
test_command "snapshot" "Snapshot"
sleep 1

test_command "pause_on" "Pause detection"
sleep 1

test_command "pause_off" "Resume detection"
sleep 1

test_command "eventstart" "Event start"
sleep 1

test_command "eventend" "Event end"
sleep 1

# Test PTZ commands (if camera supports it)
echo "Testing PTZ commands (may fail if camera doesn't support PTZ):"
test_command "pan_left" "Pan left"
test_command "pan_right" "Pan right"
test_command "tilt_up" "Tilt up"
test_command "tilt_down" "Tilt down"

# Test that JSON API still works
echo ""
echo "Verifying JSON API still works:"
echo "================================================================"
echo -n "Testing GET /1/api/config... "
json_response=$(curl -s -w "\n%{http_code}" -H "Content-Type: application/json" "$HOST/1/api/config")
json_http_code=$(echo "$json_response" | tail -n 1)
if [ "$json_http_code" = "200" ]; then
    echo "✓ OK (HTTP $json_http_code)"
else
    echo "✗ FAILED (HTTP $json_http_code)"
fi

echo ""
echo "================================================================"
echo "Tests complete!"
