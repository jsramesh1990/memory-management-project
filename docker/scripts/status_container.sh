#!/bin/bash
# status_container.sh - Check container status
# Version: 1.0.0

set -e

CONTAINER_NAME="rk3568-ddr-dev"

echo "=== Container Status: $CONTAINER_NAME ==="
echo ""

# Check if container exists
if docker ps -a --format '{{.Names}}' | grep -q "^$CONTAINER_NAME$"; then
    echo "Status: Container exists"
    
    # Check if running
    if docker ps --format '{{.Names}}' | grep -q "^$CONTAINER_NAME$"; then
        echo "State: Running"
        
        # Show container info
        echo ""
        echo "Container Info:"
        docker inspect "$CONTAINER_NAME" --format='{{.State.Status}}' | sed 's/^/  /'
        docker inspect "$CONTAINER_NAME" --format='{{.Config.Image}}' | sed 's/^/  Image: /'
        docker inspect "$CONTAINER_NAME" --format='{{.HostConfig.Memory}}' | sed 's/^/  Memory: /'
        
        # Show resource usage
        echo ""
        echo "Resource Usage:"
        docker stats --no-stream "$CONTAINER_NAME"
    else
        echo "State: Stopped"
    fi
else
    echo "Status: Container does not exist"
fi

echo ""
echo "=== Volumes ==="
docker volume ls --filter "name=rk3568_ddr"
