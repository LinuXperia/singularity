/* 
 * Copyright (c) 2015-2017, Gregory M. Kurtzer. All rights reserved.
 * 
 * Copyright (c) 2016-2017, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 * 
 * This software is licensed under a customized 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 * NOTICE.  This Software was developed under funding from the U.S. Department of
 * Energy and the U.S. Government consequently retains certain rights. As such,
 * the U.S. Government has been granted for itself and others acting on its
 * behalf a paid-up, nonexclusive, irrevocable, worldwide license in the Software
 * to reproduce, distribute copies to the public, prepare derivative works, and
 * perform publicly and display publicly, and to permit other to do so. 
 * 
*/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>

#include "util/file.h"
#include "util/util.h"
#include "lib/message.h"
#include "lib/privilege.h"
#include "lib/config_parser.h"
#include "lib/fork.h"
#include "lib/registry.h"

#include "../image.h"
#include "./sessiondir.h"


//void singularity_image_sessiondir_init(struct image_object *image);
//int singularity_image_sessiondir_create(struct image_object *image);
//int singularity_image_sessiondir_remove(struct image_object *image);



void _singularity_image_sessiondir_init(struct image_object *image) {
    char *sessiondir_prefix;
    char *sessiondir_suffix;
    char *file = strdup(image->path);
    struct stat imagestat;
    int sessiondir_suffix_len;
    uid_t uid = singularity_priv_getuid();

    if ( file == NULL ) {
        singularity_message(ERROR, "Internal error, image->path undefined\n");
        ABORT(222);
    }

    if ( ( sessiondir_prefix = singularity_registry_get("SESSIONDIR") ) != NULL ) {
        singularity_message(DEBUG, "Got sessiondir_prefix from environment: '%s'\n", sessiondir_prefix);
    } else if ( ( sessiondir_prefix = strdup(singularity_config_get_value(SESSIONDIR_PREFIX)) ) != NULL ) {
        singularity_message(DEBUG, "Got sessiondir_prefix from configuration: '%s'\n", sessiondir_prefix);
    } else {
        singularity_message(ERROR, "Could not obtain the session directory prefix.\n");
        ABORT(255);
    }
    singularity_message(DEBUG, "Set sessiondir_prefix to: %s\n", sessiondir_prefix);

    if ( stat(file, &imagestat) < 0 ) {
        singularity_message(ERROR, "Failed calling stat() on %s: %s\n", file, strerror(errno));
        ABORT(255);
    }

    sessiondir_suffix_len = intlen((int)uid) + intlen((int)imagestat.st_dev) + intlen((long unsigned)imagestat.st_ino) + 3;

    sessiondir_suffix = (char *) malloc(sessiondir_suffix_len);

    if ( snprintf(sessiondir_suffix, sessiondir_suffix_len, "%d.%d.%lu", (int)uid, (int)imagestat.st_dev, (long unsigned)imagestat.st_ino) != sessiondir_suffix_len -1 ) {
        singularity_message(ERROR, "Failed creating sessiondir_suffix: %s\n", strerror(errno));
        ABORT(255);
    }
    singularity_message(DEBUG, "Set sessiondir_suffix to: %s\n", sessiondir_suffix);

    if ( ( image->sessiondir = strcat(sessiondir_prefix, sessiondir_suffix) ) == NULL ) {
        singularity_message(ERROR, "Could not set image->sessiondir\n");
        ABORT(255);
    }

    singularity_registry_set("sessiondir", image->sessiondir);

    singularity_message(VERBOSE, "Creating session directory: %s\n", image->sessiondir);

    if ( s_mkpath(image->sessiondir, 0755) < 0 ) {
        singularity_message(ERROR, "Failed creating session directory %s: %s\n", image->sessiondir, strerror(errno));
        ABORT(255);
    }

    singularity_message(DEBUG, "Opening sessiondir file descriptor\n");
    if ( ( image->sessiondir_fd = open(image->sessiondir, O_CLOEXEC | O_RDONLY) ) < 0 ) { // Flawfinder: ignore
        singularity_message(ERROR, "Could not obtain file descriptor for session directory %s: %s\n", image->sessiondir, strerror(errno));
        ABORT(255);
    }

    singularity_message(DEBUG, "Setting shared flock() on session directory\n");
    if ( flock(image->sessiondir_fd, LOCK_SH | LOCK_NB) < 0 ) {
        singularity_message(ERROR, "Could not obtain shared lock on %s: %s\n", image->sessiondir, strerror(errno));
        ABORT(255);
    }

    return;
}



