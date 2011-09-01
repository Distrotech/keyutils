###############################################################################
#
# Copyright (C) 2005 Red Hat, Inc. All Rights Reserved.
# Written by David Howells (dhowells@redhat.com)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version
# 2 of the License, or (at your option) any later version.
#
###############################################################################

echo === $OUTPUTFILE ===

endian=`file -L /proc/$$/exe`
if expr "$endian" : '.* MSB executable.*' >&/dev/null
then
    endian=BE
elif expr "$endian" : '.* LSB executable.*' >&/dev/null
then
    endian=LE
else
    echo -e "+++ \e[31;1mCan't Determine Endianness\e[0m"
    echo "+++ Can't Determine Endianness" >>$OUTPUTFILE
    exit 2
fi

maxtypelen=31
maxtype=`for ((i=0; i<$((maxtypelen)); i++)); do echo -n a; done`

PAGE_SIZE=`getconf PAGESIZE`
maxdesclen=$((PAGE_SIZE - 1))
maxdesc=`for ((i=0; i<$((maxdesclen)); i++)); do echo -n a; done`
maxcall=$maxdesc

maxsquota=`grep '^ *0': /proc/key-users | sed s@.*/@@`

function marker ()
{
    echo -e "+++ \e[33m$*\e[0m"
    echo +++ $* >>$OUTPUTFILE
}

function failed()
{
    echo -e "\e[31;1mFAILED\e[0m"
    echo === FAILED === >>$OUTPUTFILE
    keyctl show >>$OUTPUTFILE
    echo ============== >>$OUTPUTFILE
    result=FAIL
}

function expect_args_error ()
{
    "$@" >>$OUTPUTFILE 2>&1
    if [ $? != 2 ]
    then
	failed
    fi

}

function toolbox_report_result()
{
    if [ $RUNNING_UNDER_RHTS = 1 ]
    then
	report_result $TEST $result
    fi
    if [ $result = FAIL ]
    then
	exit 1
    fi
}

###############################################################################
#
# compare version numbers to see if the first is less (older) than the second
#
###############################################################################
function version_less_than ()
{
    a=$1
    b=$2

    if [ "$a" = "$b" ]
    then
	return 1
    fi

    # grab the leaders
    x=${a%%-*}
    y=${b%%-*}

    if [ "$x" = "$a" -o "$y" = "$b" ]
    then
	if [ "$x" = "$y" ]
	then
	    [ "$x" = "$a" ]
	else
	    __version_less_than_dot "$x" "$y"
	fi
    elif [ "$x" = "$y" ]
    then
	less_than "${a#*-}" "${b#*-}"
    else
	__version_less_than_dot "$x" "$y"
    fi
}

function __version_less_than_dot ()
{
    a=$1
    b=$2

    if [ "$a" = "$b" ]
    then
	return 1
    fi

    # grab the leaders
    x=${a%%.*}
    y=${b%%.*}

    if [ "$x" = "$a" -o "$y" = "$b" ]
    then
	if [ "$x" = "$y" ]
	then
	    [ "$x" = "$a" ]
	else
	    expr "$x" \< "$y" >/dev/null
	fi
    elif [ "$x" = "$y" ]
    then
	__version_less_than_dot "${a#*.}" "${b#*.}"
    else
	expr "$x" \< "$y" >/dev/null
    fi
}


###############################################################################
#
# extract an error message from the log file and check it
#
###############################################################################
function expect_error ()
{
    my_varname=$1

    my_errmsg="`tail -1 $OUTPUTFILE`"
    eval $my_varname="\"$my_errmsg\""

    if [ $# != 1 ]
    then
	echo "Format: expect_error <symbol>" >>$OUTPUTFILE
	failed
    fi

    case $1 in
	EPERM)		my_err="Operation not permitted";;
	EAGAIN)		my_err="Resource temporarily unavailable";;
	ENOENT)		my_err="No such file or directory";;
	EEXIST)		my_err="File exists";;
	ENOTDIR)	my_err="Not a directory";;
	EACCES)		my_err="Permission denied";;
	EINVAL)		my_err="Invalid argument";;
	ENODEV)		my_err="No such device";;
	ELOOP)		my_err="Too many levels of symbolic links";;
	EOPNOTSUPP)	my_err="Operation not supported";;
	EDEADLK)	my_err="Resource deadlock avoided";;
	EDQUOT)		my_err="Disk quota exceeded";;
	ENOKEY)
	    my_err="Required key not available"
	    old_err="Requested key not available"
	    alt_err="Unknown error 126"
	    ;;
	EKEYEXPIRED)
	    my_err="Key has expired"
	    alt_err="Unknown error 127"
	    ;;
	EKEYREVOKED)
	    my_err="Key has been revoked"
	    alt_err="Unknown error 128"
	    ;;
	EKEYREJECTED)
	    my_err="Key has been rejected"
	    alt_err="Unknown error 129"
	    ;;
	*)
	    echo "Unknown error message $1" >>$OUTPUTFILE
	    failed
	    ;;
    esac

    if expr "$my_errmsg" : ".*: $my_err" >&/dev/null
    then
	:
    elif [ "x$alt_err" != "x" ] && expr "$my_errmsg" : ".*: $alt_err" >&/dev/null
    then
	:
    elif [ "x$old_err" != "x" ] && expr "$my_errmsg" : ".*: $old_err" >&/dev/null
    then
	:
    else
	failed
    fi
}

