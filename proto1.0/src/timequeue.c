/*
  Timequeue event code by Foxen
*/

#include "copyright.h"
#include "config.h"
#include "params.h"
#include "match.h"

#include "db.h"
#include "tune.h"
#include "mpi.h"
#include "props.h"
#include "interface.h"
#include "externs.h"
#include "mufevent.h"

#include <stdio.h>
#include <sys/types.h>
#include <time.h>


#define TQ_MUF_TYP 0
#define TQ_MPI_TYP 1

#define TQ_MUF_QUEUE    0x0
#define TQ_MUF_DELAY    0x1
#define TQ_MUF_LISTEN   0x2
#define TQ_MUF_READ     0x3
#define TQ_MUF_TREAD    0x4
#define TQ_MUF_TIMER    0x5
 
#define TQ_MPI_QUEUE    0x0
#define TQ_MPI_DELAY    0x1

#define TQ_MPI_SUBMASK  0x7
#define TQ_MPI_LISTEN   0x8
#define TQ_MPI_OMESG   0x10


/*
 * Events types and data:
 *  What, typ, sub, when, user, where, trig, prog, frame, str1, cmdstr, str3
 *  qmpi   1    0   1     user  loc    trig  --    --     mpi   cmd     arg
 *  dmpi   1    1   when  user  loc    trig  --    --     mpi   cmd     arg
 *  lmpi   1    8   1     spkr  loc    lstnr --    --     mpi   cmd     heard
 *  oqmpi  1   16   1     user  loc    trig  --    --     mpi   cmd     arg
 *  odmpi  1   17   when  user  loc    trig  --    --     mpi   cmd     arg
 *  olmpi  1   24   1     spkr  loc    lstnr --    --     mpi   cmd     heard
 *  qmuf   0    0   0     user  loc    trig  prog  --     stk_s cmd@    --
 *  lmuf   0    1   0     spkr  loc    lstnr prog  --     heard cmd@    --
 *  dmuf   0    2   when  user  loc    trig  prog  frame  mode  --      --
 *  rmuf   0    3   -1    user  loc    trig  prog  frame  mode  --      --
 *  trmuf  0    4   when  user  loc    trig  prog  frame  mode  --      --
 *  tevmuf 0    5   when  user  loc    trig  prog  frame  mode  event   --
 */


typedef struct timenode {
    struct timenode *next;
    int     typ;
    int     subtyp;
    time_t  when;
    int     descr;
    dbref   called_prog;
    char   *called_data;
    char   *command;
    char   *str3;
    dbref   uid;
    dbref   loc;
    dbref   trig;
    struct frame *fr;
    struct inst *where;
    int     eventnum;
}      *timequeue;

static timequeue tqhead = NULL;

void    prog_clean(struct frame * fr);
int	has_refs(dbref program, timequeue ptr);

static int
valid_objref(dbref obj)
{
    return (!((obj >= db_top)
	      || (obj >= 0 &&  Typeof(obj) == TYPE_GARBAGE)
	      || (obj < 0)));
}


extern int top_pid;
int     process_count = 0;

static timequeue free_timenode_list = NULL;
static int free_timenode_count = 0;

static  timequeue
alloc_timenode(int typ, int subtyp, time_t mytime, int descr, dbref player, dbref loc,
	       dbref trig, dbref program, struct frame * fr,
	       const char *strdata, const char *strcmd, const char *str3, timequeue nextone)
{
    timequeue ptr;

    if (free_timenode_list) {
	ptr = free_timenode_list;
	free_timenode_list = ptr->next;
	free_timenode_count--;
    } else {
	ptr = (timequeue) malloc(sizeof(struct timenode));
    }
    ptr->typ = typ;
    ptr->subtyp = subtyp;
    ptr->when = mytime;
    ptr->uid = player;
    ptr->loc = loc;
    ptr->trig = trig;
    ptr->descr = descr;
    ptr->fr = fr;
    ptr->called_prog = program;
    ptr->called_data = (char *) string_dup((char *) strdata);
    ptr->command = alloc_string(strcmd);
    ptr->str3 = alloc_string(str3);
    ptr->eventnum = (fr) ? fr->pid : top_pid++;
    ptr->next = nextone;
    return (ptr);
}

static void
free_timenode(timequeue ptr)
{
    if (ptr->command) free(ptr->command);
    if (ptr->called_data) free(ptr->called_data);
    if (ptr->str3) free(ptr->str3);
    if (ptr->fr) {
	if (ptr->fr->multitask != BACKGROUND)
	    DBFETCH(ptr->uid)->sp.player.block = 0;
	if (ptr->typ != TQ_MUF_TYP || ptr->subtyp != TQ_MUF_TIMER) {
	    prog_clean(ptr->fr);
	}
	if (ptr->typ == TQ_MUF_TYP && (ptr->subtyp == TQ_MUF_READ ||
					   ptr->subtyp == TQ_MUF_TREAD)) {
	    FLAGS(ptr->uid) &= ~INTERACTIVE;
	    FLAGS(ptr->uid) &= ~READMODE;
	    anotify_nolisten(ptr->uid, CINFO "Data input aborted.  The command you were using was killed.", 1);
	}
    }
    if (free_timenode_count < tp_free_frames_pool) {
	ptr->next = free_timenode_list;
	free_timenode_list = ptr;
	free_timenode_count++;
    } else {
	free(ptr);
    }
}

