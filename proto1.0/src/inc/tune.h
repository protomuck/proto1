/* strings */
extern const char *tp_dumpwarn_mesg;
extern const char *tp_deltawarn_mesg;
extern const char *tp_dumpdeltas_mesg;
extern const char *tp_dumping_mesg;
extern const char *tp_dumpdone_mesg;

extern const char *tp_dummy_midi;
extern const char *tp_mailserver;
extern const char *tp_servername;
extern const char *tp_leave_message;
extern const char *tp_huh_mesg;
extern const char *tp_noperm_mesg;
extern const char *tp_noguest_mesg;

extern const char *tp_idleboot_msg;

extern const char *tp_penny;
extern const char *tp_pennies;
extern const char *tp_cpenny;
extern const char *tp_cpennies;

extern const char *tp_muckname;

#ifdef RWHO
extern const char *tp_rwho_passwd;
extern const char *tp_rwho_server;
#endif

extern const char *tp_reg_email;

extern const char *tp_proplist_counter_fmt;
extern const char *tp_proplist_entry_fmt;


/* times */
#ifdef RWHO
extern time_t tp_rwho_interval;
#endif

extern time_t tp_dump_interval;
extern time_t tp_dump_warntime;
extern time_t tp_monolithic_interval;
extern time_t tp_clean_interval;
extern time_t tp_aging_time;
extern time_t tp_maxidle;
extern time_t tp_cron_interval;


/* integers */

extern int tp_textport;
extern int tp_wwwport;
extern int tp_puebloport;

extern int tp_max_object_endowment;
extern int tp_object_cost;
extern int tp_exit_cost;
extern int tp_link_cost;
extern int tp_room_cost;
extern int tp_lookup_cost;
extern int tp_max_pennies;
extern int tp_penny_rate;
extern int tp_start_pennies;

extern int tp_command_burst_size;
extern int tp_commands_per_time;
extern int tp_command_time_msec;

extern int tp_max_delta_objs;
extern int tp_max_loaded_objs;
extern int tp_max_process_limit;
extern int tp_max_plyr_processes;
extern int tp_max_instr_count;
extern int tp_instr_slice;
extern int tp_mpi_max_commands;
extern int tp_pause_min;
extern int tp_free_frames_pool;
extern int tp_max_output;
extern int tp_rand_screens;

extern int tp_listen_mlev;
extern int tp_playermax_limit;

extern int tp_num_backups;

/* dbrefs */
extern dbref tp_quit_prog;
extern dbref tp_huh_command;
extern dbref tp_login_huh_command;
extern dbref tp_login_who_prog;
extern dbref tp_player_start;
extern dbref tp_reg_wiz;
extern dbref tp_player_prototype;
extern dbref tp_cron_prog;
#ifdef HTTPD
extern dbref tp_www_root;
extern dbref tp_www_surfer;
#endif


/* booleans */
extern int tp_hostnames;
extern int tp_log_commands;
extern int tp_log_interactive;
extern int tp_log_connects;
extern int tp_log_mud_commands;
extern int tp_log_failed_commands;
extern int tp_log_programs;
extern int tp_log_guests;
extern int tp_log_files;
extern int tp_log_sockets;
extern int tp_dbdump_warning;
extern int tp_deltadump_warning;
extern int tp_periodic_program_purge;

#ifdef RWHO
extern int tp_rwho;
#endif

extern int tp_secure_who;
extern int tp_who_doing;
extern int tp_realms_control;
extern int tp_listeners;
extern int tp_listeners_obj;
extern int tp_listeners_env;
extern int tp_zombies;
extern int tp_wiz_vehicles;
extern int tp_wiz_name;
extern int tp_recycle_frobs;
extern int tp_m1_name_notify;
extern int tp_registration;
extern int tp_online_registration;
extern int tp_fast_registration;
extern int tp_teleport_to_player;
extern int tp_secure_teleport;
extern int tp_exit_darking;
extern int tp_thing_darking;
extern int tp_dark_sleepers;
extern int tp_who_hides_dark;
extern int tp_compatible_priorities;
extern int tp_do_mpi_parsing;
extern int tp_look_propqueues;
extern int tp_lock_envcheck;
extern int tp_diskbase_propvals;
extern int tp_idleboot;
extern int tp_playermax;
extern int tp_db_readonly;
extern int tp_process_timer_limit;
extern int tp_pcreate_copy_props;
extern int tp_enable_home;
extern int tp_quiet_moves;
extern int tp_quiet_connects;
extern int tp_expanded_debug;
extern int tp_proplist_int_counter;

extern int tune_count_parms();
extern void tune_load_parms_from_file(FILE *f, dbref player, int cnt);
extern void tune_save_parms_to_file(FILE *f);

extern int tp_building;
extern int tp_all_can_build_rooms;
extern int tp_restricted_building;





