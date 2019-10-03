/*    motion.c
 *
 *    Detect changes in a video stream.
 *    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
#include "motion.h"
#include "logger.h"
#include "util.h"
#include "motion_loop.h"
#include "dbse.h"
#include "webu.h"
#include "video_common.h"
#include "movie.h"
#include "netcam.h"
#include "draw.h"

/**
 * tls_key_threadnr
 *
 *   TLS key for storing thread number in thread-local storage.
 */
pthread_key_t tls_key_threadnr;

/**
 * global_lock
 *
 *   Protects any global variables (like 'threads_running') during updates,
 *   to prevent problems with multiple threads updating at the same time.
 */
//pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_lock;

/**
 * cam_list
 *
 *   List of context structures, one for each main Motion thread.
 */
struct ctx_cam **cam_list = NULL;

/**
 * threads_running
 *
 *   Keeps track of number of Motion threads currently running. Also used
 *   by 'main' to know when all threads have exited.
 */
volatile int threads_running = 0;

/* Set this when we want main to end or restart */
volatile unsigned int finish = 0;

/* Log file used instead of stderr and syslog */
FILE *ptr_logfile = NULL;

/**
 * restart
 *
 *   Differentiates between a quit and a restart. When all threads have
 *   finished running, 'main' checks if 'restart' is true and if so starts
 *   up again (instead of just quitting).
 */
unsigned int restart = 0;




/**
 * context_init
 *
 *   Initializes a context struct with the default values for all the
 *   variables.
 *
 * Parameters:
 *
 *   cam - the context struct to destroy
 *
 * Returns: nothing
 */
static void context_init(struct ctx_cam *cam)
{
   /*
    * We first clear the entire structure to zero, then fill in any
    * values which have non-zero default values.  Note that this
    * assumes that a NULL address pointer has a value of binary 0
    * (this is also assumed at other places within the code, i.e.
    * there are instances of "if (ptr)").  Just for possible future
    * changes to this assumption, any pointers which are intended
    * to be initialised to NULL are listed within a comment.
    */

    memset(cam, 0, sizeof(struct ctx_cam));
    cam->noise = 255;
    cam->lastrate = 25;

    cam->pipe = -1;
    cam->mpipe = -1;

    cam->vdev = NULL;    /*Init to NULL to check loading parms vs web updates*/
    cam->netcam = NULL;
    cam->netcam = NULL;
    cam->netcam_high = NULL;

}

/**
 * context_destroy
 *
 *   Destroys a context struct by freeing allocated memory, calling the
 *   appropriate cleanup functions and finally freeing the struct itself.
 *
 * Parameters:
 *
 *   cam - the context struct to destroy
 *
 * Returns: nothing
 */
static void context_destroy(struct ctx_cam *cam)
{
    unsigned int j;

    /* Free memory allocated for config parameters */
    for (j = 0; config_params[j].param_name != NULL; j++) {
        if (config_params[j].copy == copy_string ||
            config_params[j].copy == copy_uri ||
            config_params[j].copy == read_camera_dir) {
            void **val;
            val = (void *)((char *)cam+(int)config_params[j].conf_value);
            if (*val) {
                free(*val);
                *val = NULL;
            }
        }
    }

    free(cam);
}

/**
 * sig_handler
 *
 *  Our SIGNAL-Handler. We need this to handle alarms and external signals.
 */