int
control_process(dbref player, int count)
{
    timequeue tmp, ptr = tqhead;

    tmp = ptr;
    while ((ptr) && (count != ptr->eventnum)) {
	tmp = ptr;
	ptr = ptr->next;
    }

    if (!ptr) {
	return muf_event_controls(player, count);
    }

    if (!controls(player, ptr->called_prog) && !controls(player, ptr->trig)) {
	return 0;
    }
    return 1;
}


int
add_event(int event_typ, int subtyp, int dtime, int descr, dbref player, dbref loc,
	  dbref trig, dbref program, struct frame * fr,
	  const char *strdata, const char *strcmd, const char *str3)
{
    timequeue ptr = tqhead;
    timequeue lastevent = NULL;
    time_t  rtime = time((time_t *) NULL) + (time_t) dtime;
    int mypids = 0;

    for (ptr = tqhead, mypids = 0; ptr; ptr = ptr->next) {
	if (ptr->uid == player)
		mypids++;
	lastevent = ptr;
    }

    if (event_typ == TQ_MUF_TYP && subtyp == TQ_MUF_READ) {
	process_count++;
        if (lastevent) {
            lastevent->next = alloc_timenode(event_typ, subtyp, rtime, descr,
                                             player, loc, trig, program, fr,
                                             strdata, strcmd, str3, NULL);
	    return (lastevent->next->eventnum);
        } else {
            tqhead = alloc_timenode(event_typ, subtyp, rtime, descr,
				     player, loc, trig, program, fr,
				     strdata, strcmd, str3, NULL);
	    return (tqhead->eventnum);
        }
    }
    if (!(event_typ == TQ_MUF_TYP && subtyp == TQ_MUF_TREAD)) {
      if (process_count > tp_max_process_limit ||
	      (mypids > tp_max_plyr_processes && !Mage(OWNER(player)))) {
	  if (fr) {
	      if (fr->multitask != BACKGROUND)
		  DBFETCH(player)->sp.player.block = 0;
	      prog_clean(fr);
	  }
	  anotify_nolisten(player, CINFO "Event killed.  Timequeue table full.", 1);
	  return 0;
      }
    }
    process_count++;

    if (!tqhead) {
	tqhead = alloc_timenode(event_typ, subtyp, rtime, descr, player, loc, trig,
				program, fr, strdata, strcmd, str3, NULL);
	return (tqhead->eventnum);
    }
    if (rtime < tqhead->when || (tqhead->typ == TQ_MUF_TYP && tqhead->subtyp == TQ_MUF_READ)
    ) {
	tqhead = alloc_timenode(event_typ, subtyp, rtime, descr, player, loc, trig,
				program, fr, strdata, strcmd, str3, tqhead);
	return (tqhead->eventnum);
    }

    ptr = tqhead;
    while ((ptr->next) && (rtime >= ptr->next->when) &&
	    !(ptr->next->typ == TQ_MUF_TYP && ptr->next->subtyp == TQ_MUF_READ)
    ) {
	ptr = ptr->next;
    }

    ptr->next = alloc_timenode(event_typ, subtyp, rtime, descr, player, loc, trig,
			       program, fr, strdata, strcmd, str3, ptr->next);
    return (ptr->next->eventnum);
}


int
add_mpi_event(int delay, int descr, dbref player, dbref loc, dbref trig,
	      const char *mpi, const char *cmdstr, const char *argstr,
	      int listen_p, int omesg_p)
{
    int subtyp = TQ_MPI_QUEUE;

    if (delay >= 1) {
	subtyp = TQ_MPI_DELAY;
    }
    if (listen_p)  {
	subtyp |= TQ_MPI_LISTEN;
    }
    if (omesg_p) {
	subtyp |= TQ_MPI_OMESG;
    }
    return add_event(TQ_MPI_TYP, subtyp, delay, descr, player, loc, trig,
		     NOTHING, NULL, mpi, cmdstr, argstr);
}


int
add_muf_queue_event(int descr, dbref player, dbref loc, dbref trig, dbref prog,
		    const char *argstr, const char *cmdstr, int listen_p)
{
    return add_event(TQ_MUF_TYP, (listen_p? TQ_MUF_LISTEN: TQ_MUF_QUEUE), 0,
		     descr, player, loc, trig, prog, NULL, argstr, cmdstr, NULL);
}


int
add_muf_delayq_event(int delay, int descr, dbref player, dbref loc, dbref trig,
		    dbref prog, const char *argstr, const char *cmdstr, int listen_p)
{
    return add_event(TQ_MUF_TYP, (listen_p? TQ_MUF_LISTEN: TQ_MUF_QUEUE),
                     delay, descr, player, loc, trig, prog, NULL, argstr, cmdstr, NULL);
}


int
add_muf_read_event(int descr, dbref player, dbref prog, struct frame *fr)
{
    FLAGS(player) |= (INTERACTIVE | READMODE);
    return add_event(TQ_MUF_TYP, TQ_MUF_READ, -1, descr, player, -1, fr->trig,
		     prog, fr, "READ", NULL, NULL);
}

