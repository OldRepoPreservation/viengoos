/* output-vga.c - A VGA output driver.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/io.h>

#include "output.h"

#define VGA_VIDEO_MEM_BASE_ADDR	0x0B8000
#define VGA_VIDEO_MEM_LENGTH	0x004000

/* The default attribute is light grey on black.  */
#define VGA_DEF_ATTRIBUTE	7

#define VGA_COLUMNS	       	80
#define VGA_ROWS	       	25

/* The CRTC Registers.  XXX Depends on the I/O Address Select field.
   However, the only need to use the other values is for compatibility
   with monochrome adapters.  */
#define VGA_CRT_ADDR_REG	0x3d4
#define VGA_CRT_DATA_REG	0x3d5

/* The cursor position subregisters.  */
#define VGA_CRT_CURSOR_HIGH	0x0e
#define VGA_CRT_CURSOR_LOW	0x0f


/* Set the cursor position to POS, which is (x_pos + y_pos * width).  */
static void
vga_set_cursor_pos (unsigned int pos)
{
  outb (VGA_CRT_CURSOR_HIGH, VGA_CRT_ADDR_REG);
  outb (pos >> 8, VGA_CRT_DATA_REG);
  outb (VGA_CRT_CURSOR_LOW, VGA_CRT_ADDR_REG);
  outb (pos & 0xff, VGA_CRT_DATA_REG);
}


/* Get the cursor position, which is (x_pos + y_pos * width).  */
static unsigned int
vga_get_cursor_pos (void)
{
  unsigned int pos;

  outb (VGA_CRT_CURSOR_HIGH, VGA_CRT_ADDR_REG);
  pos = inb (VGA_CRT_DATA_REG) << 8;
  outb (VGA_CRT_CURSOR_LOW, VGA_CRT_ADDR_REG);
  pos |= inb (VGA_CRT_DATA_REG) & 0xff;

  return pos;
}


/* Global variables.  */

static int col;
static int row;
static char *video;


static void
vga_init (struct output_driver *device, const char *cfg)
{
  unsigned int pos = vga_get_cursor_pos ();
  col = pos % VGA_COLUMNS;
  row = pos / VGA_COLUMNS;

  /* FIXME: We are faulting in the video memory here.  We must have a
     way to give it back to the system eventually, for example to the
     physical memory server.  */
  video = (char *) VGA_VIDEO_MEM_BASE_ADDR;
}


static void
vga_putchar (struct output_driver *device, int chr)
{
  unsigned int pos;

  if (chr == '\n')
    {
      col = 0;
      row++;
    }
  else
    {
      pos = row * VGA_COLUMNS + col;
      video[2 * pos] = chr & 0xff;
      video[2 * pos + 1] = VGA_DEF_ATTRIBUTE;
      col++;
      if (col == VGA_COLUMNS)
	{
	  col = 0;
	  row++;
	}
    }

  if (row == VGA_ROWS)
    {
      int i;

      row--;
      for (i = 0; i < VGA_COLUMNS * row; i++)
	{
	  video[2 * i] = video[2 * (i + VGA_COLUMNS)];
	  video[2 * i + 1] = video[2 * (i + VGA_COLUMNS) + 1];
	}
      for (i = VGA_COLUMNS * row; i < VGA_COLUMNS * VGA_ROWS; i++)
	{
	  video[2 * i] = ' ';
	  video[2 * i + 1] = VGA_DEF_ATTRIBUTE;
	}
    }

  pos = row * VGA_COLUMNS + col;
  vga_set_cursor_pos (pos);
}


struct output_driver vga_output =
  {
    "vga",
    vga_init,
    0,		/* deinit */
    vga_putchar
  };
