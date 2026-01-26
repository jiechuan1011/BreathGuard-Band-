#!/bin/bash

echo "Starting Auto Git Sync Script..."
echo "Press Ctrl+C to stop the script"
echo ""

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo "Error: Python3 is not installed or not in PATH"
    exit 1
fi

# Change to parent directory (project root)
cd ..

# Run simplified script (no extra dependencies required)
echo "Running simplified auto git sync script..."
echo "Monitoring directory: $(pwd)"
echo ""
python3 scripts/auto_git_sync_simple.py