int
add_muf_tread_event(int descr, dbref player, dbref prog, struct frame *fr, int delay)
{
	FLAGS(player) |= (INTERACTIVE | READMODE);
	return add_event(TQ_MUF_TYP, TQ_MUF_TREAD, delay, descr, player, -1, fr->trig,
					 prog, fr, "READ", NULL, NULL);
}

int
add_muf_timer_event(int descr, dbref player, dbref prog, struct frame *fr, int delay, char *id)
{
	char buf[40];
	sprintf(buf, "TIMER.%.32s", id);
	fr->timercount++;
	return add_event(TQ_MUF_TYP, TQ_MUF_TIMER, delay, descr, player, -1, fr->trig,
					 prog, fr, buf, NULL, NULL);
}

int
add_muf_delay_event(int delay, int descr, dbref player, dbref loc, dbref trig, dbref prog,
		    struct frame *fr, const char *mode)
{
    return add_event(TQ_MUF_TYP, TQ_MUF_DELAY, delay, descr, player, loc, trig,
		     prog, fr, mode, NULL, NULL);
}



void
handle_read_event(int descr, dbref player, const char *command)
{
    struct frame *fr;
    timequeue ptr, lastevent;
    int flag, typ, nothing_flag;
    dbref prog;

    nothing_flag = 0;
    if (command == NULL) {
	  nothing_flag = 1;
    }
    FLAGS(player) &= ~(INTERACTIVE | READMODE);

    ptr = tqhead;
    lastevent = NULL;
    while (ptr) {
	if (ptr->typ == TQ_MUF_TYP && (ptr->subtyp == TQ_MUF_READ ||
              						ptr->subtyp == TQ_MUF_TREAD) && ptr->uid == player) {
	    break;
	}
	lastevent = ptr;
	ptr = ptr->next;
    }

    /*
     * When execution gets to here, either ptr will point to the
     * READ event for the player, or else ptr will be NULL.
     */

    if (ptr) {
	/* remember our program, and our execution frame. */
	fr = ptr->fr;
	typ = ptr->subtyp;
	prog = ptr->called_prog;

      if (command) {
  	  /* remove the READ timequeue node from the timequeue */
	  process_count--;
	  if (lastevent) {
	      lastevent->next = ptr->next;
	  } else {
	      tqhead = ptr->next;
	  }
      }

	/* remember next timequeue node, to check for more READs later */
	lastevent = ptr;
	ptr = ptr->next;

	/* Make SURE not to let the program frame get freed.  We need it. */
	lastevent->fr = NULL;

      if (command) {
	  /*
	   * Free up the READ timequeue node
	   * we just removed from the queue.
	   */
	  free_timenode(lastevent);
      }

	if (fr->brkpt.debugging && !fr->brkpt.isread) {

	    /* We're in the MUF debugger!  Call it with the input line. */
	    if (command) {
		if (muf_debugger(descr, player, prog, command, fr)) {
			/* MUF Debugger exited.  Free up the program frame & exit */
			prog_clean(fr);
			return;
		}
	    } else {
		if (muf_debugger(descr, player, prog, "", fr)) {
			/* MUF Debugger exited.  Free up the program frame & exit */
			prog_clean(fr);
			return;
		}
	    }
	} else {
	    /* This is a MUF READ event. */
          if (command)
 	    if (!string_compare(command, BREAK_COMMAND)) {

		/* Whoops!  The user typed @Q.  Free the frame and exit. */
		prog_clean(fr);
		return;
	    }

	    if ((fr->argument.top >= STACK_SIZE) ||
			(nothing_flag && fr->argument.top >= STACK_SIZE - 1)) {

	        /*
	         * Uh oh! That MUF program's stack is full!
	         * Print an error, free the frame, and exit.
		 */
		notify_nolisten(player, "Program stack overflow.", 1);
		prog_clean(fr);
		return;
	    }

	    /*
	     * Everything looks okay.  Lets stuff the input line
	     * on the program's argument stack as a string item.
	     */
  	    fr->argument.st[fr->argument.top].type = PROG_STRING;
  	    fr->argument.st[fr->argument.top++].data.string =
                alloc_prog_string(command ? command : "" );
	    if (typ == TQ_MUF_TREAD) {
		if (nothing_flag) {
			fr->argument.st[fr->argument.top].type = PROG_INTEGER;
			fr->argument.st[fr->argument.top++].data.number = 0;
		} else {
			fr->argument.st[fr->argument.top].type = PROG_INTEGER;
			fr->argument.st[fr->argument.top++].data.number = 1;
		}
	    }
	}

	/*
	 * When using the MUF Debugger, the debugger will set the
	 * INTERACTIVE bit on the user, if it does NOT want the MUF
	 * program to resume executing.
	 */
	flag = (FLAGS(player) & INTERACTIVE);

	if (!flag) {
	    interp_loop(player, prog, fr, 0);
	}

	/*
	 * Check for any other READ events for this player.
	 * If there are any, set the READ related flags.
	 */
	while (ptr) {
	    if (ptr->typ == TQ_MUF_TYP && (ptr->subtyp == TQ_MUF_READ ||
						   ptr->subtyp == TQ_MUF_TREAD)) {
		if (ptr->uid == player) {
		    FLAGS(player) |= (INTERACTIVE | READMODE);
		}
	    }
	    ptr = ptr->next;
	}
    }
}


