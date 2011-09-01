# preparation script for running keyring tests

# --- need to run in own session keyring
if [ "x`keyctl rdescribe @s | sed 's/.*;//'`" != "xRHTS/keyctl/$$" ]
then
    echo "Running with session keyring RHTS/keyctl/$$"
    exec keyctl session "RHTS/keyctl/$$" sh $0 $@ || exit 8
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

OSVER=`lsb_release -r | awk '{ print $2}'`
