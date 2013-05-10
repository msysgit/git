#!/bin/sh

REPO_GEN_NAME_START='a'
REPO_GEN_NAME_END='z'

REPO_GEN_NAME_INDEX=0
REPO_GEN_NAME_INDEX_LENGTH=20

REPO_GEN_CONTENT_SIZE=30


repo_gen_create_content_1() {
    local content
    local index
    local result
    index=0
    result=''
    while test $index -lt $2
    do
	{
	    result=$(cat<< EOF 
$result
int ${1}${index}(int a)
{
   return a + ${index};
}
EOF
)
	    index=$(($index + 1))
	}
    done
    echo "$result"
}

create_file_names() {
    local index
    index=$1
    local result
    result=''
    while test $index -lt $2
    do
	{
	    result=${result}' '${3}${index}.${4}
	    index=$(($index + 1))
	}
    done
    echo $result
}


generate_names() {
    local index
    local result
    local last_index
    local start_value
    local end_value

    index=$(printf "%d" "'${1}")
    last_index=$(printf "%d" "'${2}")
    result=''
    while test ${index} -le ${last_index}
    do
	{
	    local hex_str
	    hex_str=$(printf "\\%03o" "${index}")
	    result=$(printf "${result} ${hex_str}")
	    index=$(($index + 1))
	}
    done
    echo $result
}

generate_files()
{
    local name
    for name in $(generate_names ${REPO_GEN_NAME_START} ${REPO_GEN_NAME_END})
    do
	{
	    local contents
	    contents=$(repo_gen_create_content_1 ${name} ${REPO_GEN_CONTENT_SIZE});
	    local file_name
	    for file_name in $(create_file_names ${REPO_GEN_NAME_INDEX} ${REPO_GEN_NAME_INDEX_LENGTH} ${name} 'c')
	    do
		{
		    echo "${contents}" > "${file_name}" ;
		}
	    done
	}
    done
}

clean_generated_files()
{
    local name
    local file_names
    file_names=''
    for name in $(generate_names ${REPO_GEN_NAME_START} ${REPO_GEN_NAME_END})
    do
	{
	    local contents
	    contents=$(repo_gen_create_content_1 ${name} ${REPO_GEN_CONTENT_SIZE});
	    local file_name
	    for file_name in $(create_file_names ${REPO_GEN_NAME_INDEX} ${REPO_GEN_NAME_INDEX_LENGTH} ${name} 'c')
	    do
		{
		    file_names="${file_names} ${file_name}"
		}
	    done
	}
    done
    rm -r -f ${file_names}

}


for arg
do
    case ${arg} in
	--generate|-g)
	    mode='generate'  
	    ;;
	--remove|-r)
	    mode='remove'
	    ;;
    esac
done
case ${mode} in
    generate)
	generate_files
	;;
    remove)
	clean_generated_files
	;;
esac