void
next_timequeue_event()
{
    struct frame *tmpfr;
    dbref   tmpcp;
    int     tmpbl, tmpfg;
    timequeue ptr, lastevent;
    int     maxruns = 0;
    time_t  rtime = time((time_t *) NULL);

    lastevent = tqhead;
    while ((lastevent) && (rtime >= lastevent->when) && (maxruns < 10)) {
	lastevent = lastevent->next;
	maxruns++;
    }

    while ((tqhead != lastevent) && (maxruns--) && tqhead) {
	if (tqhead->typ == TQ_MUF_TYP && tqhead->subtyp == TQ_MUF_READ) {
	    break;
	}
	tqhead->eventnum = 0;
	if (tqhead->typ == TQ_MPI_TYP) {
	    char cbuf[BUFFER_LEN];
	    int ival;

	    strcpy(match_args, tqhead->str3? tqhead->str3 : "");
	    strcpy(match_cmdname, tqhead->command? tqhead->command : "");
	    ival = (tqhead->subtyp & TQ_MPI_OMESG)? MPI_ISPUBLIC : MPI_ISPRIVATE;
	    if (tqhead->subtyp & TQ_MPI_LISTEN) {
		ival |= MPI_ISLISTENER;
		do_parse_mesg(tqhead->descr, tqhead->uid, tqhead->trig, tqhead->called_data,
			      "(MPIlisten)", cbuf, ival);
	    } else if ((tqhead->subtyp & TQ_MPI_SUBMASK) == TQ_MPI_DELAY) {
		do_parse_mesg(tqhead->descr, tqhead->uid, tqhead->trig, tqhead->called_data,
			      "(MPIdelay)", cbuf, ival);
	    } else {
		do_parse_mesg(tqhead->descr, tqhead->uid, tqhead->trig, tqhead->called_data,
			      "(MPIqueue)", cbuf, ival);
	    }
	    if (*cbuf) {
		if (!(tqhead->subtyp & TQ_MPI_OMESG)) {
		    notify_nolisten(tqhead->uid, cbuf, 1);
		} else {
		    char bbuf[BUFFER_LEN];
		    dbref plyr;

		    sprintf(bbuf, ">> %.4000s %.*s",
			    NAME(tqhead->uid),
			    (int)(4000 - strlen(NAME(tqhead->uid))),
			    pronoun_substitute(tqhead->descr, tqhead->uid, cbuf));
		    plyr = DBFETCH(tqhead->loc)->contents;
		    for (;plyr != NOTHING; plyr = DBFETCH(plyr)->next) {
			if (Typeof(plyr)==TYPE_PLAYER && plyr!=tqhead->uid)
			    notify_nolisten(plyr, bbuf, 0);
		    }
		}
	    }
	} else if (tqhead->typ == TQ_MUF_TYP) {
	    if (Typeof(tqhead->called_prog) == TYPE_PROGRAM) {
		if (tqhead->subtyp == TQ_MUF_DELAY) {
		    tmpcp = DBFETCH(tqhead->uid)->sp.player.curr_prog;
		    tmpbl = DBFETCH(tqhead->uid)->sp.player.block;
		    tmpfg = (tqhead->fr->multitask != BACKGROUND);
		    interp_loop(tqhead->uid,tqhead->called_prog,tqhead->fr,0);
		    if (!tmpfg) {
			DBFETCH(tqhead->uid)->sp.player.block = tmpbl;
		    }
		} else if (tqhead->subtyp == TQ_MUF_TIMER) {
		    struct inst temp;

		    temp.type = PROG_INTEGER;
		    temp.data.number = tqhead->when;
		    tqhead->fr->timercount--;
		    muf_event_add(tqhead->fr, tqhead->called_data, &temp);
		} else if (tqhead->subtyp == TQ_MUF_TREAD) {
		    handle_read_event(tqhead->descr, tqhead->uid, NULL);
		} else {
		    strcpy(match_args, tqhead->called_data? tqhead->called_data : "");
		    strcpy(match_cmdname, tqhead->command? tqhead->command : "");
                    tmpfr = interp(tqhead->descr, tqhead->uid, tqhead->loc, tqhead->called_prog,
                                           tqhead->trig, BACKGROUND, STD_HARDUID);
		    if (tmpfr) {
                        interp_loop(tqhead->uid, tqhead->called_prog, tmpfr, 0);
		    }
		}
	    }
	}
	ptr = tqhead;
	tqhead = tqhead->next;
	ptr->fr = NULL;
	free_timenode(ptr);
	process_count--;
    }
}


int
in_timequeue(int pid)
{
/*    timequeue ptr = tqhead;

    if (!pid) return 0;
    if (!tqhead) return 0;
    while ((ptr) && (ptr->eventnum != pid))
	ptr = ptr->next;
    if (ptr)
	return 1;
    return 0; */
	timequeue ptr = tqhead;

	if (!pid)
		return 0;
	if (muf_event_pid_frame(pid))
		return 1;
	if (!tqhead)
		return 0;
	while ((ptr) && (ptr->eventnum != pid))
		ptr = ptr->next;
	if (ptr)
		return 1;
	return 0;
}

