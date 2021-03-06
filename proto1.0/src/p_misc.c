/* Primitives package */

#include "copyright.h"
#include "config.h"

#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include "db.h"
#include "inst.h"
#include "externs.h"
#include "match.h"
#include "interface.h"
#include "params.h"
#include "tune.h"
#include "strings.h"
#include "interp.h"

extern struct inst *oper1, *oper2, *oper3, *oper4;
extern struct inst temp1, temp2, temp3;
extern int tmp, result;
extern dbref ref;
extern char buf[BUFFER_LEN];
struct tm *time_tm;

void 
prim_time(PRIM_PROTOTYPE)
{
    CHECKOP(0);
    CHECKOFLOW(3);
    {
	time_t  lt;

	lt = time((time_t *) 0);
	time_tm = localtime(&lt);
	result = time_tm->tm_sec;
	PushInt(result);
	result = time_tm->tm_min;
	PushInt(result);
	result = time_tm->tm_hour;
	PushInt(result);
    }
}


void
prim_date(PRIM_PROTOTYPE)
{
    CHECKOP(0);
    CHECKOFLOW(3);
    {
	time_t  lt;

	lt = time((time_t *) 0);
	time_tm = localtime(&lt);
	result = time_tm->tm_mday;
	PushInt(result);
	result = time_tm->tm_mon + 1;
	PushInt(result);
	result = time_tm->tm_year + 1900;
	PushInt(result);
    }
}

void
prim_logstatus(PRIM_PROTOTYPE)
{
    CHECKOP(1);
    oper1 = POP();
    if (mlev < LARCH)
       abort_interp("Archwizard primitive.");
    if (oper1->type != PROG_STRING)
       abort_interp("Non-string argument (1).");
    if (oper1->data.string)
    {
       strcpy(buf, oper1->data.string->data);
       log_status("%s\r\n",buf);
    }
    CLEAR(oper1);
}

void
prim_gmtoffset(PRIM_PROTOTYPE)
{
    CHECKOP(0);
    CHECKOFLOW(1);
    result = get_tz_offset();
    PushInt(result);
}

void 
prim_systime(PRIM_PROTOTYPE)
{
    CHECKOP(0);
    result = time(NULL);
    CHECKOFLOW(1);
    PushInt(result);
}


void 
prim_timesplit(PRIM_PROTOTYPE)
{
    CHECKOP(1);
    oper1 = POP();		/* integer: time */
    if (oper1->type != PROG_INTEGER)
	abort_interp("Invalid argument");
    time_tm = localtime((time_t *) (&(oper1->data.number)));
    CHECKOFLOW(8);
    CLEAR(oper1);
    result = time_tm->tm_sec;
    PushInt(result);
    result = time_tm->tm_min;
    PushInt(result);
    result = time_tm->tm_hour;
    PushInt(result);
    result = time_tm->tm_mday;
    PushInt(result);
    result = time_tm->tm_mon + 1;
    PushInt(result);
    result = time_tm->tm_year + 1900;
    PushInt(result);
    result = time_tm->tm_wday + 1;
    PushInt(result);
    result = time_tm->tm_yday + 1;
    PushInt(result);
}


void 
prim_timefmt(PRIM_PROTOTYPE)
{
    CHECKOP(2);
    oper2 = POP();		/* integer: time */
    oper1 = POP();		/* string: format */
    if (oper1->type != PROG_STRING)
	abort_interp("Invalid argument (1)");
    if (oper1->data.string == (struct shared_string *) NULL)
	abort_interp("Illegal NULL string (1)");
    if (oper2->type != PROG_INTEGER)
	abort_interp("Invalid argument (2)");
    time_tm = localtime((time_t *) (&(oper2->data.number)));
    if (!format_time(buf, BUFFER_LEN, oper1->data.string->data, time_tm))
	abort_interp("Operation would result in overflow");
    CHECKOFLOW(1);
    CLEAR(oper1);
    CLEAR(oper2);
    PushString(buf);
}


