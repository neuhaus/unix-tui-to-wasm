/* pdcurses-emscripten.c - let PDCurses sleeps yield to the browser.
 *
 * PDCursesMod's VT port implements PDC_napms() with usleep(). Under
 * Emscripten that blocks the JavaScript event loop, so keyboard input and
 * timers freeze until the nap ends. The PDCurses objects are compiled with
 * usleep=utw_emscripten_usleep (see pdcurses.cmake); this function hands the
 * wait to Asyncify instead.
 *
 * PDC_napms() measures elapsed time and loops, so it works unchanged with a
 * sleep that may return a little early or late.
 */

#include <emscripten.h>
#include <unistd.h>

int utw_emscripten_usleep(useconds_t usec)
{
	emscripten_sleep((unsigned int)(usec / 1000));
	return 0;
}