###############################################################################
#
# wait for a key to be destroyed (get removed from /proc/keys)
#
###############################################################################
function pause_till_key_destroyed ()
{
    echo "+++ WAITING FOR KEY TO BE DESTROYED" >>$OUTPUTFILE
    hexkeyid=`printf %08x $1`

    while grep $hexkeyid /proc/keys
    do
	sleep 1
    done
}

###############################################################################
#
# wait for a key to be unlinked
#
###############################################################################
function pause_till_key_unlinked ()
{
    echo "+++ WAITING FOR KEY TO BE UNLINKED" >>$OUTPUTFILE

    while true
    do
	echo keyctl unlink $1 $2 >>$OUTPUTFILE
	keyctl unlink $1 $2 >>$OUTPUTFILE 2>&1
	if [ $? != 1 ]
	then
	    failed
	fi

	my_errmsg="`tail -1 $OUTPUTFILE`"
	if ! expr "$my_errmsg" : ".*: No such file or directory" >&/dev/null
	then
	    break
	fi
    done
}

###############################################################################
#
# request a key and attach it to the new keyring
#
###############################################################################
function request_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl request "$@" >>$OUTPUTFILE
    keyctl request "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# request a key and attach it to the new keyring, calling out if necessary
#
###############################################################################
function request_key_callout ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl request2 "$@" >>$OUTPUTFILE
    keyctl request2 "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# request a key and attach it to the new keyring, calling out if necessary and
# passing the callout data in on stdin
#
###############################################################################
function prequest_key_callout ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    data="$1"
    shift

    echo echo -n $data \| keyctl prequest2 "$@" >>$OUTPUTFILE
    echo -n $data | keyctl prequest2 "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# create a key and attach it to the new keyring
#
###############################################################################
function create_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl add "$@" >>$OUTPUTFILE
    keyctl add "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# create a key and attach it to the new keyring, piping in the data
#
###############################################################################
function pcreate_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    data="$1"
    shift

    echo echo -n $data \| keyctl padd "$@" >>$OUTPUTFILE
    echo -n $data | keyctl padd "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# create a key and attach it to the new keyring
#
###############################################################################
function create_keyring ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl newring "$@" >>$OUTPUTFILE
    keyctl newring "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# extract a key ID from the log file
