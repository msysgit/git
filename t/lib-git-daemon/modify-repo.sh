#!/bin/sh
modify_and_commit()
{
	local lines
	lines=`head -n 5 "$1"`
	echo "$lines" >>"$1"
	git commit -a -m "append some lines to $1"
}

