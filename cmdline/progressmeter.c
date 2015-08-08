/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * progressmeter.c : command-line progress bar
 *
 * Based on implementation of "bar" progress bar of GNU Wget,
 * Copyright (C) 2001, 2002 Free Software Foundation, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <signal.h>
#ifndef WIN32
#include <sys/ioctl.h>
#include <termios.h>
#else
#include <windows.h>
#include <io.h>
#endif

#include "progressmeter.h"
#include "wavprg_types.h"
#include "name_utils.h"

/* "Thermometer" (bar) progress. */

/* Assumed screen width if we can't find the real value.  */
#define DEFAULT_SCREEN_WIDTH 80

/* Minimum screen width we'll try to work with.  If this is too small,
   create_image will overflow the buffer.  */
#define MINIMUM_SCREEN_WIDTH 45

/* The last known screen width.  This can be updated by the code that
   detects that SIGWINCH was received (but it's never updated from the
   signal handler).  */
static int screen_width;

/* A flag that, when set, means SIGWINCH was received.  */
static volatile int received_sigwinch;

struct display_interface_internal {
  long initial_length;		/* how many bytes have been downloaded
				                     previously. */
  long total_length;		/* expected total byte count when the
				                   download finishes */
  long count;			/* bytes downloaded so far */

  int width;			/* screen width we're using at the
				             time the progress gauge was
				             created.  this is different from
				             the screen_width global variable in
				             that the latter can be changed by a
				             signal. */
  char *buffer;			/* buffer where the bar "image" is
				               stored. */
  int tick;			/* counter used for drawing the
				           progress bar where the total size
				           is not known. */
};

static void create_image  (struct display_interface_internal *bp);
static void display_image (char * buffer);

/* Determine the width of the terminal we're running on.  If that's
   not possible, return 0.  */

static
int
determine_screen_width (void)
{
  /* If there's a way to get the terminal size using POSIX
     tcgetattr(), somebody please tell me.  */
#ifdef TIOCGWINSZ
  int fd;
  struct winsize wsz;

  fd = fileno (stdout);
  if (ioctl (fd, TIOCGWINSZ, &wsz) < 0)
    return 0;			/* most likely ENOTTY */

  return wsz.ws_col;
#else  /* not TIOCGWINSZ */
# ifdef WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo (GetStdHandle (STD_ERROR_HANDLE), &csbi))
    return 0;
  return csbi.dwSize.X;
# else /* neither WINDOWS nor TIOCGWINSZ */
  return 0;
#endif /* neither WINDOWS nor TIOCGWINSZ */
#endif /* not TIOCGWINSZ */
}

static void
progress_handle_sigwinch (int sig);

static void progressmeter_set_totalbytes(struct display_interface_internal *bp, uint32_t length, const char* name, uint32_t index, uint32_t total)
{
  char ascii_name[17];

#ifdef SIGWINCH
  signal(SIGWINCH, progress_handle_sigwinch);
#endif

  if (length > 65535)
    length = 65535;
  convert_petscii_string(name, ascii_name, wav2prg_true);
  printf("Converting %s (%u of %u)\n", ascii_name, index, total);

  bp->initial_length = 0;
  bp->count          = 0;
  bp->total_length   = length;

  /* Initialize screen_width if this hasn't been done or if it might
     have changed, as indicated by receiving SIGWINCH.  */
  if (!screen_width || received_sigwinch)
  {
    screen_width = determine_screen_width ();
    if (!screen_width)
    	screen_width = DEFAULT_SCREEN_WIDTH;
    else if (screen_width < MINIMUM_SCREEN_WIDTH)
    	screen_width = MINIMUM_SCREEN_WIDTH;
    received_sigwinch = 0;
  }

  /* - 1 because we don't want to use the last screen column. */
  bp->width = screen_width - 1;
  /* + 1 for the terminating zero. */
  bp->buffer = malloc (bp->width + 1);

  create_image (bp);
  display_image (bp->buffer);
}

void
progressmeter(struct display_interface_internal *bp, uint32_t length)
{
  bp->count = length;
  if (bp->total_length > 0
      && bp->count + bp->initial_length > bp->total_length)
    /* We could be downloading more than total_length, e.g. when the
       server sends an incorrect Content-Length header.  In that case,
       adjust bp->total_length to the new reality, so that the code in
       create_image() that depends on total size being smaller or
       equal to the expected size doesn't abort.  */
    bp->total_length = bp->initial_length + bp->count;

  /* If SIGWINCH (the window size change signal) been received,
     determine the new screen size and update the screen.  */
  if (received_sigwinch)
  {
    int old_width = screen_width;
    screen_width = determine_screen_width ();
    if (!screen_width)
    	screen_width = DEFAULT_SCREEN_WIDTH;
    else if (screen_width < MINIMUM_SCREEN_WIDTH)
	    screen_width = MINIMUM_SCREEN_WIDTH;
    if (screen_width != old_width)
	  {
	    bp->width = screen_width - 1;
	    bp->buffer = realloc (bp->buffer, bp->width + 1);
	  }
    received_sigwinch = 0;
  }

  create_image (bp);
  display_image (bp->buffer);
}

