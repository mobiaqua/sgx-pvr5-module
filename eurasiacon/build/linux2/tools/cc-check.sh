#!/bin/sh
########################################################################### ###
#@Title         Test the nature of the C compiler.
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
# 
# The contents of this file are subject to the MIT license as set out below.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
# 
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
# 
# This License is also included in this distribution in the file called
# "MIT-COPYING".
# 
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
### ###########################################################################

LANG=C
export LANG

usage() {
	echo "usage: $0 [--64] [--clang] --cc CC [--out OUT] [cflag]"
	exit 1
}

check_clang() {
	if $CC -Wp,-dM -E - </dev/null | grep -q "__clang__"; then
		# Clang must be passed a program with a main() that returns 0.
		# It will produce an error if main() is improperly specified.
		IS_CLANG=1
		TEST_PROGRAM="int main(void){return 0;}"
	else
		# If we're not clang, assume we're GCC. GCC needs to be passed
		# a program with a faulty return in main() so that another
		# warning (unrelated to the flag being tested) is emitted.
		# This will cause GCC to warn about the unsupported warning flag.
		IS_CLANG=0
		TEST_PROGRAM="int main(void){return;}"

		# Bit check requires actually producing an object so GCC must
		# receive a working program.
		[ "$BIT_CHECK" = "1" ] && TEST_PROGRAM="int main(void){return 0;}"
	fi
}

do_cc() {
	echo "$TEST_PROGRAM" 2> /dev/null | $CC -W -Wall $3 -xc -c -o "$1" - > "$2" 2>&1
}

while true; do
	if [ "$1" = "--64" ]; then
		[ -z "$CLANG" ] && BIT_CHECK=1
	elif [ "$1" = "--clang" ]; then
		[ -z "$BIT_CHECK" ] && CLANG=1
	elif [ "$1" = "--cc" ]; then
		[ -z "$2" ] && usage
		CC="$2" && shift
	elif [ "$1" = "--out" ]; then
		[ -z "$2" ] && usage
		OUT="$2" && shift
	elif [ "${1#--}" != "$1" ]; then
		usage
	else
		break
	fi
	shift
done

[ -z "$CC" ] && usage
[ -z "$CLANG" ] && [ -z "$OUT" ] && usage
ccof=$OUT/cc-sanity-check
log=${ccof}.log

check_clang

if [ "$BIT_CHECK" = "1" ]; then
	do_cc "$ccof" "$log" ""
	file "$ccof" | grep 64-bit >/dev/null 2>&1 && echo true || echo false
elif [ "$CLANG" = "1" ]; then
	[ "$IS_CLANG" = "1" ] && echo true || echo false
else
	[ -z "$1" ] && usage
	do_cc "$ccof" "$log" "$1"
	if [ "$IS_CLANG" = "1" ]; then
		grep -qE "(unknown.*option .$1.|argument.*unused.* .$1.|unknown.*argument.* .$1.)" "$log"
	else
		grep -qE "(unrecognized.*option .$1.|option .$1. .*C\+\+|.$1.*no option)" "$log"
	fi
	if [ "$?" = "1" ]; then
		printf "%s\n" "$1"
		printf "%s\n" "$1" >> "${OUT}/flags.txt"
	fi
fi

rm -f "$ccof" "$log"
exit 0
