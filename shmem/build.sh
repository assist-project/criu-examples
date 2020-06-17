#!/bin/bash
readonly SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

(cd $SCRIPT_DIR; gcc main.c -L /usr/local/lib/x86_64-linux-gnu/ -pthread -lcriu -o main)