struct frame*
timequeue_pid_frame(int pid)
{
	struct frame *out = NULL;
	timequeue ptr = tqhead;

	if (!pid)
		return NULL;
	out = muf_event_pid_frame(pid);
	if (out != NULL)
		return out;

	if (!tqhead)
		return NULL;
	while ((ptr) && (ptr->eventnum != pid))
		ptr = ptr->next;
	if (ptr)
		return ptr->fr;
	return NULL;
}

time_t
next_event_time()
{
    time_t  rtime = time((time_t *) NULL);

    if (tqhead) {
	if (tqhead->when == -1) {
	    return (-1L);
	} else if (rtime >= tqhead->when) {
	    return (0L);
	} else {
	    return ((time_t) (tqhead->when - rtime));
	}
    }
    return (-1L);
}

extern char *time_format_2(time_t dt);

void
list_events(dbref player)
{
    char    buf[BUFFER_LEN];
    char    buf2[BUFFER_LEN];
    int     count = 0;
    timequeue ptr = tqhead;
    time_t  rtime = time((time_t *) NULL);

    anotify_nolisten(player, CINFO "     PID Next  Run KInst Prog#   Player", 1);

    while (ptr) {
	strcpy(buf2, ((ptr->when - rtime) > 0) ?
	       time_format_2((time_t) (ptr->when - rtime)) : "Due");
	if (ptr->typ == TQ_MUF_TYP && ptr->subtyp == TQ_MUF_DELAY) {
	    (void) sprintf(buf, "%8d %4s %4s %5d #%-6d %-16s %.512s",
			   ptr->eventnum, buf2,
			   time_format_2((time_t) (rtime - ptr->fr->started)),
			   (ptr->fr->instcnt / 1000),
			   ptr->called_prog, NAME(ptr->uid),
			   ptr->called_data);
	} else if (ptr->typ == TQ_MUF_TYP && ptr->subtyp == TQ_MUF_READ) {
	    (void) sprintf(buf, "%8d %4s %4s %5d #%-6d %-16s %.512s",
			   ptr->eventnum, "--",
			   time_format_2((time_t) (rtime - ptr->fr->started)),
			   (ptr->fr->instcnt / 1000),
			   ptr->called_prog, NAME(ptr->uid),
			   ptr->called_data);
	} else if (ptr->typ == TQ_MUF_TYP && ptr->subtyp == TQ_MUF_TIMER) {
	    (void) sprintf(buf, "(%6d) %4s %4s %5d #%-6d %-16s %.512s",
			   ptr->eventnum, buf2,
			   time_format_2((time_t) (rtime - ptr->fr->started)),
			   (ptr->fr->instcnt / 1000),
			   ptr->called_prog, NAME(ptr->uid),
                     ptr->called_data);
	} else if (ptr->typ == TQ_MUF_TYP && ptr->subtyp == TQ_MUF_TREAD) {
	    (void) sprintf(buf, "%8d %4s %4s %5d #%-6d %-16s %.512s",
			   ptr->eventnum, buf2,
			   time_format_2((time_t) (rtime - ptr->fr->started)),
			   (ptr->fr->instcnt / 1000),
			   ptr->called_prog, NAME(ptr->uid),
                     ptr->called_data);
	} else if (ptr->typ == TQ_MPI_TYP) {
	    (void) sprintf(buf, "%8d %4s   --   MPI #%-6d %-16s \"%.512s\"",
			   ptr->eventnum, buf2, ptr->trig, NAME(ptr->uid),
			   ptr->called_data);
	} else {
	    (void) sprintf(buf, "%8d %4s   0s     0 #%-6d %-16s \"%.512s\"",
			   ptr->eventnum, buf2, ptr->called_prog,
			   NAME(ptr->uid), ptr->called_data);
	}
	if (Mage(OWNER(player)) || (OWNER(ptr->called_prog) == OWNER(player))
                || (ptr->uid == player)) {
	    notify_nolisten(player, buf, 1);
        } else if (ptr->called_prog != NOTHING && OWNER(ptr->called_prog) == OWNER(player)) {
	    notify_nolisten(player, buf, 1);
	}
	ptr = ptr->next;
	count++;
    }
    count += muf_event_list(player, "%8d %4s %4s %5d #%-6d %-16s %.512s");
    sprintf(buf, CINFO "%d events.", count);
    anotify_nolisten(player, buf, 1);
}

/*
 * Sleeponly values:
 *     0: kill all matching processes
 *     1: kill only matching sleeping processes
 *     2: kill only matching foreground processes
 */
