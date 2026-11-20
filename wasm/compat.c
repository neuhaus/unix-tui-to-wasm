/* compat.c - libc shims that every browser build needs.
 *
 * Emscripten's libc has no passwd database (getpwuid() returns NULL) and
 * rejects every setuid() call, which breaks programs written for setuid
 * games. The browser has exactly one user and no privileges to drop, so
 * these shims replace the libc functions in the link: the name and home
 * directory come from $USER and $HOME (pre-js can set them), and setuid()
 * succeeds without doing anything.
 *
 * Compiled on every platform; outside Emscripten it is empty so native
 * builds keep using the system libc.
 */

#ifdef __EMSCRIPTEN__

#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static struct passwd *wasm_passwd(uid_t uid, const char *name)
{
	static struct passwd pw;
	const char *user = getenv("USER");
	const char *home = getenv("HOME");

	if (!user || !*user) user = "player";
	if (name && strcmp(name, user) != 0) return NULL;

	pw.pw_name = (char *)user;
	pw.pw_passwd = (char *)"*";
	pw.pw_uid = uid;
	pw.pw_gid = (gid_t)uid;
	pw.pw_gecos = (char *)"";
	pw.pw_dir = (char *)(home && *home ? home : "/");
	pw.pw_shell = (char *)"/bin/sh";
	return &pw;
}

struct passwd *getpwuid(uid_t uid)
{
	return wasm_passwd(uid, NULL);
}

struct passwd *getpwnam(const char *name)
{
	return wasm_passwd(getuid(), name);
}

int setuid(uid_t uid)
{
	(void)uid;
	return 0;
}

/* There are no processes in the browser. Emscripten's own stub returns -1
 * with ENOSYS; defining it here keeps the linker from pulling Emscripten's
 * libstubs object, which also defines getpwuid()/getpwnam() (the shims
 * above) and would then be a duplicate. PDCurses' bell helper is the caller:
 * it forks 'play' and falls back to a screen flash when fork fails. */
pid_t fork(void)
{
	errno = ENOSYS;
	return (pid_t)-1;
}

/* execvp() in Emscripten's libc calls execve(); the only execve() is in the
 * same libstubs object as getpwuid()/getpwnam(). Providing it here keeps the
 * object out of the link, with Emscripten's own behaviour. */
int execve(const char *path, char *const argv[], char *const envp[])
{
	(void)path;
	(void)argv;
	(void)envp;
	errno = ENOSYS;
	return -1;
}

#else

/* Keep the translation unit non-empty for strict compilers. */
typedef int utw_compat_translation_unit;

#endif /* __EMSCRIPTEN__ */
