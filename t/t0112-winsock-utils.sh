#!/bin/sh 

test_description='windows specific socket utility tests'

. ./test-lib.sh

save_config() {
	SAVED_WIN_SOCK_TIME_WAIT=$(git config --int 'win.sock.time.wait')
	return 0
}

setup_config() {
	git config --add --int 'win.sock.time.wait' 120	
}


restore_config() {
	if test -n "${SAVED_WIN_SOCK_TIME_WAIT}"
	then
		git config --add --int 'win.sock.time.wait' "${SAVED_WIN_SOCK_TIME_WAIT}"
	else
		git config --unset 'win.sock.time.wait'	
	fi
}

save_config

is_valid_output()
{
	local a_line
	local i	
	local saved_ifs
	local regex1
	local regex2
	local regex3
	local result
	regex1='\(TIME_WAIT from registry : [ [:alnum:]]\+\)'
	regex2='\(TIME_WAIT from git\-config : [ [:alnum:]]\+\)'
	regex3='\(TIME_WAIT : [[:digit:]]\+\)'

	result=0
	saved_ifs="$IFS"
	IFS=$(echo $'\n')
	i=0
	for a_line in $(cat $1)		
	do
		local match_line
		local regex
		case $i in
		0)
			regex=${regex1}
			;;
		1)
			regex=${regex2}
			;;
		2)
			regex=${regex3}
			;;
		*)
		esac
		echo "${regex}"
		match_line=$(expr "${a_line}" : ${regex})
		if test -z "${match_line}"
		then
			result=-1	
			break
		fi
		i=$((i + 1))
	done
	IFS="$saved_ifs"
	return $result
}

make_test_data()
{
	case $(uname -s) in
	*MINGW*)
		test-winsock-utils >actual
		;;
	*)
		 cat <<_EOF >actual
TIME_WAIT from registry : 10
TIME_WAIT from git-config : 10
TIME_WAIT : 20
_EOF
		;;
	esac

}

test_expect_success 'run setup' '
	setup_config
'
test_expect_success 'read from registry' '
	 make_test_data &&
	is_valid_output actual
'

restore_config

test_done

