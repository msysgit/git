#!/bin/sh

test_description='env-util functions work as we expect'
. ./test-lib.sh
cat >expect <<EOF
env_buffer_init finished
env_buffer_add finished
env_buffer_add finished
env1=value1
env2=value2
EOF


test_expect_success 'env-buffer-test' '
	test-env-utils >actual
	test_cmp expect actual
'

test_done
