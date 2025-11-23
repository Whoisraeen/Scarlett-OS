#ifndef _SCARLETTOS_PROCESS_H
#define _SCARLETTOS_PROCESS_H

#include <scarlettos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t sc_pid_t;

// Process creation and management
sc_pid_t sc_fork(void);
int sc_execve(const char *filename, char *const argv[], char *const envp[]);
sc_pid_t sc_wait(int *status);
sc_pid_t sc_waitpid(sc_pid_t pid, int *status, int options);
void sc_exit(int status);
sc_pid_t sc_getpid(void);
sc_pid_t sc_getppid(void);

// Process capabilities
int sc_cap_grant(sc_pid_t pid, uint64_t cap, uint64_t permissions);
int sc_cap_revoke(sc_pid_t pid, uint64_t cap);

#ifdef __cplusplus
}
#endif

#endif // _SCARLETTOS_PROCESS_H
