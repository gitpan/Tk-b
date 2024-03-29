/*
 * patchlevel.h --
 *
 * This file does nothing except define a "patch level" for Tk.
 * The patch level has the form "X.YpZ" where X.Y is the base
 * release, and Z is a serial number that is used to sequence
 * patches for a given release.  Thus 4.0p1 is the first patch
 * to release 4.0, 4.0p2 is the patch that follows 4.0p1, and
 * so on.  The "pZ" is omitted in an original new release, and
 * it is replaced with "bZ" for beta releases (e.g. 4.0b1 is the
 * first beta release of Tk 4.0).  The patch level ensures that
 * patches are applied in the correct order and only to appropriate
 * sources.
 *
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) patchlevel.h 1.10 95/11/27 09:03:19
 */

#define TK_PATCH_LEVEL "4.0p3"