static void sig_handler(int signo)
{
    int i;

    /*The FALLTHROUGH is a special comment required by compiler.  Do not edit it*/
    switch(signo) {
    case SIGALRM:
        /*
         * Somebody (maybe we ourself) wants us to make a snapshot
         * This feature triggers snapshots on ALL threads that have
         * snapshot_interval different from 0.
         */
        if (cam_list) {
            i = -1;
            while (cam_list[++i]) {
                if (cam_list[i]->conf.snapshot_interval)
                    cam_list[i]->snapshot = 1;

            }
        }
        break;
    case SIGUSR1:
        /* Trigger the end of a event */
        if (cam_list) {
            i = -1;
            while (cam_list[++i]){
                cam_list[i]->event_stop = TRUE;
            }
        }
        break;
    case SIGHUP:
        restart = 1;
        /*
         * Fall through, as the value of 'restart' is the only difference
         * between SIGHUP and the ones below.
         */
         /*FALLTHROUGH*/
    case SIGINT:
        /*FALLTHROUGH*/
    case SIGQUIT:
        /*FALLTHROUGH*/
    case SIGTERM:
        /*
         * Somebody wants us to quit! We should finish the actual
         * movie and end up!
         */

        if (cam_list) {
            i = -1;
            while (cam_list[++i]) {
                cam_list[i]->webcontrol_finish = TRUE;
                cam_list[i]->event_stop = TRUE;
                cam_list[i]->finish = 1;
                /*
                 * Don't restart thread when it ends,
                 * all threads restarts if global restart is set
                 */
                 cam_list[i]->restart = 0;
            }
        }
        /*
         * Set flag we want to quit main check threads loop
         * if restart is set (above) we start up again
         */
        finish = 1;
        break;
    case SIGSEGV:
        exit(0);
    case SIGVTALRM:
        printf("SIGVTALRM went off\n");
        break;
    }
}

/**
 * sigchild_handler
 *
 *   This function is a POSIX compliant replacement of the commonly used
 *   signal(SIGCHLD, SIG_IGN).
 */
static void sigchild_handler(int signo)
{
    (void)signo;
    #ifdef WNOHANG
        while (waitpid(-1, NULL, WNOHANG) > 0) {};
    #endif /* WNOHANG */
    return;
}

/**
 * setup_signals
 *   Attaches handlers to a number of signals that Motion need to catch.
 */
static void setup_signals(void){
    /*
     * Setup signals and do some initialization. 1 in the call to
     * 'motion_startup' means that Motion will become a daemon if so has been
     * requested, and argc and argc are necessary for reading the command
     * line options.
     */
    struct sigaction sig_handler_action;
    struct sigaction sigchild_action;

    #ifdef SA_NOCLDWAIT
        sigchild_action.sa_flags = SA_NOCLDWAIT;
    #else
        sigchild_action.sa_flags = 0;
    #endif
    sigchild_action.sa_handler = sigchild_handler;
    sigemptyset(&sigchild_action.sa_mask);
    #ifdef SA_RESTART
        sig_handler_action.sa_flags = SA_RESTART;
    #else
        sig_handler_action.sa_flags = 0;
    #endif
    sig_handler_action.sa_handler = sig_handler;
    sigemptyset(&sig_handler_action.sa_mask);

    /* Enable automatic zombie reaping */
    sigaction(SIGCHLD, &sigchild_action, NULL);
    sigaction(SIGPIPE, &sigchild_action, NULL);
    sigaction(SIGALRM, &sig_handler_action, NULL);
    sigaction(SIGHUP, &sig_handler_action, NULL);
    sigaction(SIGINT, &sig_handler_action, NULL);
    sigaction(SIGQUIT, &sig_handler_action, NULL);
    sigaction(SIGTERM, &sig_handler_action, NULL);
    sigaction(SIGUSR1, &sig_handler_action, NULL);

    /* use SIGVTALRM as a way to break out of the ioctl, don't restart */
    sig_handler_action.sa_flags = 0;
    sigaction(SIGVTALRM, &sig_handler_action, NULL);
}

/**
 * motion_remove_pid
 *   This function remove the process id file ( pid file ) before motion exit.
 */
static void motion_remove_pid(void)
{
    if ((cam_list[0]->daemon) && (cam_list[0]->conf.pid_file) && (restart == 0)) {
        if (!unlink(cam_list[0]->conf.pid_file))
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Removed process id file (pid file)."));
        else
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error removing pid file"));
    }

    if (ptr_logfile) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Closing logfile (%s)."),
                   cam_list[0]->conf.log_file);
        myfclose(ptr_logfile);
        set_log_mode(LOGMODE_NONE);
        ptr_logfile = NULL;
    }

}

