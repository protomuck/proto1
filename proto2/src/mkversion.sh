#!/bin/sh

# Generates version.c
#
# Copyright (c) 1990 Chelsea Dyerman
# University of California, Berkeley (XCF)

if [ ! -f version.c ]
then
        generation=0
else
        generation=`sed -n 's/^const char \*generation = \"\(.*\)\";/\1/p' < version.c`
        if [ ! "$generation" ]; then generation=0; fi
fi

generation=`expr $generation + 1`
creation=`date | awk '{if (NF == 6) { print $1 " "  $2 " " $3 " "  $6 " at " $4 " " $5 } else { print $1 " "  $2 " " $3 " " $7 " at " $4 " " $5 " " $6 }}'`
cat > version.c <<EOF
/*
 * Copyright (c) 1990 Chelsea Dyerman
 * University of California, Berkeley (XCF)
 *
 * Some parts of this code -- in particular the dictionary based compression
 * code is Copyright 1995 by Dragon's Eye Productions
 *
 */

/*
 * This file is generated by mkversion.sh. Any changes made will go away.
 */

#include "patchlevel.h"
#include "params.h"
#include "interface.h"
#include "externs.h"

const char *generation = "$generation";
const char *creation = "$creation";
const char *version = PATCHLEVEL;

const char *infotext[] =
{
    " ",
    SYSRED "ProtoMUCK " PROTOBASE " " SYSWHITE "-- " SYSCRIMSON VERSION,
#if defined(WIN32) || (WIN_VC)
    SYSYELLOW "WINDOWS" SYSBROWN
# ifdef WIN_VC
        ": Visual C++"
# else
#  ifdef WIN_CY
        ": Cygwin"
#  endif
# endif
#endif
    " ",
    SYSCYAN "Based on the original code written by these programmers:",
    "  " SYSAQUA "David Applegate    James Aspnes    Timothy Freeman    Bennet Yee",
    " ",
    SYSCYAN "Others who have provided major help along the way:",
    "  " SYSAQUA "Lachesis, ChupChups, FireFoot, Russ 'Random' Smith, Dr. Cat,",
    "  " SYSAQUA "Revar, Points, Loki, PakRat, Nodaitsu, Kaeru, Cutey_Honey,",
    "  " SYSAQUA "Zobeid",
    " ",
    SYSWHITE "ProtoMuck is derived from TinyMUCK, which was itself an extension",
    SYSWHITE
        "of TinyMUD.  Proto is an updated version of the NeonMUCK code base,",
    SYSWHITE
        "which originaly came from the FuzzBall distribution, by Foxen/Revar,",
    SYSWHITE "of TinyMUCK.",
    " ",
    SYSGREEN "And, here is the list of the programmers for ProtoMUCK:",
    SYSFOREST "  Akari    - Project Administrator and Lead Programmer",
    SYSFOREST "  Hinoserm - Programmer",
    SYSFOREST "  Alynna   - Programmer and WinProto developer",
    SYSFOREST "  Moose    - Retired Project Administrator",
    " ",
    SYSWHITE "Thanks are also due towards the multiple people who had also",
    SYSWHITE
        "contributed ideas for the MUCK as we worked on it, and helped",
    SYSWHITE "us out along the way.",
    SYSCYAN "Feel free to check ProtoMUCK's webpage at the following address:",
    SYSAQUA "http://www.protomuck.org/",
    " ",
    SYSBLUE "The Pueblo multimedia protocol is (C)Chaco Communications.",
    SYSBLUE "http://www.chaco.com/pueblo for more information.",
    SYSBLUE "The recommended client by us for any mu* server is BeipMU*.",
    SYSBLUE "http://beipmu.twu.net/ for more information.",
    CINFO "Done.",
    0,
};


void
do_credits(dbref player)
{
    int i;

    for (i = 0; infotext[i]; i++) {
        anotify_nolisten2(player, infotext[i]);
    }
}
EOF

exit 0



