
#include "copyright.h"
#include "db.h"
#include "defaults.h"

/* structures */

struct text_block {
    int     nchars;
    struct text_block *nxt;
    char   *start;
    char   *buf;
};

struct text_queue {
    int     lines;
    struct text_block *head;
    struct text_block **tail;
};

struct descriptor_data {
    int     descriptor;
    int     connected;
    int     booted;
    int	    fails;
    dbref   player;
    char   *output_prefix;
    char   *output_suffix;
    int     output_size;
    struct text_queue output;
    struct text_queue input;
    char   *raw_input;
    char   *raw_input_at;
    time_t  last_time;
    time_t  connected_at;
    int hostaddr;
    int	port;
    const char *hostname;
    const char *username;
    int     quota;
    int	    commands;
    int     linelen;
    int     type;
    int     http_login;
    char    *identify;
    char    *lastmidi;
#ifdef HTTPDELAY
    const char *httpdata;
#endif
    struct descriptor_data *next;
    struct descriptor_data **prev;
};

#define CT_MUCK		0
#define CT_HTML		1
#define CT_PUEBLO	      2

/* these symbols must be defined by the interface */

#ifdef HTTPD
extern void httpd(struct descriptor_data *d, const char *name, const char *http);
extern void httpd_unknown(struct descriptor_data *d);
#endif

extern char *html_escape (const char *str);
extern char *parse_ansi( char *buf, const char *from );
extern char *unparse_ansi( char *buf, const char *from );
extern char *tct( const char *in, char out[BUFFER_LEN]);
extern int notify(dbref player, const char *msg);
extern int notify_nolisten(dbref player, const char *msg, int ispriv);
extern void notify_descriptor(int c, const char *msg);
extern int anotify(dbref player, const char *msg);
extern int notify_html(dbref player, const char *msg);
extern int queue_string(struct descriptor_data *d, const char *s);
extern int notify_nolisten(dbref player, const char *msg, int isprivate);
extern int notify_nolisten2(dbref player, const char *msg);
extern int notify_html_nolisten(dbref player, const char *msg, int isprivate);
extern int anotify_nolisten(dbref player, const char *msg, int isprivate);
extern int notify_from_echo(dbref from, dbref player, const char *msg, int isprivate);
extern int notify_html_from_echo(dbref from, dbref player, const char *msg, int isprivate);
extern int anotify_from_echo(dbref from, dbref player, const char *msg, int isprivate);
extern int notify_from(dbref from, dbref player, const char *msg);
extern int notify_html_from(dbref from, dbref player, const char *msg);
extern int anotify_from(dbref from, dbref player, const char *msg);
extern void wall_and_flush(const char *msg);
extern void flush_user_output(dbref player);
extern void wall_all(const char *msg);
extern void wall_logwizards(const char *msg);
extern void wall_arches(const char *msg);
extern void wall_wizards(const char *msg);
extern void ansi_wall_wizards(const char *msg);
extern void show_wizards(dbref player);
extern int shutdown_flag; /* if non-zero, interface should shut down */
extern int restart_flag; /* if non-zero, should restart after shut down */
extern void emergency_shutdown(void);
extern int boot_off(dbref player);
extern void boot_player_off(dbref player);
extern int online(dbref player);
extern int pcount();
extern int pidle(int c);
extern int pdbref(int c);
extern int pontime(int c);
extern char *phost(int c);
extern char *puser(int c);
extern char *pipnum(int c);
extern char *pport(int c);
extern int pfirstdescr(dbref who);
extern void pboot(int c);
extern void pnotify(int c, char *outstr);
extern int pdescr(int c);
extern int pdescrcon(int c);
extern int pnextdescr(int c);
extern int pfirstconn(dbref who);
extern int pset_user(int c, dbref who);
extern int plogin_user(int c, dbref who);
extern int pset_user2(int c, dbref who);
extern dbref partial_pmatch(const char *name);
extern void do_armageddon( dbref, const char * );
extern void do_dinfo( dbref, const char * );
extern void do_dboot(dbref player, const char *name);
extern void do_dwall( dbref, const char *, const char * );
extern int request( dbref, struct descriptor_data *, const char *msg );
extern int dbref_first_descr(dbref c);
extern int pdescrflush(int c);
extern int pdescrp(int c);
extern int pdescrtype(int c);
extern void pdescr_welcome_user(int c);
extern void pdescr_logout(int c);
extern void pdump_who_users(int c, char *user);

/* the following symbols are provided by game.c */

extern void process_command(int descr, dbref player, char *command);

extern dbref create_player(const char *name, const char *password);
extern dbref connect_player(const char *name, const char *password);
extern void do_look_around(int descr, dbref player);

extern int init_game(const char *infile, const char *outfile);
extern void dump_database(void);
extern void panic(const char *);

/* Ansi Colors */
#define ANSINORMAL	"\033[0m"
#define ANSIFLASH	      "\033[5m"
#define ANSIINVERT	"\033[7m"
#define ANSIUNDERLINE   "\033[4m"
#define ANSIBOLD        "\033[1m"