/**
 * become_daemon
 *
 *   Turns Motion into a daemon through forking. The parent process (i.e. the
 *   one initially calling this function) will exit inside this function, while
 *   control will be returned to the child process. Standard input/output are
 *   released properly, and the current directory is set to / in order to not
 *   lock up any file system.
 *
 * Parameters:
 *
 *   cam - current thread's context struct
 *
 * Returns: nothing
 */
static void become_daemon(void)
{
    int i;
    FILE *pidf = NULL;
    struct sigaction sig_ign_action;

    /* Setup sig_ign_action */
    #ifdef SA_RESTART
        sig_ign_action.sa_flags = SA_RESTART;
    #else
        sig_ign_action.sa_flags = 0;
    #endif
    sig_ign_action.sa_handler = SIG_IGN;
    sigemptyset(&sig_ign_action.sa_mask);

    /* fork */
    if (fork()) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion going to daemon mode"));
        exit(0);
    }

    /*
     * Create the pid file if defined, if failed exit
     * If we fail we report it. If we succeed we postpone the log entry till
     * later when we have closed stdout. Otherwise Motion hangs in the terminal waiting
     * for an enter.
     */
    if (cam_list[0]->conf.pid_file) {
        pidf = myfopen(cam_list[0]->conf.pid_file, "w+");

        if (pidf) {
            (void)fprintf(pidf, "%d\n", getpid());
            myfclose(pidf);
        } else {
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                ,_("Exit motion, cannot create process"
                " id file (pid file) %s"), cam_list[0]->conf.pid_file);
            if (ptr_logfile)
                myfclose(ptr_logfile);
            exit(0);
        }
    }

    /*
     * Changing dir to root enables people to unmount a disk
     * without having to stop Motion
     */
    if (chdir("/"))
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Could not change directory"));


    #if (defined(BSD) && !defined(__APPLE__))
        setpgrp(0, getpid());
    #else
        setpgrp();
    #endif


    if ((i = open("/dev/tty", O_RDWR)) >= 0) {
        ioctl(i, TIOCNOTTY, NULL);
        close(i);
    }

    setsid();
    i = open("/dev/null", O_RDONLY);

    if (i != -1) {
        dup2(i, STDIN_FILENO);
        close(i);
    }

    i = open("/dev/null", O_WRONLY);

    if (i != -1) {
        dup2(i, STDOUT_FILENO);
        dup2(i, STDERR_FILENO);
        close(i);
    }

    /* Now it is safe to add the PID creation to the logs */
    if (pidf)
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Created process id file %s. Process ID is %d")
            ,cam_list[0]->conf.pid_file, getpid());

    sigaction(SIGTTOU, &sig_ign_action, NULL);
    sigaction(SIGTTIN, &sig_ign_action, NULL);
    sigaction(SIGTSTP, &sig_ign_action, NULL);
}

static void camlist_create(int argc, char *argv[]){
    /*
     * cam_list is an array of pointers to the context structures cam for each thread.
     * First we reserve room for a pointer to thread 0's context structure
     * and a NULL pointer which indicates that end of the array of pointers to
     * thread context structures.
     */
    cam_list = mymalloc(sizeof(struct ctx_cam *) * 2);

    /* Now we reserve room for thread 0's context structure and let cam_list[0] point to it */
    cam_list[0] = mymalloc(sizeof(struct ctx_cam));

    /* Populate context structure with start/default values */
    context_init(cam_list[0]);

    /* Initialize some static and global string variables */
    gethostname (cam_list[0]->hostname, PATH_MAX);
    cam_list[0]->hostname[PATH_MAX-1] = '\0';
    /* end of variables */

    /* cam_list[1] pointing to zero indicates no more thread context structures - they get added later */
    cam_list[1] = NULL;

    /*
     * Command line arguments are being pointed to from cam_list[0] and we call conf_load which loads
     * the config options from motion.conf, thread config files and the command line.
     */
    cam_list[0]->conf.argv = argv;
    cam_list[0]->conf.argc = argc;
    cam_list = conf_load(cam_list);
}

static void motion_shutdown(void){
    int i = -1;

    motion_remove_pid();

    webu_stop(cam_list);

    while (cam_list[++i])
        context_destroy(cam_list[i]);

    free(cam_list);
    cam_list = NULL;

    vid_mutex_destroy();
}