/*




char *sessiondir = NULL;
int sessiondir_fd = 0;

char *singularity_sessiondir_init(char *file) {

    char *file = strdup(image->path);
    pid_t child_pid;
    int retval;


    if ( file == NULL ) {
        singularity_message(DEBUG, "Got null for file, returning prior sessiondir\n");
    } else {
        char *sessiondir_prefix;
        struct stat filestat;
        uid_t uid = singularity_priv_getuid();

        sessiondir = (char *) malloc(PATH_MAX);

        singularity_message(DEBUG, "Checking Singularity configuration for 'sessiondir prefix'\n");

        if (stat(file, &filestat) < 0) {
            singularity_message(ERROR, "Failed calling stat() on %s: %s\n", file, strerror(errno));
            return(NULL);
        }

        if ( ( sessiondir_prefix = envar_path("SINGULARITY_SESSIONDIR") ) != NULL ) {
            if (snprintf(sessiondir, PATH_MAX, "%s/singularity-session-%d.%d.%lu", sessiondir_prefix, (int)uid, (int)filestat.st_dev, (long unsigned)filestat.st_ino) >= PATH_MAX) { // Flawfinder: ignore
                singularity_message(ERROR, "Overly-long session directory specified.\n");
                ABORT(255);
            }
        } else if ( ( sessiondir_prefix = strdup(singularity_config_get_value(SESSIONDIR_PREFIX)) ) != NULL ) {
            if (snprintf(sessiondir, PATH_MAX, "%s%d.%d.%lu", sessiondir_prefix, (int)uid, (int)filestat.st_dev, (long unsigned)filestat.st_ino) >= PATH_MAX) { // Flawfinder: ignore
                singularity_message(ERROR, "Overly-long session directory specified.\n");
                ABORT(255);
            }
        } else {
            singularity_message(ERROR, "Programming error - default for %s returned NULL.\n", sessiondir_prefix);
            ABORT(255);
        }
        singularity_message(DEBUG, "Set sessiondir to: %s\n", sessiondir);
        free(sessiondir_prefix);
    }

    if ( is_dir(sessiondir) < 0 ) {
        if ( s_mkpath(sessiondir, 0755) < 0 ) {
            singularity_message(ERROR, "Failed creating session directory %s: %s\n", sessiondir, strerror(errno));
            ABORT(255);
        }
    }

    if ( is_owner(sessiondir, singularity_priv_getuid()) < 0 ) {
        singularity_message(ERROR, "Session directory has wrong ownership: %s\n", sessiondir);
        ABORT(255);
    }

    singularity_message(DEBUG, "Opening sessiondir file descriptor\n");
    if ( ( sessiondir_fd = open(sessiondir, O_CLOEXEC | O_RDONLY) ) < 0 ) { // Flawfinder: ignore
        singularity_message(ERROR, "Could not obtain file descriptor for session directory %s: %s\n", sessiondir, strerror(errno));
        ABORT(255);
    }

    singularity_message(DEBUG, "Setting shared flock() on session directory\n");
    if ( flock(sessiondir_fd, LOCK_SH | LOCK_NB) < 0 ) {
        singularity_message(ERROR, "Could not obtain shared lock on %s: %s\n", sessiondir, strerror(errno));
        ABORT(255);
    }

    if ( ( envar_defined("SINGULARITY_NOSESSIONCLEANUP") == TRUE ) || ( envar_defined("SINGULARITY_NOCLEANUP") == TRUE ) ) {
        singularity_message(VERBOSE2, "Not forking a sessiondir cleanup process\n");

    } else {
        if ( ( child_pid = singularity_fork() ) > 0 ) {
            int tmpstatus;
            char *rundir = envar_path("SINGULARITY_RUNDIR");

            singularity_message(DEBUG, "Cleanup thread waiting on child...\n");

            waitpid(child_pid, &tmpstatus, 0);
            if (WIFEXITED(tmpstatus)) {
                retval = WEXITSTATUS(tmpstatus);
                singularity_message(DEBUG, "Child exited with status %d.\n", retval);
            } else {
                retval = WTERMSIG(tmpstatus);
                singularity_message(DEBUG, "Child exited with signal %d.\n", retval);
            }

            singularity_message(DEBUG, "Checking to see if we are the last process running in this sessiondir\n");
            if ( flock(sessiondir_fd, LOCK_EX | LOCK_NB) == 0 ) {
                singularity_message(VERBOSE, "Cleaning sessiondir: %s\n", sessiondir);
                if ( s_rmdir(sessiondir) < 0 ) {
                    singularity_message(ERROR, "Could not remove session directory %s: %s\n", sessiondir, strerror(errno));
                }
            }

            if ( rundir != NULL ) {
                if ( strncmp(rundir, "/tmp/", 5) == 0 ) {
                    singularity_message(VERBOSE, "Cleaning run directory: %s\n", rundir);
                    if ( s_rmdir(rundir) < 0 ) {
                        singularity_message(ERROR, "Could not remove run directory %s: %s\n", rundir, strerror(errno));
                    }
                } else {
                    singularity_message(VERBOSE, "Not removing the SINGULARITY_RUNDIR when not in /tmp: %s\n", rundir);
                }
            }

            free(rundir);
    
            if (WIFEXITED(tmpstatus)) {
                exit(retval);
            } else {
                // Try to reset this signal handler to the default one.
                // Raise will _only_ return after the signal is handled,
                // meaning that the default case for this signal is likely to
                // be ignored.  In such a case, we abort.
                signal(retval, SIG_DFL);  // Ignore failures; we'll abort later.
                raise(retval);
                singularity_message(ERROR, "Payload process failed with signal %d.\n", retval);
                ABORT(255);
            }
        }
    }

    return(sessiondir);
}

char *singularity_sessiondir_get(void) {
    if ( sessiondir == NULL ) {
        singularity_message(ERROR, "Doh, session directory has not been setup!\n");
        ABORT(255);
    }
    singularity_message(DEBUG, "Returning: %s\n", sessiondir);
    return(sessiondir);
}

int singularity_sessiondir_rm(void) {
    if ( sessiondir == NULL ) {
        singularity_message(ERROR, "Session directory is NULL, can not remove nullness!\n");
        return(-1);
    }

    singularity_message(DEBUG, "Checking to see if we are the last process running in this sessiondir\n");
    if ( flock(sessiondir_fd, LOCK_EX | LOCK_NB) == 0 ) {
        singularity_priv_escalate();
        singularity_message(VERBOSE, "Cleaning sessiondir: %s\n", sessiondir);
        if ( s_rmdir(sessiondir) < 0 ) {
            singularity_message(ERROR, "Could not remove session directory %s: %s\n", sessiondir, strerror(errno));
        }
        singularity_priv_drop();
    }
    
    return(0);
}

*/
