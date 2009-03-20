# Library for simple test scripts
# Copyright (C) 2009 Free Software Foundation, Inc.
#
# Copying and distribution of this file, with or without modification,
# in any medium, are permitted without royalty provided the copyright
# notice and this notice are preserved.

require_cat() {
    if ! cat /dev/null > /dev/null 2> /dev/null; then
	echo "This test requires the cat utility" >&2
	exit 2
    fi
}

require_diff() {
    case "`diff --version 2> /dev/null`" in
    *GNU*)
	;;
    *)
	echo "This test requires GNU diff" >&2
	exit 2
    esac
}

use_tmpdir() {
    tmpdir=`mktemp -d`
    if test -z "$tmpdir" ; then
	echo "This test requires the mktemp utility" >&2
	exit 2
    fi
    cd "$tmpdir"
}

use_local_patch() {
    test -n "$PATCH" || PATCH=$PWD/patch

    eval 'patch() {
	$PATCH "$@"
    }'
}

_check() {
    printf "[%s] %s -- " "${BASH_LINENO[1]}" "$*"
    expected=`cat`
    if got=`set +x; eval "$*" < /dev/null 2>&1` && test "$expected" = "$got" ; then
	printf "ok\n"
	checks_succeeded=$[checks_succeeded+1]
    else
	printf "FAILED\n"
	if test "$expected" != "$got" ; then
	    echo "$expected" > expected~
	    echo "$got" > got~
	    diff -u -L expected -L got expected~ got~
	    rm -f expected~ got~
	fi
	checks_failed=$[checks_failed+1]
    fi
}

check() {
    _check "$@"
}

ncheck() {
    _check "$@" < /dev/null
}

cleanup() {
    checks_total=$[checks_succeeded+checks_failed]
    status=0
    if test $checks_total -gt 0 ; then
	if test $checks_failed -gt 0 ; then
	    status=1
	fi
	printf "%s tests (%s passed, %s failed)\n" \
	       $checks_total $checks_succeeded $checks_failed
    fi
    if test -n "$tmpdir" ; then
	set -e
	cd /
	chmod -R u+rwx "$tmpdir"
	rm -rf "$tmpdir"
    fi
    exit $status
}

require_cat

checks_succeeded=0
checks_failed=0
trap cleanup 0