static void motion_camera_ids(void){
    /* Set the camera id's on the context.  They must be unique */
    int indx, indx2, invalid_ids;

    /* Set defaults */
    indx = 0;
    while (cam_list[indx] != NULL){
        if (cam_list[indx]->conf.camera_id > 0){
            cam_list[indx]->camera_id = cam_list[indx]->conf.camera_id;
        } else {
            cam_list[indx]->camera_id = indx;
        }
        indx++;
    }

    invalid_ids = FALSE;
    indx = 0;
    while (cam_list[indx] != NULL){
        if (cam_list[indx]->camera_id > 32000) invalid_ids = TRUE;
        indx2 = indx + 1;
        while (cam_list[indx2] != NULL){
            if (cam_list[indx]->camera_id == cam_list[indx2]->camera_id) invalid_ids = TRUE;

            indx2++;
        }
        indx++;
    }
    if (invalid_ids){
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Camara IDs are not unique or have values over 32,000.  Falling back to thread numbers"));
        indx = 0;
        while (cam_list[indx] != NULL){
            cam_list[indx]->camera_id = indx;
            indx++;
        }
    }
}

static void motion_ntc(void){

    #ifdef HAVE_V4L2
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("v4l2   : available"));
    #else
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("v4l2   : not available"));
    #endif

    #ifdef HAVE_WEBP
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("webp   : available"));
    #else
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("webp   : not available"));
    #endif

    #ifdef HAVE_MMAL
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("mmal   : available"));
    #else
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("mmal   : not available"));
    #endif

    #ifdef HAVE_MYSQL
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("mysql  : available"));
    #else
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("mysql  : not available"));
    #endif

    #ifdef HAVE_MARIADB
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("MariaDB: available"));
    #else
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("MariaDB: not available"));
    #endif

    #ifdef HAVE_SQLITE3
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("sqlite3: available"));
    #else
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("sqlite3: not available"));
    #endif

    #ifdef HAVE_PGSQL
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("pgsql  : available"));
    #else
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("pgsql  : not available"));
    #endif

    #ifdef HAVE_GETTEXT
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("nls    : available"));
    #else
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("nls    : not available"));
    #endif


}


/**
 * motion_startup
 *
 *   Responsible for initializing stuff when Motion starts up or is restarted,
 *   including daemon initialization and creating the context struct list.
 *
 * Parameters:
 *
 *   daemonize - non-zero to do daemon init (if the config parameters says so),
 *               or 0 to skip it
 *   argc      - size of argv
 *   argv      - command-line options, passed initially from 'main'
 *
 * Returns: nothing
 */
