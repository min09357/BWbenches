#!/bin/bash
# Thin launcher. All configuration now lives in config.py (generated from
# config_template.py by `make`); edit config.py, not this script.
set -e
python3 ./Test.py "$@"