int
dequeue_prog(dbref program, int sleeponly)
{
    int     count = 0;
    timequeue tmp, ptr;

    while (tqhead && ((tqhead->called_prog==program) ||
	    has_refs(program, tqhead) || (tqhead->uid==program))
	    && ((tqhead->fr) ? (!((tqhead->fr->multitask == BACKGROUND) &&
				  (sleeponly == 2))) : (!sleeponly))) {
	ptr = tqhead;
	tqhead = tqhead->next;
	free_timenode(ptr);
	process_count--;
	count++;
    }

    if (tqhead) {
	tmp = tqhead;
	ptr = tqhead->next;
	while (ptr) {
	    if ((ptr->called_prog == program) ||
		    (has_refs(program, ptr)) || ((ptr->uid == program)
		    && ((ptr->fr) ? (!((ptr->fr->multitask == BACKGROUND) &&
				       (sleeponly == 2))) : (!sleeponly) ))) {
		tmp->next = ptr->next;
		free_timenode(ptr);
		process_count--;
		count++;
		ptr = tmp;
	    }
	    tmp = ptr;
	    ptr = ptr->next;
	}
    }
    if (sleeponly == 1 || sleeponly == 0) {
	// treat MUF_EVENT processes as backgrounded.
	count += muf_event_dequeue(program);
    }
    for (ptr = tqhead; ptr; ptr = ptr->next) {
	if (ptr->typ == TQ_MUF_TYP && (ptr->subtyp == TQ_MUF_READ ||
						   ptr->subtyp == TQ_MUF_TREAD)) {
	    FLAGS(ptr->uid) |= (INTERACTIVE | READMODE);
	}
    }
    return (count);
}


int
dequeue_process(int pid)
{
    timequeue tmp, ptr = tqhead;
    int deqflag = 0;

    if (!pid) return 0;

    if (muf_event_dequeue_pid(pid)) {
	process_count--;
    }

    tmp = ptr;
    while (ptr) {
	if (pid == ptr->eventnum) {
		if (tmp == ptr) {
			tqhead = tmp = tmp->next;
			free_timenode(ptr);
			ptr = tmp;
		} else {
			tmp->next = ptr->next;
			free_timenode(ptr);
			ptr = tmp->next;
		}
		process_count--;
		deqflag = 1;
	} else {
		tmp = ptr;
		ptr = ptr->next;
	}
    }

/*    if (!tmp) return 0;
    if (!ptr) return 0;
    if (tmp == ptr) {
	tqhead = ptr->next;
    } else {
	tmp->next = ptr->next;
    }
    free_timenode(ptr);
    process_count--; */

    if (!deqflag) {
        return 0;
    }
    for (ptr = tqhead; ptr; ptr = ptr->next) {
	if (ptr->typ == TQ_MUF_TYP && (ptr->subtyp == TQ_MUF_READ ||
						ptr->subtyp == TQ_MUF_TREAD)) {
	    FLAGS(ptr->uid) |= (INTERACTIVE | READMODE);
	}
    }
    return 1;
}


int
dequeue_timers(int pid, char* id)
{
	char buf[40];
	timequeue tmp, ptr = tqhead;
	int deqflag = 0;

	if (!pid)
		return 0;

	if (id)
		sprintf(buf, "TIMER.%.30s", id);

	tmp = ptr;
	while (ptr) {
		if (pid == ptr->eventnum &&
			ptr->typ == TQ_MUF_TYP && ptr->subtyp == TQ_MUF_TIMER &&
			(!id || !strcmp(ptr->called_data, buf)))
		{
			if (tmp == ptr) {
				tqhead = tmp = tmp->next;
				ptr->fr->timercount--;
				free_timenode(ptr);
				ptr = tmp;
			} else {
				tmp->next = ptr->next;
				ptr->fr->timercount--;
				free_timenode(ptr);
				ptr = tmp->next;
			}
			process_count--;
			deqflag = 1;
		} else {
			tmp = ptr;
			ptr = ptr->next;
		}
	}

	return deqflag;
}


void
do_dequeue(int descr, dbref player, const char *arg1)
{
    char    buf[BUFFER_LEN];
    int     count;
    dbref   match;
    struct match_data md;
    timequeue tmp, ptr = tqhead;

    if(Guest(player)) {
	anotify_fmt(player, CFAIL "%s", tp_noguest_mesg);
	return;
    }

    if (*arg1 == '\0') {
	anotify_nolisten(player, CINFO "Dequeue which event?", 1);
    } else {
	if (!string_compare(arg1, "all")) {
	    if (!Mage(OWNER(player))) {
		anotify_fmt(player, CFAIL "%s", tp_noperm_mesg);
		return;
	    }
	    while (ptr) {
		tmp = ptr;
		ptr = ptr->next;
		free_timenode(tmp);
		process_count--;
	    }
	    tqhead = NULL;
	    muf_event_dequeue(NOTHING);
	    anotify_nolisten(player, CSUCC "Time queue cleared.", 1);
	} else {
	    if (!number(arg1)) {
		init_match(descr, player, arg1, NOTYPE, &md);
		match_absolute(&md);
		match_everything(&md);

		match = noisy_match_result(&md);
		if (match == NOTHING) {
		    anotify_nolisten(player, CINFO "I don't know what you want to dequeue!", 1);
		    return;
		}
		if (!valid_objref(match)) {
		    anotify_nolisten(player, CINFO "Invalid object.", 1);
		    return;
		}
		if (!controls(player, match)) {
		    anotify_fmt(player, CFAIL "%s", tp_noperm_mesg);
		    return;
		}
		count = dequeue_prog(match, 0);
		if (!count) {
		    anotify_nolisten(player, CINFO "That program isn't running.", 1);
		    return;
		}
		if (count > 1) {
		    sprintf(buf, CSUCC "%d processes dequeued.", count);
		} else {
		    sprintf(buf, CSUCC "Process dequeued.");
		}
		anotify_nolisten(player, buf, 1);
	    } else {
		if ((count = atoi(arg1))) {
		    if (!control_process(player, count)) {
			anotify_fmt(player, CFAIL "%s", tp_noperm_mesg);
			return;
		    }
		    if (!dequeue_process(count)) {
			anotify_nolisten(player, CINFO "No such process.", 1);
			return;
		    }
		    process_count--;
		    anotify_nolisten(player, CSUCC "Process dequeued.", 1);
		} else {
		    anotify_nolisten(player, CINFO "What process do you want to dequeue?", 1);
		}
	    }
	}
    }
    return;
}


