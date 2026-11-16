/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * unserviced-exception-port <command> [args...]
 *
 * Runs the command the way a launcher with a task-level crash reporter
 * (Sentry, Crashpad) would: with the task's Mach exception ports for bad
 * access, bad instruction and arithmetic exceptions pointed at a port.
 * Exception ports are inherited across fork and exec. This process keeps
 * the port's receive right and never answers on it, so a fault that goes
 * to the port instead of becoming a signal leaves the faulting thread
 * waiting forever. Exits with the command's exit status.
 *
 * A Mac binary rather than a Python script: macOS 15 doesn't let Apple's
 * own binaries (the system python3 among them) set exception ports like
 * this; the call never returns.
 */

#include <mach/mach.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t child;

static void forward(int sig)
{
    /* The runner times a test out with SIGALRM, which would otherwise end
     * only this process and leave the command running. */
    kill(child, SIGKILL);
    _exit(128 + sig);
}

int main(int argc, char **argv)
{
    mach_port_t port;
    kern_return_t kr;
    int status;

    if (argc < 2)
    {
        fprintf(stderr, "usage: unserviced-exception-port <command> [args...]\n");
        return 2;
    }
    kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    if (!kr) kr = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
    if (!kr) kr = task_set_exception_ports(mach_task_self(),
                                           EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION | EXC_MASK_ARITHMETIC,
                                           port, EXCEPTION_DEFAULT, THREAD_STATE_NONE);
    if (kr)
    {
        fprintf(stderr, "unserviced-exception-port: Mach call failed (%d)\n", kr);
        return 2;
    }
    printf("unserviced-exception-port: exception ports set to port %#x\n", port);
    fflush(stdout);

    if (!(child = fork()))
    {
        execvp(argv[1], argv + 1);
        perror(argv[1]);
        _exit(127);
    }
    /* This process's own faults would now deadlock on the port too, so it
     * does nothing but wait. */
    signal(SIGALRM, forward);
    signal(SIGTERM, forward);
    while (waitpid(child, &status, 0) == -1) ;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
