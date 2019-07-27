#! /usr/bin/env bash
$EXTRACTRC `find . -name 'libgit2*' -prune -o '(' -name '*.rc' -o -name '*.ui' -o -name '*.kcfg' ')'` >> rc.cpp
$XGETTEXT `find . -name 'libgit2*' -prune -o '(' -name '*.cpp' -o -name '*.h' -o -name '*.qml' ')'` -o $podir/kup.pot
rm -f rc.cpp
