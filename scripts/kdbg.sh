#!/bin/bash

DEV_NAME=kdbg
DEV_PATH=/dev/$DEV_NAME
SRC_PATH=/home/osboxes/workspace/kdbg/src/kernel
 KO_PATH=$SRC_PATH/${DEV_NAME}.ko
  SCRIPT=$(basename $0)

function isMade       () { [ -e $KO_PATH ];         }
function cmdMake      () { make -C $SRC_PATH $@;    }

function isInstalled  () { [ -e $DEV_PATH ];        }
function cmdInstall   () { sudo insmod $KO_PATH;    }
function cmdUninstall () { sudo rmmod $DEV_NAME;    }

function cmdAccess () {
    if ! isInstalled; then
        if ! isMade; then
            cmdMake || return
        fi
        cmdInstall || return
    fi
    
    local fd=3
    exec 3<> $DEV_PATH || return
    
    local tmp=$(mktemp)
    while [ -e $tmp ]; do
        cat <&$fd
        sleep 0.05
    done &
    local bg=$!
    
    if [ $# -eq 0 ]; then
        echo help >&$fd; local rc=$?
    elif [ $# -eq 2 -a "x$1" = "x-f" -a -f "$2" ]; then
        cat $2 >&$fd; local rc=$?
    else
        for arg in $@; do
            echo $arg
        done >>$tmp
        cat $tmp >&$fd; local rc=$?
    fi
    
    rm -rf $tmp
    wait $bg
    cat <&$fd
    
    exec 3>&-
    rm -rf $tmp
    return $rc
}

function cmdDriver () {
    if [ $# -eq 0 ]; then
        echo "Usage: $SCRIPT driver <makeAll|makeClean|install|uninstall> ..."
        return
    fi
    
    local cmd
    for cmd in $@; do
        case "$cmd" in
            makeAll )
                cmdMake
                ;;
            makeClean )
                cmdMake clean
                ;;
            install )
                isMade || cmdMake || return
                cmdInstall && echo "Info: install driver success"
                ;;
            uninstall )
                cmdUninstall && echo "Info: uninstall driver success"
                ;;
            help )
                echo "Usage: $SCRIPT driver <makeAll|makeClean|install|uninstall> ..."
                ;;
            * )
                echo "Error: *** invalid cmd($cmd)."
                echo "Usage: $SCRIPT driver <makeAll|makeClean|install|uninstall> ..."
                return 1
                ;;
        esac
    done
}

function usage () {
    echo "Usage: $SCRIPT driver <makeAll|makeClean|install|uninstall> ..."
    echo "       $SCRIPT access <cmd> ..."
    echo "Environ-Varibles:"
    echo "       ksym_demo_enable=true"
    echo "        spa_demo_enable=true"
}

function Main () {
    if [ $# -eq 0 ]; then
        usage
        return
    fi
    
    local cmd=$1; shift
    case "$cmd" in
        driver )
            cmdDriver $@
            ;;
        access )
            cmdAccess $@
            ;;
        help )
            usage
            ;;
        * )
            echo "Error: *** invalid cmd($cmd)."
            usage
            return 1
            ;;
    esac
}

if [ $# -gt 1 -a "$1" = "access" ]; then
    __argsFile__=$(mktemp); trap "rm -rf $__argsFile__" EXIT
    for ((i = 2; i <= $#; i++)); do
        eval "echo \"\$$i\" >>$__argsFile__"
    done
    Main access -f $__argsFile__
else
    Main $@
fi