/* Checks the MUF timequeue for address references on the stack or */
/* dbref references on the callstack */
int
has_refs(dbref program, timequeue ptr)
{
    int loop;
    if (ptr->typ != TQ_MUF_TYP || !(ptr->fr) ||
	    Typeof(program) != TYPE_PROGRAM ||
	    !(DBFETCH(program)->sp.program.instances)) {
		/*
        fprintf(stderr, "Program %s no references, terminating.\n",
                NAME(program));
				*/
	return 0;
    }

    for (loop = 1; loop < ptr->fr->caller.top; loop++) {
	if (ptr->fr->caller.st[loop] == program)
	    return 1;
    }

    for (loop = 0; loop < ptr->fr->argument.top; loop++) {
	if (ptr->fr->argument.st[loop].type == PROG_ADD &&
		ptr->fr->argument.st[loop].data.addr->progref == program)
	    return 1;
    }

    return 0;
}


int
scan_instances(dbref program)
{
    timequeue tq = tqhead;
    int i = 0, loop;
    while (tq) {
	if (tq->typ == TQ_MUF_TYP && tq->fr) {
	    if (tq->called_prog == program) {
		i++;
	    }
	    for (loop = 1; loop < tq->fr->caller.top; loop++) {
		if (tq->fr->caller.st[loop] == program)
		i++;
	    }
	    for (loop = 0; loop < tq->fr->argument.top; loop++) {
		if (tq->fr->argument.st[loop].type == PROG_ADD &&
			tq->fr->argument.st[loop].data.addr->progref == program)
		    i++;
	    }
	}
	tq = tq->next;
    }
    return i;
}


static int propq_level = 0;
void
propqueue(int descr, dbref player, dbref where, dbref trigger, dbref what, dbref xclude,
	  const char *propname, const char *toparg, int mlev, int mt)
{
    const char *tmpchar;
    const char *pname;
    dbref   the_prog;
    char    buf[BUFFER_LEN];
    char    exbuf[BUFFER_LEN];

    the_prog = NOTHING;
    tmpchar = NULL;

    /* queue up program referred to by the given property */
    if (((the_prog = get_property_dbref(what, propname)) != NOTHING) ||
	    (tmpchar = get_property_class(what, propname))) {
#ifdef COMPRESS
	if (tmpchar)
	    tmpchar = uncompress(tmpchar);
#endif
	if ((tmpchar && *tmpchar) || the_prog != NOTHING) {
	    if (tmpchar) {
		if (*tmpchar == '&') {
		    the_prog = AMBIGUOUS;
		} else if (*tmpchar == '#' && number(tmpchar+1)) {
		    the_prog = (dbref) atoi(++tmpchar);
		} else if (*tmpchar == '$') {
		    the_prog = find_registered_obj(what, tmpchar);
		} else if (number(tmpchar)) {
		    the_prog = (dbref) atoi(tmpchar);
		} else {
		    the_prog = NOTHING;
		}
	    } else {
		if (the_prog == AMBIGUOUS)
		    the_prog = NOTHING;
	    }
	    if (the_prog != AMBIGUOUS) {
		if (the_prog < 0 || the_prog >= db_top) {
		    the_prog = NOTHING;
		} else if (Typeof(the_prog) != TYPE_PROGRAM) {
		    the_prog = NOTHING;
		} else if ((OWNER(the_prog) != OWNER(player)) &&
			!(FLAGS(the_prog) & LINK_OK)) {
		    the_prog = NOTHING;
		} else if (MLevel(the_prog) < mlev) {
		    the_prog = NOTHING;
		} else if (MLevel(OWNER(the_prog)) < mlev) {
		    the_prog = NOTHING;
		} else if (the_prog == xclude) {
		    the_prog = NOTHING;
		}
	    }
	    if (propq_level < 8) {
		propq_level++;
		if (the_prog == AMBIGUOUS) {
		    char cbuf[BUFFER_LEN];
		    int ival;

		    strcpy(match_args, "");
		    strcpy(match_cmdname, toparg);
		    ival = (mt == 0)? MPI_ISPUBLIC : MPI_ISPRIVATE;
		    do_parse_mesg(descr, player, what, tmpchar+1,
				  "(MPIqueue)", cbuf, ival);
		    if (*cbuf) {
			if (mt) {
			    notify_nolisten(player, cbuf, 1);
			} else {
			    char bbuf[BUFFER_LEN];
			    dbref plyr;
			    sprintf(bbuf, ">> %.4000s",
				    pronoun_substitute(descr, player, cbuf));
			    plyr = DBFETCH(where)->contents;
			    while (plyr != NOTHING) {
				if (Typeof(plyr)==TYPE_PLAYER && plyr!=player)
				    notify_nolisten(plyr, bbuf, 0);
				plyr = DBFETCH(plyr)->next;
			    }
			}
		    }
		} else if (the_prog != NOTHING) {
		    struct frame *tmpfr;

		    strcpy(match_args, toparg? toparg : "");
		    strcpy(match_cmdname, "Queued event.");
		 /*   interp(descr, player, where, the_prog, trigger,
			   BACKGROUND, STD_HARDUID, 0); */
		    tmpfr = interp(descr, player, where, the_prog, trigger,
				       BACKGROUND, STD_HARDUID);
		    if (tmpfr) {
			interp_loop(player, the_prog, tmpfr, 0);
		    }
		}
		propq_level--;
	    } else {
		anotify_nolisten(player, CINFO "Propqueue stopped to prevent infinite loop.", 1);
	    }
	}
    }
    strcpy(buf, propname);
    if (is_propdir(what, buf)) {
	strcat(buf, "/");
	while ((pname = next_prop_name(what, exbuf, buf))) {
	    strcpy(buf, pname);
	    propqueue(descr,player,where,trigger,what,xclude,buf,toparg,mlev,mt);
	}
    }
}


