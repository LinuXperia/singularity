/* 
 * Copyright (c) 2017, SingularityWare, LLC. All rights reserved.
 *
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


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "config.h"
#include "util/file.h"
#include "util/util.h"
#include "util/daemon.h"
#include "util/registry.h"
#include "lib/image/image.h"
#include "lib/runtime/runtime.h"
#include "util/privilege.h"


void daemon_join(void) {
    char *pid_str, *ns_path, *proc_path, *uid_str, *ns_fd_str;
    int lock_result, ns_fd;
    int *lock_fd = malloc(sizeof(int));

    uid_str = int2str(singularity_priv_getuid());
    daemon_path(uid_str);
    free(uid_str);
    
    if( is_file(singularity_registry_get("DAEMON_FILE")) ) {
        /* Check if there is a lock on daemon file */
        lock_result = filelock(singularity_registry_get("DAEMON_FILE"), lock_fd);

        if( lock_result == 0 ) {
            /* Successfully obtained lock, no daemon controls this file. */
            close(*lock_fd);
            return;
        } else if( lock_result == EALREADY ) {
            /* EALREADY is set when another process has a lock on the file. */
            pid_str = filecat(singularity_registry_get("DAEMON_FILE"));
            proc_path = joinpath("/proc/", pid_str);
            ns_path = joinpath(proc_path, "/ns");

            free(proc_path);
            free(pid_str);

            /* Open FD to /proc/[PID]/ns directory to call openat() for ns files */
            ns_fd = open(ns_path, O_RDONLY | O_CLOEXEC);
            ns_fd_str = int2str(ns_fd);

            /* Set DAEMON_NS_FD to /proc/[PID]/ns FD in registry */
            singularity_registry_set("DAEMON_NS_FD", ns_fd_str);

            /* Set DAEMON as 1 in registry, to signal that we want to join the running daemon */
            singularity_registry_set("DAEMON", "1");
        }
    }
}

void daemon_path(char *host_uid) {
    char *image_devino, *daemon_path, *image_name;
    int daemon_path_len;
    
    /* Build string with daemon file location */
    image_name = singularity_registry_get("IMAGE");
    image_devino = file_devino(image_name);
    
    daemon_path_len = strlength("/tmp/.singularity-daemon-", 2048) + strlength(host_uid, 2048) +
        strlength(image_devino, 2048) + strlength(image_name, 2048) + 3; //+3 for "/", "-", "\0"
    
    daemon_path = (char *)malloc((daemon_path_len) * sizeof(char)); 
    snprintf(daemon_path, daemon_path_len, "/tmp/.singularity-daemon-%s/%s-%s",
             host_uid, image_devino, image_name);

    /* Store daemon_file string in registry as DAEMON_FILE */
    singularity_registry_set("DAEMON_FILE", daemon_path);
    
    free(image_name);
    free(image_devino);
    free(daemon_path);
}
