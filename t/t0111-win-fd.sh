#!/bin/sh

test_description='win-fd functions work as we expect'

. ./test-lib.sh

get_valid_match_data()
{
	local match_elem_1
	local match_elem_2
	local match_elem_3
	local match_elem_4
	local number_match_1
	local number_match_2
	local separator_1
	local match_elem
	match_elem_1='\[[[:blank:]]*'
	match_elem_2='[[:blank:]]*=[[:blank:]]*'
	match_elem_3='[[:blank:]]*,[[:blank:]]*'
	match_elem_4='\]'
	match_elem=${match_elem_1}"fd"${match_elem_2}'[0-9]\+'${match_elem_3}'info'${match_elem_2}'[[:alnum:]]\+'${match_elem_4}
	match_elem=${match_elem}'\('${match_elem_3}${match_elem}'\)*'
	match_elem='\('${match_elem}'\)'
	echo -n $match_elem
}

is_valid_content()
{
	local line
	local match_elem
	local result
	result=0
	match_elem=`get_valid_match_data`
	while read line
	do
		local match_line
		match_line=`expr "$line" : ${match_elem}` 
		if test -z "$match_line"
		then
			result=-1
			break
		fi
	done <$1
	return $result
}

make_test_data()
{
	case $(uname -s) in
	*MINGW*)
		test-win-fd >actual
		;;
	*)
		 cat <<_EOF >actual
[fd = 0, info = 0x01]
_EOF
		;;
	esac

}
test_expect_success 'win_fd_apply_inheritence test' '
	make_test_data
	is_valid_content actual
'

test_done


