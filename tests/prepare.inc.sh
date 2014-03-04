# preparation script for running keyring tests

# Find the relative path from pwd to the directory holding this file
includes=${BASH_SOURCE[0]}
includes=${includes%/*}/

# --- need to run in own session keyring
if [ "x`keyctl rdescribe @s | sed 's/.*;//'`" != "xRHTS/keyctl/$$" ]
then
    echo "Running with session keyring RHTS/keyctl/$$"
    exec keyctl session "RHTS/keyctl/$$" bash $0 $@ || exit 8
fi

# Set up for the Red Hat Test System
RUNNING_UNDER_RHTS=0
if [ -x /usr/bin/rhts_environment.sh ]
then
    PACKAGE=$(rpm -q --qf "%{name}" --whatprovides /bin/keyctl)
    . /usr/bin/rhts_environment.sh
    RUNNING_UNDER_RHTS=1
elif [ -z "$OUTPUTFILE" ]
then
    OUTPUTFILE=$PWD/test.out
    echo -n >$OUTPUTFILE
fi

case `lsb_release -i | awk '{ print $3}'` in
    Fedora*)		OSDIST=Fedora;;
    RedHatEnterprise*)	OSDIST=RHEL;;
    *)			OSDIST=Unknown;;
esac

OSRELEASE=`lsb_release -r | awk '{ print $2}'`

KEYUTILSVER=`keyctl --version 2>/dev/null`
if [ -n "$KEYUTILSVER" ]
then
    :
elif [ -x /bin/rpm ]
then
    KEYUTILSVER=`rpm -q keyutils`
else
    echo "Can't determine keyutils version" >&2
    exit 9
fi

KEYUTILSVER=`expr "$KEYUTILSVER" : '.*keyutils-\([0-9.]*\).*'`

. $includes/version.inc.sh

KERNELVER=`uname -r`

#
# Make sure the TEST envvar is set.
#
if [ -z "$TEST" ]
then
    p=`pwd`
    case $p in
	*/keyctl/*)
	    TEST=keyctl/${p#*/keyctl/}
	    ;;
	*/bugzillas/*)
	    TEST=bugzillas/${p#*/bugzillas/}
	    ;;
	*)
	    TEST=unknown
	    ;;
    esac
fi

#
# Work out whether the big_key type is supported by the kernel
#
have_big_key_type=0
if [ $OSDIST-$OSRELEASE = RHEL-7 ]
then
    # big_key is backported to 3.10 for RHEL-7
    have_big_key_type=1
elif kernel_at_or_later_than 3.13-rc1
then
    have_big_key_type=1
fi