static void motion_startup(int daemonize, int argc, char *argv[])
{
    int indx;

    /* Initialize our global mutex */
    pthread_mutex_init(&global_lock, NULL);

    /*
     * Create the list of context structures and load the
     * configuration.
     */
    camlist_create(argc, argv);

    if ((cam_list[0]->conf.log_level > ALL) ||
        (cam_list[0]->conf.log_level == 0)) {
        cam_list[0]->conf.log_level = LEVEL_DEFAULT;
        cam_list[0]->log_level = cam_list[0]->conf.log_level;
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Using default log level (%s) (%d)")
            ,get_log_level_str(cam_list[0]->log_level)
            ,SHOW_LEVEL_VALUE(cam_list[0]->log_level));
    } else {
        cam_list[0]->log_level = cam_list[0]->conf.log_level - 1; // Let's make syslog compatible
    }


    if ((cam_list[0]->conf.log_file) && (strncmp(cam_list[0]->conf.log_file, "syslog", 6))) {
        set_log_mode(LOGMODE_FILE);
        ptr_logfile = set_logfile(cam_list[0]->conf.log_file);

        if (ptr_logfile) {
            set_log_mode(LOGMODE_SYSLOG);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Logging to file (%s)"),cam_list[0]->conf.log_file);
            set_log_mode(LOGMODE_FILE);
        } else {
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                ,_("Exit motion, cannot create log file %s")
                ,cam_list[0]->conf.log_file);
            exit(0);
        }
    } else {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Logging to syslog"));
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Motion %s Started",VERSION);

    if ((cam_list[0]->conf.log_type == NULL) ||
        !(cam_list[0]->log_type = get_log_type(cam_list[0]->conf.log_type))) {
        cam_list[0]->log_type = TYPE_DEFAULT;
        cam_list[0]->conf.log_type = mystrcpy(cam_list[0]->conf.log_type, "ALL");
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Using default log type (%s)"),
                   get_log_type_str(cam_list[0]->log_type));
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Using log type (%s) log level (%s)"),
               get_log_type_str(cam_list[0]->log_type), get_log_level_str(cam_list[0]->log_level));

    set_log_level(cam_list[0]->log_level);
    set_log_type(cam_list[0]->log_type);

    indx= 0;
    while (cam_list[indx] != NULL){
        cam_list[indx]->cam_list = cam_list;
        indx++;
    }

    if (daemonize) {
        /*
         * If daemon mode is requested, and we're not going into setup mode,
         * become daemon.
         */
        if (cam_list[0]->daemon && cam_list[0]->conf.setup_mode == 0) {
            become_daemon();
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion running as daemon process"));
        }
    }

    if (cam_list[0]->conf.setup_mode)
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Motion running in setup mode."));

    conf_output_parms(cam_list);

    motion_ntc();

    motion_camera_ids();

    initialize_chars();

    webu_start(cam_list);

    vid_mutex_init();

}

/**
 * motion_start_thread
 *
 *   Called from main when start a motion thread
 *
 * Parameters:
 *
 *   cam - Thread context pointer
 *   thread_attr - pointer to thread attributes
 *
 * Returns: nothing
 */
static void motion_start_thread(struct ctx_cam *cam){
    int i;
    char service[6];
    pthread_attr_t thread_attr;

    if (mystrne(cam->conf_filename, "")){
        cam->conf_filename[sizeof(cam->conf_filename) - 1] = '\0';
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Camera ID: %d is from %s")
            ,cam->camera_id, cam->conf_filename);
    }

    if (cam->conf.netcam_url){
        snprintf(service,6,"%s",cam->conf.netcam_url);
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Camera ID: %d Camera Name: %s Service: %s")
            ,cam->camera_id, cam->conf.camera_name,service);
    } else {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Camera ID: %d Camera Name: %s Device: %s")
            ,cam->camera_id, cam->conf.camera_name,cam->conf.video_device);
    }

    /*
     * Check the stream port number for conflicts.
     * First we check for conflict with the control port.
     * Second we check for that two threads does not use the same port number
     * for the stream. If a duplicate port is found the stream feature gets disabled (port = 0)
     * for this thread and a warning is written to console and syslog.
     */

    if (cam->conf.stream_port != 0) {
        /* Compare against the control port. */
        if (cam_list[0]->conf.webcontrol_port == cam->conf.stream_port) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Stream port number %d for thread %d conflicts with the control port")
                ,cam->conf.stream_port, cam->threadnr);
            MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                ,_("Stream feature for thread %d is disabled.")
                ,cam->threadnr);
            cam->conf.stream_port = 0;
        }
        /* Compare against stream ports of other threads. */
        for (i = 1; cam_list[i]; i++) {
            if (cam_list[i] == cam) continue;

            if (cam_list[i]->conf.stream_port == cam->conf.stream_port) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Stream port number %d for thread %d conflicts with thread %d")
                    ,cam->conf.stream_port, cam->threadnr, cam_list[i]->threadnr);
                MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                    ,_("Stream feature for thread %d is disabled.")
                    ,cam->threadnr);
                cam->conf.stream_port = 0;
            }
        }
    }

    /*
     * Update how many threads we have running. This is done within a
     * mutex lock to prevent multiple simultaneous updates to
     * 'threads_running'.
     */
    pthread_mutex_lock(&global_lock);
    threads_running++;
    pthread_mutex_unlock(&global_lock);

    /* Set a flag that we want this thread running */
    cam->restart = 1;

    /* Give the thread WATCHDOG_TMO to start */
    cam->watchdog = WATCHDOG_TMO;

    /* Flag it as running outside of the thread, otherwise if the main loop
     * checked if it is was running before the thread set it to 1, it would
     * start another thread for this device. */
    cam->running = 1;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&cam->thread_id, &thread_attr, &motion_loop, cam)) {
        /* thread create failed, undo running state */
        cam->running = 0;
        pthread_mutex_lock(&global_lock);
        threads_running--;
        pthread_mutex_unlock(&global_lock);
    }
    pthread_attr_destroy(&thread_attr);

}

