#!/bin/bash
# Stop MidiMind service

echo "Stopping MidiMind..."
sudo systemctl stop midimind
sudo systemctl status midimind --no-pager