void 
prim_queue(PRIM_PROTOTYPE)
{
    dbref temproom;

    /* int dbref string -- */
    CHECKOP(3);
    oper1 = POP();
    oper2 = POP();
    oper3 = POP();
    if (mlev < LM3)
	abort_interp("M3 prim");
    if (oper3->type != PROG_INTEGER)
	abort_interp("Non-integer argument (1)");
    if (oper2->type != PROG_OBJECT)
	abort_interp("Argument must be a dbref (2)");
    if (!valid_object(oper2))
	abort_interp("Invalid dbref (2)");
    if (Typeof(oper2->data.objref) != TYPE_PROGRAM)
	abort_interp("Object must be a program (2)");
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string argument (3)");

    if ((oper4 = fr->variables + 1)->type != PROG_OBJECT)
	temproom = DBFETCH(player)->location;
    else
	temproom = oper4->data.objref;

    result = add_muf_delayq_event(oper3->data.number, fr->descr, player, temproom,
		    NOTHING, oper2->data.objref, DoNullInd(oper1->data.string),
		     "Queued Event.", 0);

    CLEAR(oper1);
    CLEAR(oper2);
    CLEAR(oper3);
    PushInt(result);
}


void 
prim_kill(PRIM_PROTOTYPE)
{
    /* i -- i */
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type != PROG_INTEGER)
	abort_interp("Non-integer argument (1)");
    if (oper1->data.number == fr->pid) {
	do_abort_silent();
    } else {
	if (mlev < LMAGE) {
	    if (!control_process(ProgUID, oper1->data.number)) {
		abort_interp(tp_noperm_mesg);
            }
        }
        result = dequeue_process(oper1->data.number);
    }
    CLEAR(oper1);
    PushInt(result);
}


void 
prim_force(PRIM_PROTOTYPE)
{
    struct inst *oper1, *oper2;

    /* d s -- */
    CHECKOP(2);
    oper1 = POP();		/* string to @force */
    oper2 = POP();		/* player dbref */
    if (mlev < LARCH)
	abort_interp("Arch prim");
    if (fr->level > 8)
	abort_interp("Interp call loops not allowed");
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string argument (2)");
    if (oper2->type != PROG_OBJECT)
	abort_interp("Non-object argument (1)");
    ref = oper2->data.objref;
    if (ref < 0 || ref >= db_top)
	abort_interp("Invalid object to force (1)");
    if (Typeof(ref) != TYPE_PLAYER && Typeof(ref) != TYPE_THING)
	abort_interp("Object to force not a thing or player (1)");
    if (!oper1->data.string)
	abort_interp("Null string argument (2)");
    if (index(oper1->data.string->data, '\r'))
	abort_interp("Carriage returns not allowed in command string (2)");
    if (Man(oper2->data.objref) && !Man(OWNER(program)))
	abort_interp("Cannot force the man (1)");
    force_level++;
    process_command(dbref_first_descr(oper2->data.objref), oper2->data.objref, oper1->data.string->data);
    force_level--;
    CLEAR(oper1);
    CLEAR(oper2);
}


void 
prim_timestamps(PRIM_PROTOTYPE)
{
    CHECKOP(1);
    oper1 = POP();
    if (mlev < LM2)
	abort_interp("M2 prim");
    if (oper1->type != PROG_OBJECT)
	abort_interp("Non-object argument (1)");
    ref = oper1->data.objref;
    if (ref >= db_top || ref <= NOTHING)
      abort_interp("Dbref is not an object nor garbage.");
/*    if (!valid_object(oper1))
	abort_interp("Invalid object"); */
/*    CHECKREMOTE(oper1->data.objref); */
    CHECKREMOTE(ref);
    CHECKOFLOW(4);
/*    ref = oper1->data.objref; */
    CLEAR(oper1);
    result = DBFETCH(ref)->ts.created;
    PushInt(result);
    result = DBFETCH(ref)->ts.modified;
    PushInt(result);
    result = DBFETCH(ref)->ts.lastused;
    PushInt(result);
    result = DBFETCH(ref)->ts.usecount;
    PushInt(result);
}

extern  top_pid;