#define ANSIBLACK	      "\033[0;30m"
#define ANSICRIMSON	"\033[0;31m"
#define ANSIFOREST	"\033[0;32m"
#define ANSIBROWN	      "\033[0;33m"
#define ANSINAVY	      "\033[0;34m"
#define ANSIVIOLET	"\033[0;35m"
#define ANSIAQUA	      "\033[0;36m"
#define ANSIGRAY	      "\033[0;37m"

#define ANSIGLOOM	      "\033[1;30m"
#define ANSIRED		"\033[1;31m"
#define ANSIGREEN	      "\033[1;32m"
#define ANSIYELLOW	"\033[1;33m"
#define ANSIBLUE	      "\033[1;34m"
#define ANSIPURPLE	"\033[1;35m"
#define ANSICYAN	      "\033[1;36m"
#define ANSIWHITE	      "\033[1;37m"

#define ANSICBLACK      "\033[30m"
#define ANSICRED		"\033[31m"
#define ANSICGREEN	"\033[32m"
#define ANSICYELLOW	"\033[33m"
#define ANSICBLUE	      "\033[34m"
#define ANSICPURPLE	"\033[35m"
#define ANSICCYAN	      "\033[36m"
#define ANSICWHITE	"\033[37m"

#define ANSIBBLACK	"\033[40m"
#define ANSIBRED	      "\033[41m"
#define ANSIBGREEN	"\033[42m"
#define ANSIBBROWN	"\033[43m"
#define ANSIBBLUE	      "\033[44m"
#define ANSIBPURPLE	"\033[45m"
#define ANSIBCYAN	      "\033[46m"
#define ANSIBGRAY	      "\033[47m"


/* Colors */
#define NORMAL	"^NORMAL^"
#define FLASH	"^FLASH^"
#define INVERT	"^INVERT^"
#define UNDERLINE "^UNDERLINE^"
#define BOLD      "^BOLD^"

#define BLACK	"^BLACK^"
#define CRIMSON	"^CRIMSON^"
#define FOREST	"^FOREST^"
#define BROWN	"^BROWN^"
#define NAVY	"^NAVY^"
#define VIOLET	"^VIOLET^"
#define AQUA	"^AQUA^"
#define GRAY	"^GRAY^"

#define GLOOM	"^GLOOM^"
#define RED	      "^RED^"
#define GREEN	"^GREEN^"
#define YELLOW	"^YELLOW^"
#define BLUE	"^BLUE^"
#define PURPLE	"^PURPLE^"
#define CYAN	"^CYAN^"
#define WHITE	"^WHITE^"

#define CBLACK	"^CBLACK^"
#define CRED	"^CRED^"
#define CGREEN	"^CGREEN^"
#define CYELLOW	"^CYELLOW^"
#define CBLUE	"^CBLUE^"
#define CPURPLE	"^CPURPLE^"
#define CCYAN	"^CCYAN^"
#define CWHITE	"^CWHITE^"

#define BBLACK	"^BBLACK^"
#define BRED	"^BRED^"
#define BGREEN	"^BGREEN^"
#define BBROWN	"^BBROWN^"
#define BBLUE	"^BBLUE^"
#define BPURPLE	"^BPURPLE^"
#define BCYAN	"^BCYAN^"
#define BGRAY	"^BGRAY^"

/* These are defined in defaults.h */
#define CFAIL "^" CCFAIL "^"
#define CSUCC "^" CCSUCC "^"
#define CINFO "^" CCINFO "^"
#define CNOTE "^" CCNOTE "^"
#define CMOVE "^" CCMOVE "^"

/* ANSI attributes and color codes for FB6 style ansi routines */

#define ANSI_RESET	"\033[0m"

#define ANSI_BOLD	      "\033[1m"
#define ANSI_DIM      	"\033[2m"
#define ANSI_UNDERLINE	"\033[4m"
#define ANSI_FLASH	"\033[5m"
#define ANSI_REVERSE	"\033[7m"

#define ANSI_FG_BLACK	"\033[30m"
#define ANSI_FG_RED	"\033[31m"
#define ANSI_FG_YELLOW	"\033[33m"
#define ANSI_FG_GREEN	"\033[32m"
#define ANSI_FG_CYAN	"\033[36m"
#define ANSI_FG_BLUE	"\033[34m"
#define ANSI_FG_MAGENTA	"\033[35m"
#define ANSI_FG_WHITE	"\033[37m"

#define ANSI_BG_BLACK	"\033[40m"
#define ANSI_BG_RED	"\033[41m"
#define ANSI_BG_YELLOW	"\033[43m"
#define ANSI_BG_GREEN	"\033[42m"
#define ANSI_BG_CYAN	"\033[46m"
#define ANSI_BG_BLUE	"\033[44m"
#define ANSI_BG_MAGENTA	"\033[45m"
#define ANSI_BG_WHITE	"\033[47m"

