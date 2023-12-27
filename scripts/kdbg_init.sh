#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: source kdbg_init.sh <PathOfDirScripts>"
    [ $# -eq 0 ]
else
    export     KDBG_BIN_PATH=$1
    export  ksym_demo_enable=true
    export   spa_demo_enable=true
    export trace_demo_enable=true
    export update_zfs_depend=false
    
    chmod +x $KDBG_BIN_PATH/ksym.py $KDBG_BIN_PATH/kdbg.sh
    
    if [[ "$PATH" =~ "$KDBG_BIN_PATH" ]]; then
        echo "Warning: KDBG_BIN_PATH($KDBG_BIN_PATH) is already set"
    else
        export PATH=$PATH:$KDBG_BIN_PATH
    fi
fi
