#!/bin/sh

if test -z "$GIT_TEST_GIT_DAEMON"
then
	skip_all="git-daemon testing disabled (define GIT_TEST_GIT_DAEMON to enable)"
	test_done
fi

LIB_GIT_DAEMON_PORT=${LIB_GIT_DAEMON_PORT-'8121'}

GIT_DAEMON_PID=
GIT_DAEMON_REAL_PID=
GIT_DAEMON_DOCUMENT_ROOT_PATH="$PWD"/repo
GIT_DAEMON_PID_FILE="$GIT_DAEMON_DOCUMENT_ROOT_PATH"/daemon_pid
GIT_DAEMON_URL=git://127.0.0.1:$LIB_GIT_DAEMON_PORT

GIT_DAEMON_OUTPUT=git_daemon_output

kill_and_wait() {
	local result
	result=0
	case $(uname -s) in
	*MINGW*)
		(exec kill -f $1)	
		wait $2
		# $2 process will be exit on normaly. 
		# Because child process exit.
		# I thought it was better to exit as kill emulation. 
		result=$((128 + 15))
		;;
	*)
		kill $2
		wait $2
		result=$?
		;;
	esac
	return $result

}

git_daemon_kill() {
	local result
	result=0
	if test $1 -ne $2;
	then
		# assume running on Cygiwin or Mingw or Msysgit
		kill_and_wait $1 $2
		result=$?
	else 
		kill $2
		wait $2
		result=$?
	fi
	return $result
}



start_git_daemon() {
	if test -n "$GIT_DAEMON_PID"
	then
		error "start_git_daemon already called"
	fi

	mkdir -p "$GIT_DAEMON_DOCUMENT_ROOT_PATH"

	trap 'code=$?; stop_git_daemon; (exit $code); die' EXIT

	say >&3 "Starting git daemon ..."
	if mkfifo "${GIT_DAEMON_OUTPUT}";
	then
		GIT_DAEMON_OUT_FIFO=1
	else
		GIT_DAEMON_OUT_PUT="$GIT_DAEMON_DOCUMENT_ROOT_PATH"/"$GIT_DAEMON_OUTPUT"
	fi
	git daemon --listen=127.0.0.1 --port="$LIB_GIT_DAEMON_PORT" \
		--reuseaddr --verbose \
		--base-path="$GIT_DAEMON_DOCUMENT_ROOT_PATH" \
		--pid-file="$GIT_DAEMON_PID_FILE" \
		"$@" "$GIT_DAEMON_DOCUMENT_ROOT_PATH" \
		>&3 2>"$GIT_DAEMON_OUTPUT" &

	GIT_DAEMON_PID=$!


	if test -n "$GIT_DAEMON_OUT_FIFO";
	then
		{
			read line <&7
			echo >&4 "$line"
			cat <&7 >&4 &
		} 7<"$GIT_DAEMON_OUTPUT" &&

		# Check expected output
		if test x"$(expr "$line" : "\[[0-9]*\] \(.*\)")" != x"Ready to rumble"
		then
			read GIT_DAEMON_REAL_PID <"$GIT_DAEMON_PID_FILE" 

			git_daemon_kill "$GIT_DAEMON_REAL_PID" \
				"$GIT_DAEMON_PID"
			rm -f "$GIT_DAEMON_PID_FILE"
			trap 'die' EXIT
			error "git daemon failed to start"
		fi
	fi	

}

stop_git_daemon() {
	if test -z "$GIT_DAEMON_PID"
	then
		return
	fi

	trap 'die' EXIT
	if test -z "$GIT_DAEMON_OUT_FIFO";
	then
		cat <"$GIT_DAEMON_OUTPUT" >&4
	fi
	read GIT_DAEMON_REAL_PID <"$GIT_DAEMON_PID_FILE" 
	# kill git-daemon child of git
	say >&3 "Stopping git daemon ..."
	git_daemon_kill "$GIT_DAEMON_REAL_PID"  "$GIT_DAEMON_PID" >&3 2>&4
	#git_daemon_kill "$GIT_DAEMON_REAL_PID"  "$GIT_DAEMON_PID"

	ret=$?
	# expect exit with status 143 = 128+15 for signal TERM=15
	if test $ret -ne 143
	then
		error "git daemon exited with status: $ret"
	fi
	rm -f "$GIT_DAEMON_PID_FILE"
	GIT_DAEMON_PID=
	rm -f git_daemon_output
}