static void motion_restart(int argc, char **argv){
    /*
    * Handle the restart situation. Currently the approach is to
    * cleanup everything, and then initialize everything again
    * (including re-reading the config file(s)).
    */
    MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Restarting motion."));
    motion_shutdown();

    SLEEP(2, 0);

    motion_startup(0, argc, argv); /* 0 = skip daemon init */
    MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Motion restarted"));

    restart = 0;
}

static void motion_watchdog(int indx){

    /* Notes:
     * To test scenarios, just double lock a mutex in a spawned thread.
     * We use detached threads because pthread_join would lock the main thread
     * If we only call the first pthread_cancel when we reach the watchdog_kill
     *   it does not break us out of the mutex lock.
     * We keep sending VTAlarms so the pthread_cancel queued can be caught.
     * The calls to pthread_kill 'may' not work or cause crashes
     *   The cancel could finish and then the pthread_kill could be called
     *   on the invalid thread_id which could cause undefined results
     * Even if the cancel finishes it is not clean since memory is not cleaned.
     * The other option instead of cancel would be to exit(1) and terminate everything
     * Best to just not get into a watchdog situation...
     */

    if (!cam_list[indx]->running) return;

    cam_list[indx]->watchdog--;
    if (cam_list[indx]->watchdog == 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Thread %d - Watchdog timeout. Trying to do a graceful restart")
            , cam_list[indx]->threadnr);
        cam_list[indx]->event_stop = TRUE; /* Trigger end of event */
        cam_list[indx]->finish = 1;
    }

    if (cam_list[indx]->watchdog == WATCHDOG_KILL) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Thread %d - Watchdog timeout did NOT restart, killing it!")
            , cam_list[indx]->threadnr);
        if ((cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cam_list[indx]->netcam != NULL)){
            pthread_cancel(cam_list[indx]->netcam->thread_id);
        }
        if ((cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cam_list[indx]->netcam_high != NULL)){
            pthread_cancel(cam_list[indx]->netcam_high->thread_id);
        }
        pthread_cancel(cam_list[indx]->thread_id);
    }

    if (cam_list[indx]->watchdog < WATCHDOG_KILL) {
        if ((cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cam_list[indx]->netcam != NULL)){
            if (!cam_list[indx]->netcam->handler_finished &&
                pthread_kill(cam_list[indx]->netcam->thread_id, 0) == ESRCH) {
                cam_list[indx]->netcam->handler_finished = TRUE;
                pthread_mutex_lock(&global_lock);
                    threads_running--;
                pthread_mutex_unlock(&global_lock);
                netcam_cleanup(cam_list[indx],FALSE);
            } else {
                pthread_kill(cam_list[indx]->netcam->thread_id, SIGVTALRM);
            }
        }
        if ((cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cam_list[indx]->netcam_high != NULL)){
            if (!cam_list[indx]->netcam_high->handler_finished &&
                pthread_kill(cam_list[indx]->netcam_high->thread_id, 0) == ESRCH) {
                cam_list[indx]->netcam_high->handler_finished = TRUE;
                pthread_mutex_lock(&global_lock);
                    threads_running--;
                pthread_mutex_unlock(&global_lock);
                netcam_cleanup(cam_list[indx],FALSE);
            } else {
                pthread_kill(cam_list[indx]->netcam_high->thread_id, SIGVTALRM);
            }
        }
        if (cam_list[indx]->running &&
            pthread_kill(cam_list[indx]->thread_id, 0) == ESRCH){
            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
                ,_("Thread %d - Cleaning thread.")
                , cam_list[indx]->threadnr);
            pthread_mutex_lock(&global_lock);
                threads_running--;
            pthread_mutex_unlock(&global_lock);
            motion_cleanup(cam_list[indx]);
            cam_list[indx]->running = 0;
            cam_list[indx]->finish = 0;
        } else {
            pthread_kill(cam_list[indx]->thread_id,SIGVTALRM);
        }
    }
}

