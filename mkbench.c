/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Watch Resources
 *  ---------------
 *  Copyright (C) 2012, Eduardo Silva P. <edsiper@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "wr.h"
#include "proc.h"

static void wr_banner()
{
}

static void wr_help()
{
    printf("Usage: ./mkbench [-c N] [-l N] [-s N] -p PID http://URL\n\n");
    printf("%sAvailable options%s\n", ANSI_BOLD, ANSI_RESET);
    printf("  -p  \tserver process ID to test\n");
    printf("  -r  \tnumber of requests (default: %i)\n", WR_REQUESTS);
    printf("  -c  \tinitial concurrency (default: %i)\n", WR_CONC_FROM);
    printf("  -t  \tnumber of threads (default: %i)\n", WR_THREADS);
    printf("  -l  \tmax concurrency after each step (default: %i)\n", WR_CONC_TO);
    printf("  -s  \tnumber of concurrency steps (default: %i)\n", WR_CONC_STEP);
    printf("  -N  \tdo not gather system resources usage\n");
    printf("  -v  \tshow version number\n");
    printf("  -h, \tprint this help\n\n");
    fflush(stdout);
}

static void wr_version()
{
}

/*
 * Given a parent ID process and the list of childs, perform a sumatory of
 * basic statistics over fields wr_rss, wr_utime_ms and wr_stime_ms.
 */
static struct wr_proc_task *bench_stats_sum(pid_t ppid,
                                            struct wr_proc_task **childs,
                                            int childs_count)
{
    int i;
    char *tmp;
    struct wr_proc_task *child;
    struct wr_proc_task *st;
    struct wr_proc_task *final;

    final = wr_proc_stat(ppid);

    for (i = 0; i < childs_count; i++) {
        child = (childs)[i];

        st = wr_proc_stat(child->pid);

        final->wr_rss      += st->wr_rss;
        final->wr_utime_ms += st->wr_utime_ms;
        final->wr_stime_ms += st->wr_stime_ms;

        wr_proc_free(st);
    }

    /* Get the average usage after sumatory */
    final->wr_rss      /= (childs_count + 1);
    tmp = human_readable_size(final->wr_rss);
    free(final->wr_rss_hr);
    final->wr_rss_hr = tmp;
    final->wr_utime_ms /= (childs_count + 1);
    final->wr_stime_ms /= (childs_count + 1);

    return final;
}

long spawn_benchmark(const char *cmd)
{
    char *s, *p;
    const size_t buf_size = 4096;
    char buf[buf_size];
    FILE *f;

    f = popen(cmd, "r");

    /* Find the results line */
    while (fgets(buf, sizeof(buf) - 1, f)) {
        if (strncmp(buf, "finished ", 9) == 0) {
            s = strstr(buf, " req/s,");
            if (!s) {
                printf("Error: Invalid benchmarking tool output\n");
                exit(EXIT_FAILURE);
            }
            p = (s - 1);
            while (*p != ' ') *p--;
            *p++;

            strncpy(buf, p, (s - p));
            buf[(s - p)] = '\0';
            break;
        }
    }
    fclose(f);

    return atol(buf);
}

void bench_time(time_t t)
{
    const int one_day  = 86400;
    const int one_hour = 3600;
    const int one_min  = 60;

    int days;
    int hours;
    int minutes;
    int seconds;
    long int upmind;
    long int upminh;

    /* days */
    days = t / one_day;
    upmind = t - (days * one_day);

    /* hours */
    hours = upmind / one_hour;
    upminh = upmind - hours * one_hour;

    /* minutes */
    minutes = upminh / one_min;
    seconds = upminh - minutes * one_min;

    printf("Elapsed time: %i day%s, %i hour%s, %i minute%s and %i second%s\n\n",
           days, (days > 1) ? "s" : "", hours, (hours > 1) ? "s" : "", minutes,
           (minutes > 1) ? "s" : "", seconds, (seconds > 1) ? "s" : "");
}

