#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#ifndef WIN32
# include <sys/wait.h>
#endif

#include "db.h"
#include "props.h"
#include "params.h"
#include "tune.h"
#include "interface.h"
#include "match.h"
#include "msgparse.h"
#include "externs.h"
#include "strings.h"

/* declarations */
static const char *dumpfile = 0;
static int epoch = 0;
FILE   *input_file;
FILE   *delta_infile;
FILE   *delta_outfile;
char   *in_filename;

void 
do_dump(dbref player, const char *newfile)
{
    char    buf[BUFFER_LEN];

    if (Wiz(player)) {
	if (*newfile && Man(player)) {
	    if (dumpfile) free( (void *)dumpfile );
	    dumpfile = alloc_string(newfile);
	    sprintf(buf, CINFO "Dumping to file %s...", dumpfile);
	} else {
	    sprintf(buf, CINFO "Dumping to file %s...", dumpfile);
	}
	anotify_nolisten2(player, buf);
	dump_db_now();
	anotify_nolisten2(player, CINFO "Done.");
    } else {
	anotify_fmt(player, CFAIL "%s", tp_noperm_mesg);
    }
}

#ifdef DELTADUMPS
void 
do_delta(dbref player)
{
    if (Mage(player)) {
	anotify_nolisten2(player, CINFO "Dumping deltas...");
	delta_dump_now();
	anotify_nolisten2(player, CINFO "Done.");
    } else
	anotify_fmt(player, CFAIL "%s", tp_noperm_mesg);
}
#endif

void 
do_shutdown(dbref player, const char *msg)
{
    if (Arch(player)) {
      if( *msg == '\0' || strcmp(msg, tp_muckname))
      {
	notify(player, "Usage: @shutdown muckname" );
	return;
      }
	log_status("SHUT: by %s\n", unparse_object(player, player));
	shutdown_flag = 1;
	restart_flag = 0;
    } else {
	anotify_fmt(player, CFAIL "%s", tp_noperm_mesg);
	log_status("SHAM: Shutdown by %s\n", unparse_object(player, player));
    }
}

void 
do_restart(dbref player, const char *msg)
{
    if (Arch(player)) {
      if( *msg == '\0' || strcmp(msg, tp_muckname))
      {
	notify(player, "Usage: @restart muckname" );
	return;
      }
	log_status("REST: by %s\n", unparse_object(player, player));
	shutdown_flag = 1;
	restart_flag = 1;
    } else {
	anotify_fmt(player, CFAIL "%s", tp_noperm_mesg);
	log_status("SHAM: Restart by %s\n", unparse_object(player, player));
    }
}


#ifdef DISKBASE
extern int propcache_hits;
extern int propcache_misses;
#endif

static void 
dump_database_internal(void)
{
    char    tmpfile[2048];
    char    timestring[1024];
    struct tm *timestamp;
    time_t curtime;
    FILE   *f;
   
    curtime = time((time_t *)NULL);
    timestamp = localtime(&curtime); 

    tune_save_parmsfile();

    format_time(timestring, 1024, "%Y.%m.%d", timestamp);

    if (tp_dbdump_warning)
	wall_and_flush(tp_dumping_mesg);

#ifndef KEEPDUMPS
    sprintf(tmpfile, "%s.#%d#", dumpfile, epoch - 1);
    (void) unlink(tmpfile);     /* nuke our predecessor */
#else
/*
    sprintf(tmpfile, "backup/%s.%s.#%d#", dumpfile, timestring, epoch - 1);
 */
    sprintf(tmpfile, "backup/%s.#%d#", dumpfile, epoch - 1);
    (void) rename(dumpfile, tmpfile);
    sprintf(tmpfile, "backup/%s.#%d#", dumpfile, epoch - 10);
    (void) unlink(tmpfile);
#endif

    sprintf(tmpfile, "%s.#%d#", dumpfile, epoch);

    if ((f = fopen(tmpfile, "w")) != NULL) {
	db_write(f);
	fclose(f);
#ifdef WIN32
	if (unlink(dumpfile))
		perror(dumpfile);
#endif
	if (rename(tmpfile, dumpfile) < 0)
	    perror(tmpfile);

#ifdef DISKBASE

#ifdef FLUSHCHANGED
	fclose(input_file);
	free((void *)in_filename);
	in_filename = string_dup(dumpfile);
	if ((input_file = fopen(in_filename, "r")) == NULL)
	    perror(dumpfile);

#ifdef DELTADUMPS
	fclose(delta_outfile);
	if ((delta_outfile = fopen(DELTAFILE_NAME, "w")) == NULL)
	    perror(DELTAFILE_NAME);

	fclose(delta_infile);
	if ((delta_infile = fopen(DELTAFILE_NAME, "r")) == NULL)
	    perror(DELTAFILE_NAME);
#endif
#endif

#endif

    } else {
	perror(tmpfile);
    }

    /* Write out the macros */

    sprintf(tmpfile, "%s.#%d#", MACRO_FILE, epoch - 1);
    (void) unlink(tmpfile);

    sprintf(tmpfile, "%s.#%d#", MACRO_FILE, epoch);

    if ((f = fopen(tmpfile, "w")) != NULL) {
	macrodump(macrotop, f);
	fclose(f);
#ifdef WIN32
	if (unlink(MACRO_FILE))
		perror(MACRO_FILE);
#endif
	if (rename(tmpfile, MACRO_FILE) < 0)
	    perror(tmpfile);
    } else {
	perror(tmpfile);
    }

    if (tp_dbdump_warning)
	wall_and_flush(tp_dumpdone_mesg);
#ifdef DISKBASE
    propcache_hits = 0L;
    propcache_misses = 1L;
#endif

    if (tp_periodic_program_purge)
	free_unused_programs();
#ifdef DISKBASE
    dispose_all_oldprops();
#endif
}