void 
prim_fork(PRIM_PROTOTYPE)
{
    int     i, j;
    struct frame *tmpfr;

    CHECKOP(0);
    CHECKOFLOW(1);

    if (mlev < LMAGE)
	abort_interp("Mage prim");

    fr->pc = pc;

    tmpfr = (struct frame *) calloc(1, sizeof(struct frame));

    tmpfr->system.top = fr->system.top;
    for (i = 0; i < fr->system.top; i++)
	tmpfr->system.st[i] = fr->system.st[i];

    tmpfr->argument.top = fr->argument.top;
    for (i = 0; i < fr->argument.top; i++)
	copyinst(&fr->argument.st[i], &tmpfr->argument.st[i]);

    tmpfr->caller.top = fr->caller.top;
    for (i = 0; i <= fr->caller.top; i++) {
	tmpfr->caller.st[i] = fr->caller.st[i];
	if (i > 0) DBFETCH(fr->caller.st[i])->sp.program.instances++;
    }

    for (i = 0; i < MAX_VAR; i++)
	copyinst(&fr->variables[i], &tmpfr->variables[i]);

    tmpfr->varset.top = fr->varset.top;
    for (i = fr->varset.top; i >= 0; i--) {
	tmpfr->varset.st[i] = (vars *) calloc(1, sizeof(vars));
	for (j = 0; j < MAX_VAR; j++)
	    copyinst(&((*fr->varset.st[i])[j]), &((*tmpfr->varset.st[i])[j]));
    }

    tmpfr->pc = pc;
    tmpfr->pc++;
    tmpfr->level = fr->level;
    tmpfr->already_created = fr->already_created;
    tmpfr->trig = fr->trig;

    tmpfr->brkpt.debugging = 0;
    tmpfr->brkpt.count = 0;
    tmpfr->brkpt.showstack = 0;
    tmpfr->brkpt.isread = 0;
    tmpfr->brkpt.bypass = 0;
    tmpfr->brkpt.lastcmd = NULL;

    tmpfr->pid = top_pid++;
    tmpfr->multitask = BACKGROUND;
    tmpfr->writeonly = 1;
    tmpfr->started = time(NULL);
    tmpfr->instcnt = 0;

    /* child process gets a 0 returned on the stack */
    result = 0;
    push(tmpfr->argument.st, &(tmpfr->argument.top),
	 PROG_INTEGER, MIPSCAST & result);

    result = add_muf_delay_event(0, fr->descr, player, NOTHING, NOTHING, program,
				tmpfr, "BACKGROUND");

    /* parent process gets the child's pid returned on the stack */
    if (!result)
	result = -1;
    PushInt(result);
}


void 
prim_pid(PRIM_PROTOTYPE)
{
    CHECKOP(0);
    CHECKOFLOW(1);
    result = fr->pid;
    PushInt(result);
}


void 
prim_stats(PRIM_PROTOTYPE)
{
    /* A WhiteFire special. :) */
    CHECKOP(1);
    oper1 = POP();
    if (mlev < LM2)
	abort_interp("M2 prim");
    if (!valid_player(oper1) && (oper1->data.objref != NOTHING))
	abort_interp("non-player argument (1)");
    ref = oper1->data.objref;
    CLEAR(oper1);
    {
	dbref   i;
	int     rooms, exits, things, players, programs, garbage;

	/* tmp, ref */
	rooms = exits = things = players = programs = garbage = 0;
	for (i = 0; i < db_top; i++) {
	    if (ref == NOTHING || OWNER(i) == ref) {
		switch (Typeof(i)) {
		    case TYPE_ROOM:
			rooms++;
			break;
		    case TYPE_EXIT:
			exits++;
			break;
		    case TYPE_THING:
			things++;
			break;
		    case TYPE_PLAYER:
			players++;
			break;
		    case TYPE_PROGRAM:
			programs++;
			break;
		    case TYPE_GARBAGE:
			garbage++;
			break;
		}
	    }
	}
	ref = rooms + exits + things + players + programs + garbage;
	PushInt(ref);
	PushInt(rooms);
	PushInt(exits);
	PushInt(things);
	PushInt(programs);
	PushInt(players);
	PushInt(garbage);
	/* push results */
    }
}

void 
prim_abort(PRIM_PROTOTYPE)
{
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Invalid argument");
    strcpy(buf, DoNullInd(oper1->data.string));
    abort_interp(buf);
}


void 
prim_ispidp(PRIM_PROTOTYPE)
{
    /* i -- i */
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type != PROG_INTEGER)
	abort_interp("Non-integer argument (1)");
    if (oper1->data.number == fr->pid) {
	result = 1;
    } else {
        result = in_timequeue(oper1->data.number);
    }
    CLEAR(oper1);
    PushInt(result);
}


