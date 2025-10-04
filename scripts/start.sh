#!/bin/bash
# Start MidiMind service

echo "Starting MidiMind..."
sudo systemctl start midimind
sudo systemctl status midimind --no-pager