void 
panic(const char *message)
{
    char    panicfile[2048];
    FILE   *f;

    log_status("PANIC: %s\n", message);
    fprintf(stderr, "PANIC: %s\n", message);

    /* shut down interface */
    emergency_shutdown();

    /* dump panic file */
    sprintf(panicfile, "%s.PANIC", dumpfile);
    if ((f = fopen(panicfile, "w")) == NULL) {
	perror("CANNOT OPEN PANIC FILE, YOU LOSE");

#if defined(NOCOREDUMP) || defined(WINNT)
	_exit(135);
#else                           /* !NOCOREDUMP */
# ifdef SIGIOT
	signal(SIGIOT, SIG_DFL);
# endif
	abort();
#endif                          /* NOCOREDUMP */
    } else {
	log_status("DUMP: %s\n", panicfile);
	fprintf(stderr, "DUMP: %s\n", panicfile);
	db_write(f);
	fclose(f);
	log_status("DUMP: %s (done)\n", panicfile);
	fprintf(stderr, "DUMP: %s (done)\n", panicfile);
	(void) unlink(DELTAFILE_NAME);
    }

    /* Write out the macros */
    sprintf(panicfile, "%s.PANIC", MACRO_FILE);
    if ((f = fopen(panicfile, "w")) != NULL) {
	macrodump(macrotop, f);
	fclose(f);
    } else {
	perror("CANNOT OPEN MACRO PANIC FILE, YOU LOSE");
#if defined(NOCOREDUMP) || defined(WINNT)
	_exit(135);
#else                           /* !NOCOREDUMP */
# ifdef SIGIOT
	signal(SIGIOT, SIG_DFL);
# endif
	abort();
#endif                          /* NOCOREDUMP */
    }

#if defined(NOCOREDUMP) || defined(WINNT)
    _exit(136);
#else                           /* !NOCOREDUMP */
# ifdef SIGIOT
	signal(SIGIOT, SIG_DFL);
# endif
    abort();
#endif                          /* NOCOREDUMP */
}

void 
dump_database(void)
{
    epoch++;

    log_status("DUMP: %s.#%d#\n", dumpfile, epoch);
    dump_database_internal();
    log_status("DUMP: %s.#%d# (done)\n", dumpfile, epoch);
}

time_t last_monolithic_time = 0;

/*
 * Named "fork_and_dump()" mostly for historical reasons...
 */
void fork_and_dump(void)
{
    epoch++;

    last_monolithic_time = time(NULL);
    log_status("DUMP: %s.#%d#\n", dumpfile, epoch);

    dump_database_internal();
    host_save();
}

#ifdef DELTADUMPS
extern deltas_count;

int
time_for_monolithic(void)
{
    dbref i;
    int count = 0;
    int a, b;

    if (!last_monolithic_time)
	last_monolithic_time = time(NULL);
    if (time(NULL) - last_monolithic_time >=
	    (tp_monolithic_interval - tp_dump_warntime)
    ) {
	return 1;
    }

    for (i = 0; i < db_top; i++)
	if (FLAGS(i) & (SAVED_DELTA | OBJECT_CHANGED)) count++;
    if (((count * 100) / db_top)  > tp_max_delta_objs) {
	return 1;
    }

    fseek(delta_infile, 0L, 2);
    a = ftell(delta_infile);
    fseek(input_file, 0L, 2);
    b = ftell(input_file);
    if (a >= b) {
	return 1;
    }
    return 0;
}
#endif

