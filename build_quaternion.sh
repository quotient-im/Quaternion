#!/bin/bash

SELF="$0"
ARGS="$@"

DIE() {
    E=$1; shift
    cat <<-EOF
	
	============================
	== FAIL: $@
	============================
	
	EOF
    exit $E
}

InstallDeps() {
    echo "Run this command to install latest dependencies"
    `grep 'apt-get' README.md`
}

Pull() {
    echo "to update the codebase run"
    echo "git pull"
    echo "then re-run this script"
    git submodule init || DIE 9 "submodule init"
    git submodule update || DIE 9 "submodule update"
}


Build() {
    [[ -f Quaternion.project ]] || DIE 9 "Not in top level of the Quaternion project tree"
    [[ -d build_dir ]] || mkdir build_dir || DIE 9 "Couldn't cd to build_dir"
    pushd build_dir || DIE 9 "Couldn't cd to build_dir"
    cmake .. || DIE 9 "cmake .."
    cmake --build . --target all || DIE 9 "cmake --build"
    popd
}

Update() {
    Pull
    InstallDeps
}

Run() {
    build_dir/quaternion || DIE 1 "starting quaternion"
}

Help() {
    cat <<-EOF
	This script updates and builds, or installs and builds Quaternion on a debian based linux.
	run it like....
	
	    $SELF -u  # update the code using git pull etc
	    $SELF -b  # build the code
	    $SELF -r  # run the binary
	    $SELF -h  # this help
	
	Multiple arguments can be used on the same commandline
	
	EOF
    exit 1;
}

main() {
    noHelp=false;
    if [[ "$@" =~ -h|-H|--help ]]; then
        Help
    fi

    if [[ "$@" =~ -u|-U|--update ]]; then
        noHelp=true;
        Update
    fi

    if [[ "$@" =~ -b|-B|--build ]]; then
        noHelp=true;
        Build
    fi

    if [[ "$@" =~ -r|-R|--run ]]; then
        noHelp=true;
        Run
    fi

    if ! $noHelp; then
        Help
    fi
}

main "$@";
