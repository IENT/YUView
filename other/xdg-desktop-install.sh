#!/bin/sh

# -- Open script-dir-homed subshell
(

ABS_SCRIPT_DIR=`dirname "$0"`
cd "${ABS_SCRIPT_DIR}"

COMPANY='IENT'
APPS='YUView'
ICONSIZES='16 32 128 256'

MODE='--mode user'
[ "$USER" = 'root' ] && MODE='--mode system'

#-- Mime types: Slashes replaced by '-'. Designer is legacy
MIMETYPES="application-vnd.ient.yuview.yuvfile"

# THEME='--theme crystalsvg'


# export XDG_UTILS_DEBUG_LEVEL=1

usage()
{
cat <<EOF

$0 (-i|-d)

Options:
 -i  Install
 -d  Deinstall

Installs YUView into the application menu of XDG-compliant desktops
provided, that the XDG utilities are installed:
http://portland.freedesktop.org/wiki/XdgUtils
EOF
}


# -- main: Opts
case "$1" in
    -i) OPERATION='install' ;;
    -d) OPERATION='deinstall' ;;
    *) usage; exit 1;;
esac

if [ ! "x$2" = "x" ]
then
    usage
    exit 1
fi

if [ -z "$OPERATION" ]
then
   usage
   exit 1
fi

for required in xdg-mime xdg-desktop-menu xdg-icon-resource ; do
    if ${required} --version 1>/dev/null 2>/dev/null ; then
        continue
    fi
    echo "Required command ${required} not found."
    exit 1
done


# -- main: deinstall
if [ $OPERATION = deinstall ]
then
    echo "### Deinstalling mime types"
    xdg-mime uninstall $MODE IENT-YUView.xml 1>/dev/null 2>/dev/null

    echo "### Deinstalling desktop files"
    for A in $APPS
    do
    	# -- desktop file
        NAME=$COMPANY-$A
        xdg-desktop-menu uninstall $MODE $NAME.desktop 1>/dev/null 2>/dev/null
	# -- Icon
        for S in $ICONSIZES
	do
            xdg-icon-resource uninstall $MODE $THEME --size $S $NAME 1>/dev/null 2>/dev/null
	    # File types: Todo: Provide one per type!
	    for M in $MIMETYPES
	    do
	        xdg-icon-resource uninstall $MODE $THEME --context mimetypes  --size $S $M 1>/dev/null 2>/dev/null
	    done
	done
    done
else
    echo "### Installing desktop files"
    
    # -- Open file generation subshell
    (

    for A in $APPS
    do
        # -- Icon
        NAME=$COMPANY-$A
	for S in $ICONSIZES
	do
	    ICONFILE=$NAME-$S.png
            xdg-icon-resource install $MODE $THEME --size $S $ICONFILE $NAME || exit 1
	    # File types: Todo: Provide one per type!
	    for M in $MIMETYPES
	    do
	        xdg-icon-resource install $MODE $THEME --context mimetypes --size $S $ICONFILE $M || exit 1
	    done
	done
	# -- desktop file (no dash since this confuses KDE 3.5)
	xdg-desktop-menu install $MODE $NAME.desktop || exit 1
    done

    # -- Close file generation subshell and clean up
    )
    ret=$?
    rm -rf "${TEMPDIR}"
    if [ ! $ret = 0 ]; then
       exit $?
    fi

    echo "### Installing mime types"
    xdg-mime install $MODE IENT-YUView.xml || exit 1
fi


# -- Properly close subshell
) 
exit $?