void
dump_warning(void)
{
    if (tp_dbdump_warning) {
#ifdef DELTADUMPS
	if (time_for_monolithic()) {
	    wall_and_flush(tp_dumpwarn_mesg);
	} else {
	    if (tp_deltadump_warning) {
		wall_and_flush(tp_deltawarn_mesg);
	    }
	}
#else
	wall_and_flush(tp_dumpwarn_mesg);
#endif
    }
}

#ifdef DELTADUMPS
void
dump_deltas(void)
{
    if (time_for_monolithic()) {
	fork_and_dump();
	deltas_count = 0;
	return;
    }

    epoch++;
    log_status("DELT: %s.#%d#\n", dumpfile, epoch);

    if (tp_deltadump_warning)
	wall_and_flush(tp_dumpdeltas_mesg);

    db_write_deltas(delta_outfile);

    if (tp_deltadump_warning)
	wall_and_flush(tp_dumpdone_mesg);
#ifdef DISKBASE
    propcache_hits = 0L;
    propcache_misses = 1L;
#endif
    host_save();
}
#endif

extern short db_conversion_flag;

int 
init_game(const char *infile, const char *outfile)
{
    FILE   *f;

    if ((f = fopen(MACRO_FILE, "r")) == NULL)
	log_status("INIT: Macro storage file %s is tweaked.\n", MACRO_FILE);
    else {
	macroload(f);
	fclose(f);
    }

    in_filename = (char *)string_dup(infile);
    if ((input_file = fopen(infile, "r")) == NULL)
	return -1;

#ifdef DELTADUMPS
    if ((delta_outfile = fopen(DELTAFILE_NAME, "w")) == NULL)
	return -1;

    if ((delta_infile = fopen(DELTAFILE_NAME, "r")) == NULL)
	return -1;
#endif

    db_free();
    init_primitives();                 /* init muf compiler */
#ifdef MPI
    mesg_init();                       /* init mpi interpreter */
#endif
    SRANDOM(getpid());                 /* init random number generator */
    tune_load_parmsfile(NOTHING);      /* load @tune parms from file */

    /* ok, read the db in */
    log_status("LOAD: %s\n", infile);
    fprintf(stderr, "LOAD: %s\n", infile);
    if (db_read(input_file) < 0)
	return -1;
    log_status("LOAD: %s (done)\n", infile);
    fprintf(stderr, "LOAD: %s (done)\n", infile);

#ifndef DISKBASE
    /* everything ok */
    fclose(input_file);
#endif

    tune_load_parmsfile(NOTHING);      /* load @tune parms from file */
    /* set up dumper */
    if (dumpfile)
	free((void *) dumpfile);
    dumpfile = alloc_string(outfile);

    if (!db_conversion_flag) {
	/* initialize the ~sys/startuptime property */
	add_property((dbref)0, "~sys/startuptime", NULL,
		     (int)time((time_t *) NULL));
	add_property((dbref)0, "~sys/maxpennies", NULL, tp_max_pennies);
	add_property((dbref)0, "~sys/dumpinterval", NULL, tp_dump_interval);
	add_property((dbref)0, "~sys/max_connects", NULL, 0);
#ifdef ALLOW_OLD_TRIGGERS
	add_property((dbref)0, "_sys/startuptime", NULL,
		     (int)time((time_t *) NULL));
	add_property((dbref)0, "_sys/maxpennies", NULL, tp_max_pennies);
	add_property((dbref)0, "_sys/dumpinterval", NULL, tp_dump_interval);
	add_property((dbref)0, "_sys/max_connects", NULL, 0);
#endif
    }

    return 0;
}


extern short wizonly_mode;
void
do_restrict(dbref player, const char *arg)
{
    if (!Wiz(player)) {
	notify(player, tp_noperm_mesg);
	return;
    }

    if (!strcmp(arg, "on")) {
	wizonly_mode = 1;
	notify(player, "Login access is now restricted to wizards only.");
    } else if (!strcmp(arg, "off")) {
	wizonly_mode = 0;
	notify(player, "Login access is now unrestricted.");
    } else {
	notify(player, "Argument must be 'on' or 'off'.");
    }
}


/* use this only in process_command */
#define Matched(string) { if(!string_prefix((string), command)) goto bad; }

void
find_path( dbref loc, const char *pathname, char buf[BUFFER_LEN] ) {
    PropPtr propadr, pptr;

#ifdef DISKBASE
    fetchprops(loc);
#endif
    buf[0] = '\0';
    propadr = first_prop(loc, "@u/d/", &pptr, buf);

    while (propadr > 0) {
	if( exit_prefix(buf, pathname) )
	    return;
	propadr = next_prop(pptr, propadr, buf);
    }
    buf[0] = '\0';
}

int force_level = 0;