static int motion_check_threadcount(void){
    /* Return 1 if we should break out of loop */

    /* It has been observed that this is not counting every
     * thread running.  The netcams spawn handler threads which are not
     * counted here.  This is only counting context threads and when they
     * all get to zero, then we are done.
     */

    int motion_threads_running, indx;

    motion_threads_running = 0;

    for (indx = (cam_list[1] != NULL ? 1 : 0); cam_list[indx]; indx++) {
        if (cam_list[indx]->running || cam_list[indx]->restart)
            motion_threads_running++;
    }

    /* If the web control/streams are in finish/shutdown, we
     * do not want to count them.  They will be completely closed
     * by the process outside of loop that is checking the counts
     * of threads.  If the webcontrol is not in a finish / shutdown
     * then we want to keep them in the tread count to allow user
     * to restart the cameras and keep Motion running.
     */
    indx = 0;
    while (cam_list[indx] != NULL){
        if ((cam_list[indx]->webcontrol_finish == FALSE) &&
            ((cam_list[indx]->webcontrol_daemon != NULL) ||
             (cam_list[indx]->stream.daemon != NULL))) {
            motion_threads_running++;
        }
        indx++;
    }


    if (((motion_threads_running == 0) && finish) ||
        ((motion_threads_running == 0) && (threads_running == 0))) {
        MOTION_LOG(ALL, TYPE_ALL, NO_ERRNO
            ,_("DEBUG-1 threads_running %d motion_threads_running %d , finish %d")
            ,threads_running, motion_threads_running, finish);
        return 1;
    } else {
        return 0;
    }
}

/**
 * main
 *
 *   Main entry point of Motion. Launches all the motion threads and contains
 *   the logic for starting up, restarting and cleaning up everything.
 *
 * Parameters:
 *
 *   argc - size of argv
 *   argv - command-line options
 *
 * Returns: Motion exit status = 0 always
 */
int main (int argc, char **argv)
{
    int i;

    /* Create the TLS key for thread number. */
    pthread_key_create(&tls_key_threadnr, NULL);
    pthread_setspecific(tls_key_threadnr, (void *)(0));

    setup_signals();

    motion_startup(1, argc, argv);

    movie_global_init();

    dbse_global_init(cam_list);

    mytranslate_init();

    do {
        if (restart) motion_restart(argc, argv);

        for (i = cam_list[1] != NULL ? 1 : 0; cam_list[i]; i++) {
            cam_list[i]->threadnr = i ? i : 1;
            motion_start_thread(cam_list[i]);
        }

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Waiting for threads to finish, pid: %d"), getpid());

        while (1) {
            SLEEP(1, 0);
            if (motion_check_threadcount()) break;

            for (i = (cam_list[1] != NULL ? 1 : 0); cam_list[i]; i++) {
                /* Check if threads wants to be restarted */
                if ((!cam_list[i]->running) && (cam_list[i]->restart)) {
                    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                        ,_("Motion thread %d restart"), cam_list[i]->threadnr);
                    motion_start_thread(cam_list[i]);
                }
                motion_watchdog(i);
            }
        }

        /* Reset end main loop flag */
        finish = 0;

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Threads finished"));

        /* Rest for a while if we're supposed to restart. */
        if (restart) SLEEP(1, 0);

    } while (restart); /* loop if we're supposed to restart */


    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion terminating"));

    movie_global_deinit();

    dbse_global_deinit(cam_list);

    motion_shutdown();

    /* Perform final cleanup. */
    pthread_key_delete(tls_key_threadnr);
    pthread_mutex_destroy(&global_lock);

    return 0;
}

