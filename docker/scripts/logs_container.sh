#!/bin/bash
# logs_container.sh - View container logs
# Version: 1.0.0

set -e

CONTAINER_NAME="rk3568-ddr-dev"

echo "Showing logs for container: $CONTAINER_NAME"

if ! docker ps --format '{{.Names}}' | grep -q "^$CONTAINER_NAME$"; then
    echo "Error: Container '$CONTAINER_NAME' is not running."
    exit 1
fi

docker logs -f "$CONTAINER_NAME"