int main(int argc, char **argv)
{
    int opt;
    int pid = -1;
    int ret;
    int disable_stats = 0;
    int keepalive   = 0;
    int conc_from   = WR_CONC_FROM;
    int conc_to     = WR_CONC_TO;
    int conc_step   = WR_CONC_STEP;
    int conc_bench  = conc_from;
    int requests    = WR_REQUESTS;
    int threads     = WR_THREADS;
    int childs_size = 256;
    int childs_count;
    long req_sec    = 0;
    const size_t buf_size = 4096;
    char buf[buf_size];
    time_t init_time;
    time_t end_time;
    struct wr_proc_task *task_old, *task_new;
    struct wr_proc_task *childs;

    wr_banner();

    while ((opt = getopt(argc, argv, "vhkr:c:t:s:l:p:N")) != -1) {
        switch (opt) {
        case 'v':
            wr_version();
            exit(EXIT_SUCCESS);
        case 'h':
            wr_help();
            exit(EXIT_SUCCESS);
        case 'k':
            keepalive = 1;
            break;
        case 'r':
            requests = atoi(optarg);
            break;
        case 'c':
            conc_from = atoi(optarg);
            break;
        case 't':
            threads = atoi(optarg);
            break;
        case 'l':
            conc_to = atoi(optarg);
            break;
        case 's':
            conc_step = atoi(optarg);
            break;
        case 'p':
            pid = atoi(optarg);
            break;
        case 'N':
            disable_stats = 1;
            break;
        case '?':
            printf("Error: Invalid option or option needs an argument.\n");
            wr_help();
            exit(EXIT_FAILURE);
        }
    }

    if (pid <= 0 && disable_stats == 0) {
        printf("Error: Process ID (PID) not specified\n\n");
        wr_help();
        exit(EXIT_FAILURE);
    }

    /* Kernel information: PAGESIZE and CPU_HZ */
    wr_pagesize  = sysconf(_SC_PAGESIZE);
    wr_cpu_hz    = sysconf(_SC_CLK_TCK);

    /* Check PID children */
    if (disable_stats == 0) {
        childs_count = childs_size;
        childs = malloc(sizeof(struct wr_proc_task) * childs_size);
        ret = wr_proc_get_childs(pid, &childs, &childs_count);
        if (ret == -1) {
            printf("Error: failed processing process children.\n");
            wr_help();
            exit(EXIT_FAILURE);
        }
    }

    /* Get the URL */
    if (strncmp(argv[argc - 1], "http", 4) != 0) {
        printf("Error: Invalid URL\n\n");
        wr_help();
        exit(EXIT_FAILURE);
    }

    /* Validate input arguments, do not mess up with weighttp */
    if (threads > requests || threads > conc_from || conc_from > requests) {
        printf("Error: insane arguments\n\n");
        wr_help();
        exit(EXIT_FAILURE);
    }

    /* Initial details */
    if (disable_stats == 0) {
        task_old = wr_proc_stat(pid);
        printf("Process ID   : %i\n", pid);
        printf("Process name : %s\n", task_old->comm);
        printf("Childs found : %i\n", childs_count);
        if (childs_count > 0) {
            int i;
            struct wr_proc_task *t;
            for (i = 0 ; i < childs_count; i++) {
                t = (&childs)[i];
                printf("               pid=%i (%s)\n", t->pid, t->comm);
            }
        }
        wr_proc_free(task_old);
    }

    printf("Concurrency  : from %i to %i, step %i\n", conc_from, conc_to, conc_step);

    /* Command */
    memset(buf, '\0', sizeof(buf));
    snprintf(buf, buf_size - 1, BC_BIN,
             threads,
             requests, conc_from, keepalive > 0 ? "-k": "", argv[argc - 1]);
    printf("Command      : %s\n\n", buf);

    /* Table header */
    printf("concurrency  requests/second  user time (ms)  "
           "system time (ms)  Mem (bytes)   Mem unit \n"
           "-----------  ---------------  --------------  "
           "----------------  -----------   ---------\n");

    init_time = time(NULL);
    conc_bench = conc_from;
    while (conc_bench <= conc_to) {
        if (disable_stats) {
            req_sec  = spawn_benchmark(buf);
            printf("%11i %16ld\n",
                   conc_bench,
                   req_sec);
        }
        else {
            task_old = bench_stats_sum(pid, &childs, childs_count);
            req_sec  = spawn_benchmark(buf);
            task_new = bench_stats_sum(pid, &childs, childs_count);

            printf("%11i %16ld %15ld %17ld %12ld %11s\n",
                   conc_bench,
                   req_sec,
                   (task_new->wr_utime_ms - task_old->wr_utime_ms),
                   (task_new->wr_stime_ms - task_old->wr_stime_ms),
                   task_new->wr_rss,
                   task_new->wr_rss_hr);
            wr_proc_free(task_old);
            wr_proc_free(task_new);
        }
        /* Prepare the command */
        conc_bench += conc_step;
        memset(buf, '\0', sizeof(buf));
        snprintf(buf, buf_size - 1, BC_BIN,
                 threads,
                 requests, conc_bench, keepalive > 0 ? "-k": "", argv[argc - 1]);
    }

    printf("\n--\n");
    end_time = time(NULL);
    bench_time((end_time - init_time));
    fflush(stdout);
    return 0;
}
