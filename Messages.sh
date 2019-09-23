#! /usr/bin/env bash
$EXTRACTRC `find . -name '*.rc' -o -name '*.ui' -o -name '*.kcfg' | grep -v 'libgit2'` >> rc.cpp
$XGETTEXT `find . -name '*.cpp' -o -name '*.h' -o -name '*.qml' | grep -v 'libgit2'` -o $podir/kup.pot
rm -f rc.cpp
