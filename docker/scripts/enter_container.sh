#!/bin/bash
# enter_container.sh - Enter the RK3568 DDR Memory Manager Development Container
# Version: 1.0.0
# Author: Sebastian
# Description: This script provides easy access to the development container
#              with proper environment setup

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Default container name
CONTAINER_NAME="rk3568-ddr-dev"
DOCKER_COMPOSE_FILE="$PROJECT_ROOT/docker/docker-compose.yml"

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if Docker is installed
check_docker() {
    if ! command -v docker &> /dev/null; then
        print_error "Docker is not installed. Please install Docker first."
        exit 1
    fi
    
    if ! docker info &> /dev/null; then
        print_error "Docker daemon is not running. Please start Docker first."
        exit 1
    fi
}

# Function to check if container exists
container_exists() {
    docker ps -a --format '{{.Names}}' | grep -q "^$1$"
}

# Function to check if container is running
container_running() {
    docker ps --format '{{.Names}}' | grep -q "^$1$"
}

# Function to build the container if needed
build_container() {
    local compose_file="$1"
    local container="$2"
    
    print_info "Checking if container '$container' exists..."
    
    if ! container_exists "$container"; then
        print_warning "Container '$container' not found. Building it now..."
        
        if [ -f "$compose_file" ]; then
            print_info "Using docker-compose to build..."
            docker-compose -f "$compose_file" build dev
        else
            print_info "Using docker build directly..."
            docker build -t rk3568-ddr-dev -f "$PROJECT_ROOT/docker/Dockerfile.dev" "$PROJECT_ROOT/docker"
        fi
        
        if [ $? -eq 0 ]; then
            print_success "Container built successfully!"
        else
            print_error "Failed to build container."
            exit 1
        fi
    else
        print_info "Container '$container' already exists."
    fi
}

# Function to start container
start_container() {
    local compose_file="$1"
    local container="$2"
    
    if ! container_running "$container"; then
        print_info "Starting container '$container'..."
        
        if [ -f "$compose_file" ]; then
            docker-compose -f "$compose_file" up -d dev
        else
            docker start "$container" 2>/dev/null || true
        fi
        
        if [ $? -eq 0 ]; then
            print_success "Container started!"
        else
            print_error "Failed to start container."
            exit 1
        fi
    else
        print_info "Container '$container' is already running."
    fi
}

# Function to enter container with proper environment
enter_container() {
    local container="$1"
    
    print_info "Entering container '$container'..."
    print_info "Type 'exit' to leave the container"
    echo ""
    
    # Check if we're running in interactive terminal
    if [ -t 0 ] && [ -t 1 ]; then
        # Interactive mode
        docker exec -it "$container" bash -c "
            cd /workspace
            echo '=== RK3568 DDR Memory Manager Development Environment ==='
            echo 'Current directory: $(pwd)'
            echo 'Available commands: make, git, vim, python3, etc.'
            echo ''
            echo 'Quick commands:'
            echo '  make clean        - Clean build artifacts'
            echo '  make              - Build the project'
            echo '  make install      - Install the project'
            echo '  make test         - Run tests'
            echo '  make docs         - Build documentation'
            echo ''
            exec bash
        "
    else
        # Non-interactive mode
        docker exec -t "$container" bash -c "
            cd /workspace
            exec bash
        "
    fi
}

# Function to handle SSH agent forwarding
setup_ssh_agent() {
    local container="$1"
    
    if [ -n "$SSH_AUTH_SOCK" ] && [ -S "$SSH_AUTH_SOCK" ]; then
        print_info "Forwarding SSH agent to container..."
        docker exec -e SSH_AUTH_SOCK=/tmp/ssh-agent-sock "$container" \
            ssh-agent -a /tmp/ssh-agent-sock > /dev/null 2>&1 || true
        docker cp "$SSH_AUTH_SOCK" "$container:/tmp/ssh-agent-sock" 2>/dev/null || true
    fi
}

# Function to mount additional volumes
mount_volumes() {
    local container="$1"
    
    print_info "Setting up additional volume mounts..."
    
    # Mount SSH directory
    if [ -d "$HOME/.ssh" ]; then
        docker exec "$container" mkdir -p /home/developer/.ssh 2>/dev/null || true
        docker cp "$HOME/.ssh" "$container:/home/developer/" 2>/dev/null || true
        docker exec "$container" chmod 700 /home/developer/.ssh 2>/dev/null || true
        docker exec "$container" chmod 600 /home/developer/.ssh/* 2>/dev/null || true
    fi
    
    # Mount Git config
    if [ -f "$HOME/.gitconfig" ]; then
        docker cp "$HOME/.gitconfig" "$container:/home/developer/.gitconfig" 2>/dev/null || true
    fi
}

# Main function
main() {
    print_info "RK3568 DDR Memory Manager - Development Container"
    print_info "==================================================="
    echo ""
    
    # Check Docker
    check_docker
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -c|--container)
                CONTAINER_NAME="$2"
                shift 2
                ;;
            -b|--build)
                BUILD_CONTAINER=true
                shift
                ;;
            -h|--help)
                echo "Usage: $0 [OPTIONS]"
                echo "Options:"
                echo "  -c, --container NAME   Container name (default: rk3568-ddr-dev)"
                echo "  -b, --build            Force rebuild of container"
                echo "  -h, --help             Show this help message"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                echo "Use -h for help"
                exit 1
                ;;
        esac
    done
    
    # Build container if needed
    if [ "$BUILD_CONTAINER" = true ] || ! container_exists "$CONTAINER_NAME"; then
        build_container "$DOCKER_COMPOSE_FILE" "$CONTAINER_NAME"
    fi
    
    # Start container
    start_container "$DOCKER_COMPOSE_FILE" "$CONTAINER_NAME"
    
    # Setup SSH agent forwarding
    setup_ssh_agent "$CONTAINER_NAME"
    
    # Mount additional volumes
    mount_volumes "$CONTAINER_NAME"
    
    # Enter container
    enter_container "$CONTAINER_NAME"
    
    # Cleanup SSH agent
    docker exec "$CONTAINER_NAME" ssh-agent -k > /dev/null 2>&1 || true
    
    print_success "Exited container successfully!"
}

# Run main function with all arguments
main "$@"