void 
prim_parselock(PRIM_PROTOTYPE)
{
    struct boolexp *lok;

    CHECKOP(1);
    oper1 = POP();		/* string: lock string */
    CHECKOFLOW(1);
    if (oper1->type != PROG_STRING)
	abort_interp("Invalid argument");
    if (oper1->data.string != (struct shared_string *) NULL) {
	lok = parse_boolexp(fr->descr, ProgUID, oper1->data.string->data, 0);
    } else {
	lok = TRUE_BOOLEXP;
    }
    CLEAR(oper1);
    PushLock(lok);
}


void 
prim_unparselock(PRIM_PROTOTYPE)
{
    const char *ptr;

    CHECKOP(1);
    oper1 = POP();		/* lock: lock */
    if (oper1->type != PROG_LOCK)
	abort_interp("Invalid argument");
    if (oper1->data.lock != (struct boolexp *) TRUE_BOOLEXP) {
	ptr = unparse_boolexp(ProgUID, oper1->data.lock, 0);
    } else {
	ptr = NULL;
    }
    CHECKOFLOW(1);
    CLEAR(oper1);
    if (ptr) {
	PushString(ptr);
    } else {
	PushNullStr;
    }
}


void 
prim_prettylock(PRIM_PROTOTYPE)
{
    const char *ptr;

    CHECKOP(1);
    oper1 = POP();		/* lock: lock */
    if (oper1->type != PROG_LOCK)
	abort_interp("Invalid argument");
    ptr = unparse_boolexp(ProgUID, oper1->data.lock, 1);
    CHECKOFLOW(1);
    CLEAR(oper1);
    PushString(ptr);
}


void 
prim_testlock(PRIM_PROTOTYPE)
{
    /* d d - i */
    CHECKOP(2);
    oper1 = POP();		/* boolexp lock */
    oper2 = POP();		/* player dbref */
    if (fr->level > 8)
	abort_interp("Interp call loops not allowed");
    if (!valid_object(oper2))
	abort_interp("Invalid argument (1).");
    if (Typeof(oper2->data.objref) != TYPE_PLAYER &&
        Typeof(oper2->data.objref) != TYPE_THING )
    {
	abort_interp("Invalid object type (1).");
    }
    CHECKREMOTE(oper2->data.objref);
    if (oper1->type != PROG_LOCK)
	abort_interp("Invalid argument (2)");
    result = eval_boolexp(fr->descr, oper2->data.objref, oper1->data.lock, player);
    CLEAR(oper1);
    CLEAR(oper2);
    PushInt(result);
}

void 
prim_sysparm(PRIM_PROTOTYPE)
{
    const char *ptr;
    const char *tune_get_parmstring(const char *name, int mlev);

    CHECKOP(1);
    oper1 = POP();		/* string: system parm name */
    if (oper1->type != PROG_STRING)
	abort_interp("Invalid argument");
    if (oper1->data.string) {
	ptr = tune_get_parmstring(oper1->data.string->data, mlev);
    } else {
	ptr = "";
    }
    CHECKOFLOW(1);
    CLEAR(oper1);
    PushString(ptr);
}

void
prim_setsysparm(PRIM_PROTOTYPE)
{
	CHECKOP(2);
	oper1 = POP();				/* string: new parameter value */
	oper2 = POP();				/* string: parameter to tune */

	if (mlev < 6)
		abort_interp("Archwizards or higher only.");
	if (oper2->type != PROG_STRING)
		abort_interp("Invalid argument. (1)");
	if (!oper2->data.string)
		abort_interp("Null string argument. (1)");
	if (oper1->type != PROG_STRING)
		abort_interp("Invalid argument. (2)");
	if (!oper1->data.string)
		abort_interp("Null string argument. (2)");

	result = tune_setparm(program, oper2->data.string->data, oper1->data.string->data);

	switch (result) {
	case 0:					/* TUNESET_SUCCESS */
		log_status("TUNED (MUF): %s(%d) tuned %s to %s\n",
				   NAME(player), player, oper2->data.string->data, oper1->data.string->data);
		break;
	case 1:					/* TUNESET_UNKNOWN */
		abort_interp("Unknown parameter. (1)");
		break;
	case 2:					/* TUNESET_SYNTAX */
		abort_interp("Bad parameter syntax. (2)");
		break;
	case 3:					/* TUNESET_BADVAL */
		abort_interp("Bad parameter value. (2)");
		break;
	}
	CLEAR(oper1);
	CLEAR(oper2);
}