void
progressmeter_finish(struct display_interface_internal *bp)
{
  if (isatty (fileno (stdout)))
    fputs("\n",stdout);
  free(bp->buffer);
}

#define APPEND_LITERAL(s) do {			\
  memcpy (p, s, sizeof (s) - 1);		\
  p += sizeof (s) - 1;				\
} while (0)

#ifndef MAX
# define MAX(a, b) ((a) >= (b) ? (a) : (b))
#endif

static void
create_image (struct display_interface_internal *bp)
{
  char *p = bp->buffer;
  long size = bp->initial_length + bp->count;

  /* The progress bar should look like this:
     xx% [=======>             ] nnnnn

     Calculate the geometry.  The idea is to assign as much room as
     possible to the progress bar.  The other idea is to never let
     things "jitter", i.e. pad elements that vary in size so that
     their variance does not affect the placement of other elements.
     It would be especially bad for the progress bar to be resized
     randomly.

     "xx% " or "100%"  - percentage               - 4 chars
     "[]"              - progress bar decorations - 2 chars

     "=====>..."       - progress bar             - the rest
  */
  int progress_size = bp->width - (4 + 2);

  if (progress_size < 5)
    progress_size = 0;

  /* "xx% " */
  if (bp->total_length > 0)
  {
    int percentage = (int)(100.0 * size / bp->total_length);

    assert (percentage <= 100);

    if (percentage < 100)
    	sprintf (p, "%2d%% ", percentage);
    else
    	strcpy (p, "100%");
    p += 4;
  }
  else
    APPEND_LITERAL ("    ");

  /* The progress bar: "[====>      ]" or "[++==>      ]". */
  if (progress_size && bp->total_length > 0)
  {
    /* Size of the initial portion. */
    int insz = (int)((double)bp->initial_length / bp->total_length * progress_size);

    /* Size of the downloaded portion. */
    int dlsz = (int)((double)size / bp->total_length * progress_size);

    char *begin;
    int i;

    assert (dlsz <= progress_size);
    assert (insz <= dlsz);

    *p++ = '[';
    begin = p;

    /* Print the initial portion of the download with '+' chars, the
       rest with '=' and one '>'.  */
    for (i = 0; i < insz; i++)
      *p++ = '+';
    dlsz -= insz;
    if (dlsz > 0)
    {
      for (i = 0; i < dlsz - 1; i++)
        *p++ = '=';
      *p++ = '>';
    }

    while (p - begin < progress_size)
    	*p++ = ' ';
    *p++ = ']';
  }
  else if (progress_size)
  {
    /* If we can't draw a real progress bar, then at least show
       *something* to the user.  */
    int ind = bp->tick % (progress_size * 2 - 6);
    int i, pos;

    /* Make the star move in two directions. */
    if (ind < progress_size - 2)
    	pos = ind + 1;
    else
    	pos = progress_size - (ind - progress_size + 5);

    *p++ = '[';
    for (i = 0; i < progress_size; i++)
    {
      if      (i == pos - 1) *p++ = '<';
      else if (i == pos    ) *p++ = '=';
      else if (i == pos + 1) *p++ = '>';
      else
        *p++ = ' ';
    }
    *p++ = ']';

    ++bp->tick;
  }

  assert (p - bp->buffer <= bp->width);

  while (p < bp->buffer + bp->width)
    *p++ = ' ';
  *p = '\0';
}

/* Print the contents of the buffer as a one-line ASCII "image" so
   that it can be overwritten next time.  */

static void
display_image (char *buf)
{
  if (!isatty (fileno (stdout)))
    return;
  fputs ("\r", stdout);
  fputs (buf, stdout);
  fflush (stdout);
}

#ifdef SIGWINCH
static void
progress_handle_sigwinch (int sig)
{
  received_sigwinch = 1;
  signal (SIGWINCH, progress_handle_sigwinch);
}
#endif

struct prg2wav_display_interface cmdline_display_interface =
{
  progressmeter_set_totalbytes,
  progressmeter,
  progressmeter_finish
};

struct display_interface_internal* get_cmdline_display_interface(void)
{
  return malloc(sizeof(struct display_interface_internal));
}

