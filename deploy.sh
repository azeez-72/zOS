#!/bin/bash

set -e  # exit on any error

# Run from repo root (parent of sbunix-skeleton) so the correct tree is copied.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

HOST="course-containers.compas.cs.stonybrook.edu"
PORT="2207"
USER="student"
REMOTE_DIR="~/tests/sbunix_azeez_test_main"
LOCAL_DIR="sbunix-skeleton"

# ─── colors ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # no color

echo -e "${YELLOW}[1/4] Copying files to remote...${NC}"
# Create remote dir, then copy *contents* of LOCAL_DIR into it so we overwrite
# existing files. (Using "scp -r dir host:path" when path exists would create
# path/dir/ and leave path/* unchanged.)
ssh -p "$PORT" "$USER@$HOST" "mkdir -p $REMOTE_DIR"
scp -P "$PORT" -r "$LOCAL_DIR"/* "$USER@$HOST:$REMOTE_DIR/"
echo -e "${GREEN}Done.${NC}"

echo -e "${YELLOW}[2/4] Running make clean...${NC}"
ssh -p "$PORT" "$USER@$HOST" "cd $REMOTE_DIR && make clean"
echo -e "${GREEN}Done.${NC}"

echo -e "${YELLOW}[3/4] Running make...${NC}"
ssh -p "$PORT" "$USER@$HOST" "cd $REMOTE_DIR && make"
echo -e "${GREEN}Done.${NC}"

echo -e "${YELLOW}[4/4] Running make qemu...${NC}"
ssh -p "$PORT" -t "$USER@$HOST" "cd $REMOTE_DIR && make qemu"

