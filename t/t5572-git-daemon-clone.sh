#!/bin/sh

test_description='test clone over git protocol repeated'

TEST_NO_CREATE_REPO=1

. ./test-lib.sh

LIB_GIT_DAEMON_PORT=${LIB_GIT_DAEMON_PORT-5572}
. "$TEST_DIRECTORY"/lib-git-daemon.sh

. "$TEST_DIRECTORY"/lib-git-daemon/repo-gen.sh

. "$TEST_DIRECTORY"/lib-git-daemon/modify-repo.sh

clone_in_stress() {
    local index
    local result
    index=0
    result=0
    while test ${index} -lt ${1}
	do
	{
	    git clone ${GIT_DAEMON_URL}/repo rep_clone
	    result=$?
	    rm -r -f rep_clone
	    if test $result -ne 0
	    then
		break
	    fi
	    index=$((${index} + 1))
	}
    done
    return ${result}
}

init_repository()
{
	mkdir "$GIT_DAEMON_DOCUMENT_ROOT_PATH/repo"
	(cd "$GIT_DAEMON_DOCUMENT_ROOT_PATH/repo"
	 generate_files &&
	 git init &&
	 git add . &&
	 git commit -m "created new test repository"
         : >.git/git-daemon-export-ok
	)
}
modify_some_files()
{
	local name
	for name in `generate_names 'a' 'f'`
	do
		local file_name
		for file_name in `create_file_names 0 5 ${name} 'c'`
	    	do
			{
				modify_and_commit ${file_name}
			}
	    	done
	done
	
}

start_git_daemon

say >&3 "$GIT_DAEMON_DOCUMENT_ROOT_PATH/repo"

test_expect_success 'setup repository' '
	init_repository &&
	(cd "$GIT_DAEMON_DOCUMENT_ROOT_PATH/repo" &&
	 modify_some_files)
'

test_expect_success 'clone in stress circumstance' '
	clone_in_stress 200
'

stop_git_daemon

test_done