void 
process_command(int descr, dbref player, char *command)
{
    char   *arg1;
    char   *arg2;
    char   *full_command, *commandline=command, *commandstuff;
    char   *p;                  /* utility */
    char    pbuf[BUFFER_LEN];
    char    xbuf[BUFFER_LEN];
    char    ybuf[BUFFER_LEN];
    const char *path;
    struct frame *tmpfr;

    if (command == 0)
	abort();

    /* robustify player */
    if (player < 0 || player >= db_top ||
	    (Typeof(player) != TYPE_PLAYER && Typeof(player) != TYPE_THING)) {
	log_status("process_command: bad player %d\n", player);
	return;
    }

    if ((tp_log_commands || (tp_log_guests && Guest(OWNER(player)))) &&
	    (!(FLAGS(player)&(INTERACTIVE | READMODE)))) {
	log_command("%s%s%s%s(%d) in %s(%d):%s %s\n",
		    Mage(OWNER(player)) ? "WIZ: " : "",
		    (Typeof(player) != TYPE_PLAYER) ? NAME(player) : "",
		    (Typeof(player) != TYPE_PLAYER) ? " by " : "",
		    NAME(OWNER(player)), (int) player,
		    NAME(DBFETCH(player)->location),
		    (int) DBFETCH(player)->location,
		    (FLAGS(player) & INTERACTIVE) ?
		    " [interactive]" : " ", command);
    }

    if (FLAGS(player) & INTERACTIVE) {
	interactive(descr, player, command);
	return;
    }
    /* eat leading whitespace */
    while (*command && isspace(*command))
	command++;

    commandstuff = command;
    /* check for single-character commands */
    if (*command == SAY_TOKEN || *command == '\'') {
	sprintf(pbuf, "say %s", command + 1);
	command = &pbuf[0];
    } else if (*command == POSE_TOKEN || *command == ';') {
	sprintf(pbuf, "pose %s", command + 1);
	command = &pbuf[0];
    } else if ((*command == '|' || (*commandstuff++ == '\\' && *commandstuff == '\\') ) && can_move(descr, player, "spoof", 0)) {
      if(*command = '\\')
         sprintf(pbuf, "spoof %s", command + 2);
      else
         sprintf(pbuf, "spoof %s", command + 1);
      command = &pbuf[0];
    }

    /* if player is a wizard, and uses overide token to start line...*/
    /* ... then do NOT run actions, but run the command they specify. */
    if (!strcmp(command, WHO_COMMAND)) {
       char xxbuf[BUFFER_LEN];

       strcpy(xxbuf, "@");
       strcat(xxbuf, WHO_COMMAND);
       strcat(xxbuf, " ");
       strcat(xxbuf, command + sizeof(WHO_COMMAND) - 1);
       if (can_move(descr, player, xxbuf, 5)) {
          do_move(descr, player, xxbuf, 5);
       } else {
          pdump_who_users(descr, command + sizeof(WHO_COMMAND) - 1);
       }
       return;
    }

    if (!( *command == OVERIDE_TOKEN && TMage(player) )) {
	if( can_move(descr, player, command, 0) ) {
	    do_move(descr, player, command, 0); /* command is exact match for exit */
	    *match_args = 0;
	    *match_cmdname = 0; 
	    return;
	}
    }

	if (*command == OVERIDE_TOKEN && TMage(player))
	    command++;

	full_command = strcpy(xbuf, command);
	for (; *full_command && !isspace(*full_command); full_command++);
	if (*full_command)
	    full_command++;

	/* find arg1 -- move over command word */
	command = strcpy(ybuf, command);
	for (arg1 = command; *arg1 && !isspace(*arg1); arg1++);
	/* truncate command */
	if (*arg1)
	    *arg1++ = '\0';

	/* remember command for programs */
	strcpy(match_cmdname, command);

	/* move over spaces */
	while (*arg1 && isspace(*arg1))
	    arg1++;

	/* find end of arg1, start of arg2 */
	for (arg2 = arg1; *arg2 && *arg2 != ARG_DELIMITER; arg2++);

	/* truncate arg1 */
	for (p = arg2 - 1; p >= arg1 && isspace(*p); p--)
	    *p = '\0';

	/* go past delimiter if present */
	if (*arg2)
	    *arg2++ = '\0';
	while (*arg2 && isspace(*arg2))
	    arg2++;

	strcpy(match_cmdname, command);
	strcpy(match_args, full_command);

	switch (command[0]) {
	    case '@':
		switch (command[1]) {
		    case 'a':
		    case 'A':
			/* @action, @attach */
			switch (command[2]) {
			    case 'c':
			    case 'C':
				Matched("@action");
				do_action(descr, player, arg1, arg2);
				break;
			    case 'r':
			    case 'R':
				if (string_compare(command, "@armageddon"))
				    goto bad;
				do_armageddon(player, full_command);
				break;
			    case 't':
			    case 'T':
				Matched("@attach");
				do_attach(descr, player, arg1, arg2);
				break;
                      case 'n':
                      case 'N':
                        Matched("@ansidescribe");
                        do_ansidescribe(descr, player, arg1, arg2);
                        break;
			    default:
				goto bad;
			}
			break;
		    case 'b':
		    case 'B':
			Matched("@boot");
			do_boot(player, arg1);
			break;
		    case 'c':
		    case 'C':
			/* chown, contents, create */
			switch (command[2]) {
			    case 'h':
			    case 'H':
				switch (command[3]) {
				    case 'l':
				    case 'L':
					Matched("@chlock");
					do_chlock(descr, player, arg1, arg2);
					break;
				    case 'o':
				    case 'O':
					Matched("@chown");
					do_chown(descr, player, arg1, arg2);
					break;
				    default:
					goto bad;
				}
				break;
			    case 'o':
			    case 'O':
				switch (command[4]) {
				    case 'l':
				    case 'L':
					Matched("@conlock");
					do_conlock(descr, player, arg1, arg2);
					break;
				    case 't':
				    case 'T':
					Matched("@contents");
					do_contents(descr, player, arg1, arg2);
					break;
				    default:
					goto bad;
				}
				break;
			    case 'r':
			    case 'R':
				if (string_compare(command, "@credits")) {
				    Matched("@create");
				    do_create(player, arg1, arg2);
				} else {
				    do_credits(player);
				}
				break;
			    default:
				goto bad;
			}
			break;
		    case 'd':
		    case 'D':
			/* describe, dequeue, dig, or dump */
			switch (command[2]) {
			    case 'b':
			    case 'B':
				Matched("@dbginfo");
				do_serverdebug(descr, player, arg1, arg2);
				break;
			    case 'e':
			    case 'E':
#ifdef DELTADUMPS
				if(command[3] == 'l' || command[3] == 'L') {
				    Matched("@delta");
				    do_delta(player);
				} else
#endif
				{
                          if( (command[3] == 's' || command[3] == 'S') &&
                              (command[4] == 't' || command[4] == 'T') ) {
                            Matched("@destroy");
 				    do_recycle(descr, player, arg1);
                          } else {
				    Matched("@describe");
				    do_describe(descr, player, arg1, arg2);
                          }
				}
				break;
			    case 'i':
			    case 'I':
				Matched("@dig");
				do_dig(descr, player, arg1, arg2);
				break;
#ifdef DELTADUMPS
			    case 'l':
			    case 'L':
				Matched("@dlt");
				do_delta(player);
				break;
#endif
			    case 'o':
			    case 'O':
				Matched("@doing");
				if (!tp_who_doing) goto bad;
				do_doing(descr, player, arg1, arg2);
				break;
			    case 'r':
			    case 'R':
				Matched("@drop");
				do_drop_message(descr, player, arg1, arg2);
				break;
			    case 'u':
			    case 'U':
				Matched("@dump");
				do_dump(player, full_command);
				break;
			    default:
				goto bad;
			}
			break;
		    case 'e':
		    case 'E':
			switch (command[2]) {
			    case 'd':
			    case 'D':
				Matched("@edit");
				do_edit(descr, player, arg1);
				break;
			    case 'n':
			    case 'N':
				Matched("@entrances");
				do_entrances(descr, player, arg1, arg2);
				break;
			    case 'x':
			    case 'X':
				Matched("@examine");
				sane_dump_object(player, arg1);
				break;
			    default:
				goto bad;
			}
			break;
		    case 'f':
		    case 'F':
			/* fail, find, force, or frob */
			switch (command[2]) {
			    case 'a':
			    case 'A':
				Matched("@fail");
				do_fail(descr, player, arg1, arg2);
				break;
			    case 'i':
			    case 'I':
				if(command[3] == 'x' || command[3] == 'X') {
				    Matched("@fixwizbits");
				    do_fixw(player, full_command);
				} else {
				    Matched("@find");
				    do_find(player, arg1, arg2);
				}
				break;
			    case 'l':
			    case 'L':
				Matched("@flock");
				do_flock(descr, player, arg1, arg2);
				break;
			    case 'o':
			    case 'O':
				Matched("@force");
				do_force(descr, player, arg1, arg2);
				break;
			    case 'r':
			    case 'R':
				if (string_compare(command, "@frob"))
				    goto bad;
				do_frob(descr, player, arg1, arg2);
				break;
			    default:
				goto bad;
			}
			break;
		    case 'h':
		    case 'H':
                  switch (command[2]) {
                     case 'T':
                     case 't':
                        Matched("@htmldescribe");
                        do_htmldescribe(descr, player, arg1, arg2);
                        break;
                     default:
 			      Matched("@hopper");
			      do_hopper(player, arg1);
                        break;
                  }
			break;
		    case 'i':
		    case 'I':
                  switch(command[2]) {
                     case 'h':
                     case 'H':
   	   		      Matched("@ihtmldescribe");
			      do_ihtmldescribe(descr, player, arg1, arg2);
			      break;
                     case 'a':
                     case 'A':
                        Matched("@iansidescribe");
                        do_iansidescribe(descr, player, arg1, arg2);
                        break;
                     default:
   			      Matched("@idescribe");
			      do_idescribe(descr, player, arg1, arg2);
			      break;
                  }
                  break;
		    case 'k':
		    case 'K':
			Matched("@kill");
			do_dequeue(descr, player, arg1);
			break;
		    case 'l':
		    case 'L':
			/* lock or link */
			switch (command[2]) {
			    case 'i':
			    case 'I':
				switch (command[3]) {
				    case 'n':
				    case 'N':
					Matched("@link");
					do_link(descr, player, arg1, arg2);
					break;
				    case 's':
				    case 'S':
					Matched("@list");
					match_and_list(descr, player, arg1, arg2, 1);
					break;
				    default:
					goto bad;
				}
				break;
			    case 'o':
			    case 'O':
				Matched("@lock");
				do_lock(descr, player, arg1, arg2);
				break;
			    default:
				goto bad;
			}
			break;
		    case 'm':
		    case 'M':
			switch (command[2]) {
			    case 'e':
			    case 'E':
				Matched("@memory");
				do_memory(player);
				break;
			    case 'c':
			    case 'C':
				switch (command[4]) {
				   case '2':
				     Matched("@mcp2muf");
				     match_and_list(descr, player, arg1, arg2, 0);
				     break;
				   default:
				     Matched("@mcp");
				     do_prog(descr, player, arg1, 1);
				     break;
				}
                     default:
                        goto bad;
			}
			break;
		    case 'n':
		    case 'N':
			/* @name or @newpassword */
			switch (command[2]) {
			    case 'a':
			    case 'A':
				Matched("@name");
				do_name(descr, player, arg1, arg2);
				break;
			    case 'e':
			    case 'E':
				if (string_compare(command, "@newpassword"))
				    goto bad;
				do_newpassword(player, arg1, arg2);
				break;
			    default:
				goto bad;
			}
			break;
		    case 'o':
		    case 'O':
			switch (command[2]) {
			    case 'd':
			    case 'D':
				Matched("@odrop");
				do_odrop(descr, player, arg1, arg2);
				break;
			    case 'e':
			    case 'E':
				Matched("@oecho");
				do_oecho(descr, player, arg1, arg2);
				break;
			    case 'f':
			    case 'F':
				Matched("@ofail");
				do_ofail(descr, player, arg1, arg2);
				break;
			    case 'p':
			    case 'P':
				Matched("@open");
				do_open(descr, player, arg1, arg2);
				break;
			    case 's':
			    case 'S':
				Matched("@osuccess");
				do_osuccess(descr, player, arg1, arg2);
				break;
			    case 'w':
			    case 'W':
				Matched("@owned");
				do_owned(player, arg1, arg2);
				break;
			    default:
				goto bad;
			}
			break;
		    case 'p':
		    case 'P':
			switch (command[2]) {
			    case 'a':
			    case 'A':
				Matched("@password");
				do_password(player, arg1, arg2);
				break;
			    case 'c':
			    case 'C':
				Matched("@pcreate");
				do_pcreate(player, arg1, arg2);
				break;
			    case 'e':
			    case 'E':
				Matched("@pecho");
				do_pecho(descr, player, arg1, arg2);
				break;
			    case 'r':
			    case 'R':
				if (string_prefix("@program", command)) {
				    do_prog(descr, player, arg1, 0);
				    break;
				} else if (string_prefix("@proginfo", command)) {
				    do_proginfo(player, arg1);
				    break;
				} else {
				    Matched("@propset");
				    do_propset(descr, player, arg1, arg2);
				    break;
				}
			    case 's':
			    case 'S':
				Matched("@ps");
				list_events(player);
				break;
			    case 'u':
			    case 'U':
				Matched("@purge");
				do_purge(descr, player, arg1, arg2);
				break;
			    default:
				goto bad;
			}
			break;
		    case 'r':
		    case 'R':
			switch (command[3]) {
			    case 'c':
			    case 'C':
				Matched("@recycle");
				do_recycle(descr, player, arg1);
				break;
			    case 's':
			    case 'S':
				if (!string_compare(command, "@restart")) {
				    do_restart(player, full_command);
				} else if (!string_compare(command, "@restrict")) {
				    do_restrict(player, arg1);
				} else {
				    goto bad;
				}
				break;
			    default:
				goto bad;
			}
			break;
		    case 's':
		    case 'S':
			/* set, shutdown, success */
			switch (command[2]) {
			    case 'a':
			    case 'A':
				if (!string_compare(command, "@sanity")) {
				    sanity(player);
				} else if (!string_compare(command, "@sanchange")) {
				    sanechange(player, full_command);
				} else if (!string_compare(command, "@sanfix")) {
				    sanfix(player);
				} else {
				    goto bad;
				}
				break;
			    case 'e':
			    case 'E':
				Matched("@set");
				do_set(descr, player, arg1, arg2);
				break;
			    case 'h':
			    case 'H':
				if (!string_compare(command, "@shout")) {
				    do_wall(player, full_command);
				    break;
				}
				if (string_compare(command, "@shutdown"))
				    goto bad;
				do_shutdown(player, arg1);
				break;
			    case 't':
			    case 'T':
				Matched("@stats");
				do_stats(player, arg1);
				break;
			    case 'u':
			    case 'U':
				Matched("@success");
				do_success(descr, player, arg1, arg2);
				break;
			    case 'w':
			    case 'W':
				Matched("@sweep");
				do_sweep(descr, player, arg1);
				break;
			    default:
				goto bad;
			}
			break;
		    case 't':
		    case 'T':
			switch (command[2]) {
			    case 'e':
			    case 'E':
				Matched("@teleport");
				do_teleport(descr, player, arg1, arg2);
				break;
                      case 'o':
                      case 'O':
				if (string_compare(command, "@toad"))
				    goto bad;
				do_frob(descr, player, arg1, arg2);
				break;
			    case 'r':
			    case 'R':
				Matched("@trace");
				do_trace(descr, player, arg1, atoi(arg2));
				break;
			    case 'u':
			    case 'U':
				Matched("@tune");
				do_tune(player, arg1, arg2);
				break;
			    default:
				goto bad;
			}
			break;
		    case 'u':
		    case 'U':
			switch (command[2]) {
			    case 'N':
			    case 'n':
				if (string_prefix(command, "@unlink")) {
				    do_unlink(descr, player, arg1);
				} else if (string_prefix(command, "@unlock")) {
				    do_unlock(descr, player, arg1);
				} else if (string_prefix(command, "@uncompile")) {
				    do_uncompile(player);
				} else {
				    goto bad;
				}
				break;

#ifndef NO_USAGE_COMMAND
			    case 'S':
			    case 's':
				Matched("@usage");
				do_usage(player);
				break;
#endif

			    default:
				goto bad;
				break;
			}
			break;
		    case 'v':
		    case 'V':
			Matched("@version");
			anotify_nolisten2(player, CRIMSON "ProtoMUCK " PROTOBASE PURPLE " (" RED VERSION WHITE " -- " AQUA NEONVER PURPLE ")" );
			break;
		    case 'w':
		    case 'W':
			if (string_compare(command, "@wall"))
			    goto bad;
			do_wall(player, full_command);
			break;
		    default:
			goto bad;
		}
		break;
          case '&':
            do_mush_set(descr, player, arg1, arg2, command);
            break;
	    case 'd':
	    case 'D':
		switch (command[1]) {
		    case 'b':
		    case 'B':
			Matched("dboot");
			do_dboot(player, arg1);
			break;
		    case 'i':
		    case 'I':
			Matched("dinfo");
			do_dinfo(player, arg1);
			break;
		    case 'r':
		    case 'R':
			Matched("drop");
			do_drop(descr, player, arg1, arg2);
			break;
		    case 'w':
		    case 'W':
			Matched("dwall");
			do_dwall(player, arg1, arg2);
			break;
		    default:
			goto bad;
		}
		break;
	    case 'e':
	    case 'E':
		switch (command[1]) {
		    case 'm':
		    case 'M':
			Matched("emote");
			do_pose(descr, player, full_command);
			break;
		    case 'x':
		    case 'X':
		    case '\0':
			Matched("examine");
			do_examine(descr, player, arg1, arg2);
			break;
		    default:
			goto bad;
		}
		break;
	    case 'g':
	    case 'G':
		/* get, give, go, or gripe */
		switch (command[1]) {
		    case 'e':
		    case 'E':
			Matched("get");
			do_get(descr, player, arg1, arg2);
			break;
		    case 'i':
		    case 'I':
			Matched("give");
			do_give(descr, player, arg1, atoi(arg2));
			break;
		    case 'o':
		    case 'O':
			Matched("goto");
			do_move(descr, player, arg1, 0);
			break;
		    case 'r':
		    case 'R':
			if(command[2]=='i' || command[2]=='I') {
				if (string_compare(command, "gripe"))
				    goto bad;
				do_gripe(player, full_command);
				break;
			}
		    default:
			goto bad;
		}
		break;
	    case 'h':
	    case 'H':
		switch (command[1]) {
		    case 'a':
		    case 'A':
			Matched("hand");
			do_drop(descr, player, arg1, arg2);
			break;
		    case 'e':
		    case 'E':
			Matched("help");
			do_help(player, arg1, arg2);
			break;
		    default:
			goto bad;
		}
		break;
	    case 'i':
	    case 'I':
		if (string_compare(command, "info")) {
		    Matched("inventory");
		    do_inventory(player);
		} else {
		    do_info(player, arg1, arg2);
		}
		break;
	    case 'l':
	    case 'L':
		if (string_prefix("look", command)) {
		    do_look_at(descr, player, arg1, arg2);
		    break;
		} else {
		    Matched("leave");
		    do_leave(descr, player);
		    break;
		}
	    case 'm':
	    case 'M':
		if (string_prefix(command, "move")) {
		    do_move(descr, player, arg1, 0);
		    break;
		} else if (!string_compare(command, "motd")) {
		    do_motd(player, full_command);
		    break;
		} else if (!string_compare(command, "mpi")) {
		    do_mpihelp(player, arg1, arg2);
		    break;
		} else {
		    if (string_compare(command, "man"))
			goto bad;
		    do_man(player, arg1, arg2);
		}
		break;
	    case 'n':
	    case 'N':
		/* news */
		Matched("news");
		do_news(player, arg1, arg2);
		break;
	    case 'p':
	    case 'P':
		switch (command[1]) {
		    case 'a':
		    case 'A':
		    case '\0':
			Matched("page");
			do_page(descr, player, arg1, arg2);
			break;
		    case 'o':
		    case 'O':
			Matched("pose");
			do_pose(descr, player, full_command);
			break;
		    case 'u':
		    case 'U':
			Matched("put");
			do_drop(descr, player, arg1, arg2);
			break;
		    default:
			goto bad;
		}
		break;
	    case 'r':
	    case 'R':
		switch (command[1]) {
		    case 'e':
		    case 'E':
			if(command[2] == 'q' || command[2] == 'Q'){
			    Matched("request");
			    request(player, NULL, commandline);
			} else {
			    Matched("read");    /* undocumented alias for look */
			    do_look_at(descr, player, arg1, arg2);
			}
			break;
		    default:
			goto bad;
		}
		break;
	    case 's':
	    case 'S':
		/* say, "score" */
		switch (command[1]) {
		    case 'a':
		    case 'A':
			Matched("say");
			do_say(descr, player, full_command);
			break;
		    case 'c':
		    case 'C':
		    case '\0':
			if( command[1] &&
			    (command[2] == 'a' || command[2] == 'A' )) {
			    Matched("scan");
			    do_sweep(descr, player, arg1);
			} else {
			    Matched("score");
			    do_score(player, 1);
			}
			break;
		    default:
			goto bad;
		}
		break;
	    case 't':
	    case 'T':
		switch (command[1]) {
		    case 'a':
		    case 'A':
			Matched("take");
			do_get(descr, player, arg1, arg2);
			break;
		    case 'h':
		    case 'H':
			Matched("throw");
			do_drop(descr, player, arg1, arg2);
			break;
		    default:
			goto bad;
		}
		break;
	    case 'w':
	    case 'W':
		switch (command[1]){
		    case 'c':
		    case 'C':
			Matched("wc");
			do_wizchat(player, full_command);
			break;
		    case 'i':
		    case 'I':
			Matched("wizchat");
			do_wizchat(player, arg1);
			break;
		    case 'h':
		    case 'H':
		    case '\0':
			Matched("whisper");
			do_whisper(descr, player, arg1, arg2);
			break;
		    default:
			goto bad;
		}
		break;
	    default:
	bad:
 		if ( Typeof(tp_huh_command) == TYPE_PROGRAM && !(player == -1)) {
			tmpfr = interp(descr, player, DBFETCH(player)->location, tp_huh_command, (dbref)-4, FOREGROUND, STD_REGUID);
			if (!tmpfr)
				goto bad2;
			(void) interp_loop(player, tp_huh_command, tmpfr, 0);
            } else {
   		   bad2:
                  anotify_fmt(player, CINFO "%s",tp_huh_mesg);
			if (tp_log_failed_commands && !controls(player, DBFETCH(player)->location)) {
		    		log_status("HUH from %s(%d) in %s(%d)[%s]: %s %s\n", NAME(player), player,
                        NAME(DBFETCH(player)->location),
			      DBFETCH(player)->location,
			      NAME(OWNER(DBFETCH(player)->location)), command,
			      full_command);
			}
		}
		break;
	}

}

#undef Matched