void
prim_cancallp(PRIM_PROTOTYPE)
{
	CHECKOP(2);
	oper2 = POP();				/* string: public function name */
	oper1 = POP();				/* dbref: Program dbref to check */
	if (oper1->type != PROG_OBJECT)
		abort_interp("Expected dbref argument. (1)");
	if (!valid_object(oper1))
		abort_interp("Invalid dbref (1)");
	if (Typeof(oper1->data.objref) != TYPE_PROGRAM)
		abort_interp("Object is not a MUF Program. (1)");
	if (oper2->type != PROG_STRING)
		abort_interp("Expected string argument. (2)");
	if (!oper2->data.string)
		abort_interp("Invalid Null string argument. (2)");

	if (!(DBFETCH(oper1->data.objref)->sp.program.code)) {
		struct line *tmpline;

		tmpline = DBFETCH(oper1->data.objref)->sp.program.first;
		DBFETCH(oper1->data.objref)->sp.program.first = ( (struct line *) read_program(oper1->data.objref) );
		do_compile(fr->descr, OWNER(oper1->data.objref), oper1->data.objref);
		free_prog_text(DBFETCH(oper1->data.objref)->sp.program.first);
		DBFETCH(oper1->data.objref)->sp.program.first = tmpline;
	}

	result = 0;
	if (ProgMLevel(oper1->data.objref) > 0 &&
		(mlev >= 4 || OWNER(oper1->data.objref) == ProgUID || Linkable(oper1->data.objref))
			) {
		struct publics *pbs;

		pbs = DBFETCH(oper1->data.objref)->sp.program.pubs;
		while (pbs) {
			if (!string_compare(oper2->data.string->data, pbs->subname))
				break;
			pbs = pbs->next;
		}
		if (pbs && mlev >= pbs->mlev)
			result = 1;
	}
	CHECKOFLOW(1);
	CLEAR(oper1);
	CLEAR(oper2);
	PushInt(result);
}



void
prim_timer_start(PRIM_PROTOTYPE)
{
	CHECKOP(2);
	oper2 = POP();				/* string: timer id */
	oper1 = POP();				/* int: delay length in seconds */

	if (fr->timercount > tp_process_timer_limit)
		abort_interp("Too many timers!");
	if (oper1->type != PROG_INTEGER)
		abort_interp("Expected an integer delay time. (1)");
	if (oper2->type != PROG_STRING)
		abort_interp("Expected a string timer id. (2)");

    dequeue_timers(fr->pid, DoNullInd(oper2->data.string));
	add_muf_timer_event(fr->descr, player, program, fr, oper1->data.number, DoNullInd(oper2->data.string));

	CLEAR(oper1);
	CLEAR(oper2);
}


void
prim_timer_stop(PRIM_PROTOTYPE)
{
	CHECKOP(1);
	oper1 = POP();				/* string: timer id */

	if (oper1->type != PROG_STRING)
		abort_interp("Expected a string timer id. (2)");

    dequeue_timers(fr->pid, DoNullInd(oper1->data.string));

	CLEAR(oper1);
}


void
prim_event_count(PRIM_PROTOTYPE)
{
	CHECKOFLOW(1);
	result = muf_event_count(fr);
	PushInt(result);
}


void
prim_event_send(PRIM_PROTOTYPE)
{
      struct frame *destfr;
      stk_array *arr;
      struct inst temp1;

	CHECKOP(3);
	oper3 = POP();				/* any: data to pass */
	oper2 = POP();				/* string: event id */
	oper1 = POP();				/* int: process id to send to */

	if (mlev < 3)
		abort_interp("Requires Mucker level 3 or better.");
	if (oper1->type != PROG_INTEGER)
		abort_interp("Expected an integer process id. (1)");
	if (oper2->type != PROG_STRING)
		abort_interp("Expected a string event id. (2)");

      destfr = (struct frame*) timequeue_pid_frame(oper1->data.number);
	if (destfr) {
		arr = new_array_dictionary();
		array_set_strkey(&arr, "data", oper3);
		array_set_strkey_intval(&arr, "caller_pid", fr->pid);
		array_set_strkey_refval(&arr, "caller_prog", program);

		temp1.type = PROG_ARRAY;
		temp1.data.array = arr; 

		sprintf(buf, "USER.%.32s", DoNullInd(oper2->data.string));
		muf_event_add(destfr, buf, oper3);
	}

      CLEAR(oper1);
	CLEAR(oper2);
	CLEAR(oper3);
}



