#!/bin/sh

. ../../../prepare.inc.sh
. ../../../toolbox.inc.sh


# ---- do the actual testing ----

result=PASS
echo "++++ BEGINNING TEST" >$OUTPUTFILE

marker "NO ARGS"
expect_args_error keyctl request
expect_args_error keyctl request2
expect_args_error keyctl prequest2

marker "ONE ARG"
expect_args_error keyctl request 0
expect_args_error keyctl request2 0
expect_args_error keyctl prequest2 0

marker "TWO ARGS"
expect_args_error keyctl request2 0 0

marker "FOUR ARGS"
expect_args_error keyctl request 0 0 0 0
expect_args_error keyctl prequest2 0 0 0 0

marker "FIVE ARGS"
expect_args_error keyctl request2 0 0 0 0 0

echo "++++ FINISHED TEST: $result" >>$OUTPUTFILE

# --- then report the results in the database ---
toolbox_report_result $TEST $result
