#!/bin/bash
# clean_container.sh - Clean up Docker container and volumes
# Version: 1.0.0

set -e

CONTAINER_NAME="rk3568-ddr-dev"

echo "Cleaning up Docker container: $CONTAINER_NAME"

# Stop container if running
if docker ps --format '{{.Names}}' | grep -q "^$CONTAINER_NAME$"; then
    echo "Stopping container..."
    docker stop "$CONTAINER_NAME"
fi

# Remove container
if docker ps -a --format '{{.Names}}' | grep -q "^$CONTAINER_NAME$"; then
    echo "Removing container..."
    docker rm "$CONTAINER_NAME"
fi

# Remove volumes (optional)
read -p "Remove associated volumes? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    docker volume rm rk3568_ddr_dev-home rk3568_ddr_dev-data 2>/dev/null || true
fi

echo "Cleanup complete!"
