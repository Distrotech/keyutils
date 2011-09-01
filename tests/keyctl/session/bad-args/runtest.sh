#!/bin/sh

. ../../../prepare.inc.sh
. ../../../toolbox.inc.sh


# ---- do the actual testing ----

result=PASS
echo "++++ BEGINNING TEST" >$OUTPUTFILE

# check that an empty keyring name fails correctly
marker "SESSION WITH EMPTY KEYRING NAME"
new_session --fail ""
expect_error EINVAL

# check that an overlong keyring name fails correctly
marker "SESSION WITH OVERLONG KEYRING NAME"
new_session --fail a$maxdesc
expect_error EINVAL

echo "++++ FINISHED TEST: $result" >>$OUTPUTFILE

# --- then report the results in the database ---
toolbox_report_result $TEST $result