#
###############################################################################
function expect_keyid ()
{
    my_varname=$1

    my_keyid="`tail -1 $OUTPUTFILE`"
    if expr "$my_keyid" : '[1-9][0-9]*' >&/dev/null
    then
	eval $my_varname=$my_keyid

	if [ $# = 2 -a "x$my_keyid" != "x$2" ]
	then
	    failed
	fi
    else
	eval $my_varname=no
	result=FAIL
    fi
}

###############################################################################
#
# prettily list a keyring
#
###############################################################################
function pretty_list_keyring ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl list $1 >>$OUTPUTFILE
    keyctl list $1 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# list a keyring
#
###############################################################################
function list_keyring ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl rlist $1 >>$OUTPUTFILE
    keyctl rlist $1 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# extract a keyring listing from the log file and see if a key ID is contained
# therein
#
###############################################################################
function expect_keyring_rlist ()
{
    my_varname=$1

    my_rlist="`tail -1 $OUTPUTFILE`"
    eval $my_varname="\"$my_rlist\""

    if [ $# = 2 ]
    then
	if [ "$2" = "empty" ]
	then
	    if [ "x$my_rlist" != "x" ]
	    then
		failed
	    fi
	else
	    my_found=0
	    case "$my_rlist" in
		$my_keyid|*\ $my_keyid|*\ $my_keyid\ *|$my_keyid\ *)
		    my_found=1
		    ;;
	    esac

	    if [ $my_found == 0 ]
	    then
		failed
	    fi
	fi
    fi
}

###############################################################################
#
# prettily describe a key
#
###############################################################################
function pretty_describe_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl describe $1 >>$OUTPUTFILE
    keyctl describe $1 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# describe a key
#
###############################################################################
function describe_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl rdescribe $1 "@" >>$OUTPUTFILE
    keyctl rdescribe $1 "@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# extract a raw key description from the log file and check it
#
###############################################################################
function expect_key_rdesc ()
{
    my_varname=$1

    my_rdesc="`tail -1 $OUTPUTFILE`"
    eval $my_varname="\"$my_rdesc\""

    if ! expr "$my_rdesc" : "$2" >&/dev/null
    then
	failed
    fi
}

###############################################################################
#
# read a key's payload as a hex dump
#
###############################################################################
function read_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl read $1 >>$OUTPUTFILE
    keyctl read $1 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# read a key's payload as a printable string
#
###############################################################################
function print_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl print $1 >>$OUTPUTFILE
    keyctl print $1 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# pipe a key's raw payload to stdout
#
###############################################################################
function pipe_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl pipe $1 >>$OUTPUTFILE
    keyctl pipe $1 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# extract a printed payload from the log file
#
###############################################################################
function expect_payload ()
{
    my_varname=$1

    my_payload="`tail -1 $OUTPUTFILE`"
    eval $my_varname="\"$my_payload\""

    if [ $# == 2 -a "x$my_payload" != "x$2" ]
    then
	failed
    fi
}

###############################################################################
#
# revoke a key
#
###############################################################################
function revoke_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl revoke $1 >>$OUTPUTFILE
    keyctl revoke $1 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# unlink a key from a keyring
#
###############################################################################
function unlink_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    my_wait=0
    if [ "x$1" = "x--wait" ]
    then
	my_wait=1
	shift
    fi

    echo keyctl unlink $1 $2 >>$OUTPUTFILE
    keyctl unlink $1 $2 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi

    # keys are destroyed lazily
    if [ $my_wait = 1 ]
    then
	pause_till_key_unlinked $1 $2
    fi
}

###############################################################################
#
# update a key from a keyring
#
###############################################################################
function update_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl update $1 $2 >>$OUTPUTFILE
    keyctl update $1 $2 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# update a key from a keyring, piping the data in over stdin
#
###############################################################################
function pupdate_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo echo -n $2 \| keyctl pupdate $1 >>$OUTPUTFILE
    echo -n $2 | keyctl pupdate $1 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# clear a keyring
#
###############################################################################
function clear_keyring ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl clear $1 >>$OUTPUTFILE
    keyctl clear $1 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# link a key to a keyring
#
###############################################################################
function link_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl link $1 $2 >>$OUTPUTFILE
    keyctl link $1 $2 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# search for a key in a keyring
#
###############################################################################
function search_for_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl search "$@" >>$OUTPUTFILE
    keyctl search "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# set the permissions mask on a key
#
###############################################################################
function set_key_perm ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl setperm "$@" >>$OUTPUTFILE
    keyctl setperm "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# set the ownership of a key
#
###############################################################################
function chown_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl chown "$@" >>$OUTPUTFILE
    keyctl chown "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# set the group ownership of a key
#
###############################################################################
function chgrp_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl chgrp "$@" >>$OUTPUTFILE
    keyctl chgrp "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# run as a new session
#
###############################################################################
function new_session ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl session "$@" >>$OUTPUTFILE
    keyctl session "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# instantiate a key
#
###############################################################################
function instantiate_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl instantiate "$@" >>$OUTPUTFILE
    keyctl instantiate "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# instantiate a key, piping the data in over stdin
#
###############################################################################
function pinstantiate_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    data="$1"
    shift

    echo echo -n $data \| keyctl pinstantiate "$@" >>$OUTPUTFILE
    echo -n $data | keyctl pinstantiate "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# negate a key
#
###############################################################################
function negate_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl negate "$@" >>$OUTPUTFILE
    keyctl negate "$@" >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}

###############################################################################
#
# set a key's expiry time
#
###############################################################################
function timeout_key ()
{
    my_exitval=0
    if [ "x$1" = "x--fail" ]
    then
	my_exitval=1
	shift
    fi

    echo keyctl timeout $1 $2 >>$OUTPUTFILE
    keyctl timeout $1 $2 >>$OUTPUTFILE 2>&1
    if [ $? != $my_exitval ]
    then
	failed
    fi
}