void
envpropqueue(int descr, dbref player, dbref where, dbref trigger, dbref what, dbref xclude,
	     const char *propname, const char *toparg, int mlev, int mt)
{
    while (what != NOTHING) {
	propqueue(descr,player,where,trigger,what,xclude,propname,toparg,mlev,mt);
	what = getparent(what);
    }
}


void
listenqueue(int descr, dbref player, dbref where, dbref trigger, dbref what, dbref xclude,
	const char *propname, const char *toparg, int mlev, int mt, int mpi_p)
{
    const char *tmpchar;
    const char *pname, *sep, *ptr;
    dbref   the_prog = NOTHING;
    char    buf[BUFFER_LEN];
    char    exbuf[BUFFER_LEN];
    char *ptr2;

    if (!(FLAGS(what) & LISTENER) && !(FLAGS(OWNER(what)) & ZOMBIE)) return;

    the_prog = NOTHING;
    tmpchar = NULL;

    /* queue up program referred to by the given property */
    if (((the_prog = get_property_dbref(what, propname)) != NOTHING) ||
	    (tmpchar = get_property_class(what, propname))) {

	if (tmpchar) {
#ifdef COMPRESS
	    tmpchar = uncompress(tmpchar);
#endif
	    sep = tmpchar;
	    while (*sep) {
		if (*sep == '\\') {
		    sep++;
		} else if (*sep == '=') {
		    break;
		}
		if (*sep) sep++;
	    }
	    if (*sep == '=') {
		for (ptr = tmpchar, ptr2 = buf; ptr < sep; *ptr2++ = *ptr++);
		*ptr2 = '\0';
		strcpy(exbuf, toparg);
		if (!equalstr(buf, exbuf)) {
		    tmpchar = NULL;
		} else {
		    tmpchar = ++sep;
		}
	    }
	}

	if ((tmpchar && *tmpchar) || the_prog != NOTHING) {
	    if (tmpchar) {
		if (*tmpchar == '&') {
		    the_prog = AMBIGUOUS;
		} else if (*tmpchar == '#' && number(tmpchar+1)) {
		    the_prog = (dbref) atoi(++tmpchar);
		} else if (*tmpchar == '$') {
		    the_prog = find_registered_obj(what, tmpchar);
		} else if (number(tmpchar)) {
		    the_prog = (dbref) atoi(tmpchar);
		} else {
		    the_prog = NOTHING;
		}
	    } else {
		if (the_prog == AMBIGUOUS)
		    the_prog = NOTHING;
	    }
	    if (the_prog != AMBIGUOUS) {
		if (the_prog < 0 || the_prog >= db_top) {
		    the_prog = NOTHING;
		} else if (Typeof(the_prog) != TYPE_PROGRAM) {
		    the_prog = NOTHING;
		} else if (OWNER(the_prog) != OWNER(player) &&
			!(FLAGS(the_prog) & LINK_OK)) {
		    the_prog = NOTHING;
		} else if (MLevel(the_prog) < mlev) {
		    the_prog = NOTHING;
		} else if (MLevel(OWNER(the_prog)) < mlev) {
		    the_prog = NOTHING;
		} else if (the_prog == xclude) {
		    the_prog = NOTHING;
		}
	    }
	    if (the_prog == AMBIGUOUS) {
		if (mpi_p) {
		    add_mpi_event(1, descr, player, where, trigger, tmpchar+1,
			    (mt? "Listen" : "Olisten"), toparg, 1, (mt == 0));
		}
	    } else if (the_prog != NOTHING) {
		add_muf_queue_event(descr, player, where, trigger, the_prog, toparg,
				    "(_Listen)", 1);
	    }
	}
    }
    strcpy(buf, propname);
    if (is_propdir(what, buf)) {
	strcat(buf, "/");
	while ((pname = next_prop_name(what, exbuf, buf))) {
	    strcpy(buf, pname);
	    listenqueue(descr, player, where, trigger, what, xclude, buf,
			toparg, mlev, mt, mpi_p);
	}
    }
}




