/* 
 * tkCmds.c --
 *
 *	This file contains a collection of Tk-related Tcl commands
 *	that didn't fit in any particular file of the toolkit.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

static char sccsid[] = "@(#) tkCmds.c 1.100 95/06/24 17:43:37";

#include "tkPort.h"
#include "tkInt.h"
#include <errno.h>

/*
 * Forward declarations for procedures defined later in this file:
 */

static Tk_Window	GetDisplayOf _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, Arg *args));
static char *		WaitVariableProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Var name1, char *name2,
			    int flags));
static void		WaitVisibilityProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		WaitWindowProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));

/*
 *----------------------------------------------------------------------
 *
 * Tk_BellCmd --
 *
 *	This procedure is invoked to process the "bell" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tk_BellCmd(clientData, interp, argc, args)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    size_t length;

    if ((argc != 1) && (argc != 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", LangString(args[0]),
		" ?-displayof window?\"",          NULL);
	return TCL_ERROR;
    }

    if (argc == 3) {
	length = strlen(LangString(args[1]));
	if ((length < 2) || (LangCmpOpt("-displayof", LangString(args[1]), length) != 0)) {
	    Tcl_AppendResult(interp, "bad option \"", LangString(args[1]),
		    "\": must be -displayof",          NULL);
	    return TCL_ERROR;
	}
	tkwin = Tk_NameToWindow(interp, LangString(args[2]), tkwin);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
    }
    XBell(Tk_Display(tkwin), 0);
    XForceScreenSaver(Tk_Display(tkwin), ScreenSaverReset);
    XFlush(Tk_Display(tkwin));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_BindCmd --
 *
 *	This procedure is invoked to process the "bind" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tk_BindCmd(clientData, interp, argc, args)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr;
    ClientData object;

    if ((argc < 2) || (argc > 4)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", LangString(args[0]),
		" window ?pattern? ?command?\"",          NULL);
	return TCL_ERROR;
    }
    if (LangString(args[1])[0] == '.') {
	winPtr = (TkWindow *) Tk_NameToWindow(interp, LangString(args[1]), tkwin);
	if (winPtr == NULL) {
	    return TCL_ERROR;
	}
	object = (ClientData) winPtr->pathName;
    } else {
	winPtr = (TkWindow *) clientData;
	object = (ClientData) Tk_GetUid(LangString(args[1]));
    }

    if (argc == 4) {
	int append = 0;
	unsigned long mask;

	if (LangString(args[3])[0] == 0) {
	    return Tk_DeleteBinding(interp, winPtr->mainPtr->bindingTable,
		    object, LangString(args[2]));
	}
	if (LangString(args[3])[0] == '+') {
	    args[3] = LangStringArg(LangString(args[3])+1);
	    append = 1;
	}
	mask = Tk_CreateBinding(interp, winPtr->mainPtr->bindingTable,
		object, LangString(args[2]), args[3], append);
	if (mask == 0) {
	    return TCL_ERROR;
	}
    } else if (argc == 3) {
	LangCallback *command;

	command = Tk_GetBinding(interp, winPtr->mainPtr->bindingTable,
		object, LangString(args[2]));
	if (command == NULL) {
	    Tcl_ResetResult(interp);
	    return TCL_OK;
	}
	Tcl_ArgResult(interp,LangCallbackArg(command));
    } else {
	Tk_GetAllBindings(interp, winPtr->mainPtr->bindingTable, object);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkBindEventProc --
 *
 *	This procedure is invoked by Tk_HandleEvent for each event;  it
 *	causes any appropriate bindings for that event to be invoked.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what bindings have been established with the "bind"
 *	command.
 *
 *----------------------------------------------------------------------
 */

void
TkBindEventProc(winPtr, eventPtr)
    TkWindow *winPtr;			/* Pointer to info about window. */
    XEvent *eventPtr;			/* Information about event. */
{
#define MAX_OBJS 20
    ClientData objects[MAX_OBJS], *objPtr;
    static Tk_Uid allUid = NULL;
    TkWindow *topLevPtr;
    int i, count;
    char *p;
    Tcl_HashEntry *hPtr;

    if ((winPtr->mainPtr == NULL) || (winPtr->mainPtr->bindingTable == NULL)) {
	return;
    }

    objPtr = objects;
    if (winPtr->numTags != 0) {
	/*
	 * Make a copy of the tags for the window, replacing window names
	 * with pointers to the pathName from the appropriate window.
	 */

	if (winPtr->numTags > MAX_OBJS) {
	    objPtr = (ClientData *) ckalloc((unsigned)
		    (winPtr->numTags * sizeof(ClientData)));
	}
	for (i = 0; i < winPtr->numTags; i++) {
	    p = (char *) winPtr->tagPtr[i];
	    if (*p == '.') {
		hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->nameTable, p);
		if (hPtr != NULL) {
		    p = ((TkWindow *) Tcl_GetHashValue(hPtr))->pathName;
		} else {
		    p = NULL;
		}
	    }
	    objPtr[i] = (ClientData) p;
	}
	count = winPtr->numTags;
    } else {
	objPtr[0] = (ClientData) winPtr->pathName;
	objPtr[1] = (ClientData) winPtr->classUid;
	for (topLevPtr = winPtr;
		(topLevPtr != NULL) && !(topLevPtr->flags & TK_TOP_LEVEL);
		topLevPtr = topLevPtr->parentPtr) {
	    /* Empty loop body. */
	}
	if ((winPtr != topLevPtr) && (topLevPtr != NULL)) {
	    count = 4;
	    objPtr[2] = (ClientData) topLevPtr->pathName;
	} else {
	    count = 3;
	}
	if (allUid == NULL) {
	    allUid = Tk_GetUid("all");
	}
	objPtr[count-1] = (ClientData) allUid;
    }
    Tk_BindEvent(winPtr->mainPtr->bindingTable, eventPtr, (Tk_Window) winPtr,
	    count, objPtr);
    if (objPtr != objects) {
	ckfree((char *) objPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_BindtagsCmd --
 *
 *	This procedure is invoked to process the "bindtags" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tk_BindtagsCmd(clientData, interp, argc, args)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr, *winPtr2;
    int i, tagArgc;
    char *p, **tagArg;
    Arg *tagArgv;
    LangFreeProc *freeProc = NULL;

    if ((argc < 2) || (argc > 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", LangString(args[0]),
		" window ?tags?\"",          NULL);
	return TCL_ERROR;
    }
    winPtr = (TkWindow *) Tk_NameToWindow(interp, LangString(args[1]), tkwin);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
    if (argc == 2) {
	if (winPtr->numTags == 0) {
	    Tcl_AppendElement(interp, winPtr->pathName);
	    Tcl_AppendElement(interp, winPtr->classUid);
	    for (winPtr2 = winPtr;
		    (winPtr2 != NULL) && !(winPtr2->flags & TK_TOP_LEVEL);
		    winPtr2 = winPtr2->parentPtr) {
		/* Empty loop body. */
	    }
	    if ((winPtr != winPtr2) && (winPtr2 != NULL)) {
		Tcl_AppendElement(interp, winPtr2->pathName);
	    }
	    Tcl_AppendElement(interp, "all");
	} else {
	    for (i = 0; i < winPtr->numTags; i++) {
		Tcl_AppendElement(interp, (char *) winPtr->tagPtr[i]);
	    }
	}
	return TCL_OK;
    }
    if (winPtr->tagPtr != NULL) {
	TkFreeBindingTags(winPtr);
    }
    if (LangString(args[2])[0] == 0) {
	return TCL_OK;
    }
    if (Lang_SplitList(interp, args[2], &tagArgc, &tagArgv, &freeProc) != TCL_OK) {
	return TCL_ERROR;
    }
    winPtr->numTags = tagArgc;
    winPtr->tagPtr = (ClientData *) ckalloc((unsigned)
	    (tagArgc * sizeof(ClientData)));
    for (i = 0; i < tagArgc; i++) {
	p = LangString(tagArgv[i]);
	if (p[0] == '.') {
	    char *copy;

	    /*
	     * Handle names starting with "." specially: store a malloc'ed
	     * string, rather than a Uid;  at event time we'll look up the
	     * name in the window table and use the corresponding window,
	     * if there is one.
	     */

	    copy = (char *) ckalloc((unsigned) (strlen(p) + 1));
	    strcpy(copy, p);
	    winPtr->tagPtr[i] = (ClientData) copy;
	} else {
	    winPtr->tagPtr[i] = (ClientData) Tk_GetUid(p);
	}
    }
    if (freeProc)
     (*freeProc)(tagArgc, tagArgv);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFreeBindingTags --
 *
 *	This procedure is called to free all of the binding tags
 *	associated with a window;  typically it is only invoked where
 *	there are window-specific tags.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Any binding tags for winPtr are freed.
 *
 *----------------------------------------------------------------------
 */

void
TkFreeBindingTags(winPtr)
    TkWindow *winPtr;		/* Window whose tags are to be released. */
{
    int i;
    char *p;

    for (i = 0; i < winPtr->numTags; i++) {
	p = (char *) (winPtr->tagPtr[i]);
	if (*p == '.') {
	    /*
	     * Names starting with "." are malloced rather than Uids, so
	     * they have to be freed.
	     */
    
	    ckfree(p);
	}
    }
    ckfree((char *) winPtr->tagPtr);
    winPtr->numTags = 0;
    winPtr->tagPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_DestroyCmd --
 *
 *	This procedure is invoked to process the "destroy" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tk_DestroyCmd(clientData, interp, argc, args)
    ClientData clientData;		/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    Tk_Window window;
    Tk_Window tkwin = (Tk_Window) clientData;
    int i;

    for (i = 1; i < argc; i++) {
	window = Tk_NameToWindow(interp, LangString(args[i]), tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	Tk_DestroyWindow(window);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_ExitCmd --
 *
 *	This procedure is invoked to process the "exit" Tcl command.
 *	See the user documentation for details on what it does.
 *	Note: this command replaces the Tcl "exit" command in order
 *	to properly destroy all windows.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/*ARGSUSED*/
int
Tk_ExitCmd(clientData, interp, argc, args)
    ClientData clientData;		/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    int value;

    if ((argc != 1) && (argc != 2)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", LangString(args[0]),
		" ?returnCode?\"",          NULL);
	return TCL_ERROR;
    }
    if (argc == 1) {
	value = 0;
    } else {
	if (Tcl_GetInt(interp, args[1], &value) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    while (tkMainWindowList != NULL) {
	Tk_DestroyWindow((Tk_Window) tkMainWindowList->winPtr);
    }
    LangExit(value);
    /* NOTREACHED */
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_LowerCmd --
 *
 *	This procedure is invoked to process the "lower" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tk_LowerCmd(clientData, interp, argc, args)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    Tk_Window main = (Tk_Window) clientData;
    Tk_Window tkwin, other;

    if ((argc != 2) && (argc != 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		LangString(args[0]), " window ?belowThis?\"",          NULL);
	return TCL_ERROR;
    }

    tkwin = Tk_NameToWindow(interp, LangString(args[1]), main);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    if (argc == 2) {
	other = NULL;
    } else {
	other = Tk_NameToWindow(interp, LangString(args[2]), main);
	if (other == NULL) {
	    return TCL_ERROR;
	}
    }
    if (Tk_RestackWindow(tkwin, Below, other) != TCL_OK) {
	Tcl_AppendResult(interp, "can't lower \"", LangString(args[1]), "\" below \"",
		LangString(args[2]), "\"",          NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_RaiseCmd --
 *
 *	This procedure is invoked to process the "raise" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tk_RaiseCmd(clientData, interp, argc, args)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    Tk_Window main = (Tk_Window) clientData;
    Tk_Window tkwin, other;

    if ((argc != 2) && (argc != 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		LangString(args[0]), " window ?aboveThis?\"",          NULL);
	return TCL_ERROR;
    }

    tkwin = Tk_NameToWindow(interp, LangString(args[1]), main);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    if (argc == 2) {
	other = NULL;
    } else {
	other = Tk_NameToWindow(interp, LangString(args[2]), main);
	if (other == NULL) {
	    return TCL_ERROR;
	}
    }
    if (Tk_RestackWindow(tkwin, Above, other) != TCL_OK) {
	Tcl_AppendResult(interp, "can't raise \"", LangString(args[1]), "\" above \"",
		LangString(args[2]), "\"",          NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_TkCmd --
 *
 *	This procedure is invoked to process the "tk" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tk_TkCmd(clientData, interp, argc, args)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    char c;
    size_t length;
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		LangString(args[0]), " option ?arg?\"",          NULL);
	return TCL_ERROR;
    }
    c = LangString(args[1])[0];
    length = strlen(LangString(args[1]));
    if ((c == 'a') && (strncmp(LangString(args[1]), "appname", length) == 0)) {
	winPtr = ((TkWindow *) tkwin)->mainPtr->winPtr;
	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", LangString(args[0]),
		    " appname ?newName?\"",          NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    winPtr->nameUid = Tk_GetUid(Tk_SetAppName(tkwin, LangString(args[2])));
	}
	Tcl_SetResult(interp, winPtr->nameUid,TCL_STATIC);
    } else {
	Tcl_AppendResult(interp, "bad option \"", LangString(args[1]),
		"\": must be appname",          NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_TkwaitCmd --
 *
 *	This procedure is invoked to process the "tkwait" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tk_TkwaitCmd(clientData, interp, argc, args)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    int c, done;
    size_t length;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		LangString(args[0]), " variable|visible|window name\"",          NULL);
	return TCL_ERROR;
    }
    c = LangString(args[1])[0];
    length = strlen(LangString(args[1]));
    if ((c == 'v') && (strncmp(LangString(args[1]), "variable", length) == 0)
	    && (length >= 2)) {
        Var variable;
        if (LangSaveVar(interp,args[2],&variable,TK_CONFIG_SCALARVAR) != TCL_OK)
	    return TCL_ERROR;
	if (Tcl_TraceVar(interp, variable,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		WaitVariableProc, (ClientData) &done) != TCL_OK) {
	    return TCL_ERROR;
	}
	done = 0;
	while (!done) {
	    Tk_DoOneEvent(0);
	}
	Tcl_UntraceVar(interp, variable,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		WaitVariableProc, (ClientData) &done);
        LangFreeVar(variable);
    } else if ((c == 'v') && (strncmp(LangString(args[1]), "visibility", length) == 0)
	    && (length >= 2)) {
	Tk_Window window;

	window = Tk_NameToWindow(interp, LangString(args[2]), tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	Tk_CreateEventHandler(window, VisibilityChangeMask,
	    WaitVisibilityProc, (ClientData) &done);
	done = 0;
	while (!done) {
	    Tk_DoOneEvent(0);
	}
	Tk_DeleteEventHandler(window, VisibilityChangeMask,
	    WaitVisibilityProc, (ClientData) &done);
    } else if ((c == 'w') && (strncmp(LangString(args[1]), "window", length) == 0)) {
	Tk_Window window;

	window = Tk_NameToWindow(interp, LangString(args[2]), tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	Tk_CreateEventHandler(window, StructureNotifyMask,
	    WaitWindowProc, (ClientData) &done);
	done = 0;
	while (!done) {
	    Tk_DoOneEvent(0);
	}
	/*
	 * Note:  there's no need to delete the event handler.  It was
	 * deleted automatically when the window was destroyed.
	 */
    } else {
	Tcl_AppendResult(interp, "bad option \"", LangString(args[1]),
		"\": must be variable, visibility, or window",          NULL);
	return TCL_ERROR;
    }

    /*
     * Clear out the interpreter's result, since it may have been set
     * by event handlers.
     */

    Tcl_ResetResult(interp);
    return TCL_OK;
}

	/* ARGSUSED */
static char *
WaitVariableProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    Var name1;			/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    int *donePtr = (int *) clientData;

    *donePtr = 1;
    return          NULL;
}

	/*ARGSUSED*/
static void
WaitVisibilityProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    XEvent *eventPtr;		/* Information about event (not used). */
{
    int *donePtr = (int *) clientData;
    *donePtr = 1;
}

static void
WaitWindowProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    XEvent *eventPtr;		/* Information about event. */
{
    int *donePtr = (int *) clientData;

    if (eventPtr->type == DestroyNotify) {
	*donePtr = 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_UpdateCmd --
 *
 *	This procedure is invoked to process the "update" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tk_UpdateCmd(clientData, interp, argc, args)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    int flags;
    Display *display;

    if (argc == 1) {
	flags = TK_DONT_WAIT;
    } else if (argc == 2) {
	if (strncmp(LangString(args[1]), "idletasks", strlen(LangString(args[1]))) != 0) {
	    Tcl_AppendResult(interp, "bad argument \"", LangString(args[1]),
		    "\": must be idletasks",          NULL);
	    return TCL_ERROR;
	}
	flags = TK_IDLE_EVENTS;
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		LangString(args[0]), " ?idletasks?\"",          NULL);
	return TCL_ERROR;
    }

    /*
     * Handle all pending events, sync the display, and repeat over
     * and over again until all pending events have been handled.
     * Special note:  it's possible that the entire application could
     * be destroyed by an event handler that occurs during the update.
     * Thus, don't use any information from tkwin after calling
     * Tk_DoOneEvent.
     */

    display = Tk_Display(tkwin);
    while (1) {
	while (Tk_DoOneEvent(flags) != 0) {
	    /* Empty loop body */
	}
	XSync(display, False);
	if (Tk_DoOneEvent(flags) == 0) {
	    break;
	}
    }

    /*
     * Must clear the interpreter's result because event handlers could
     * have executed commands.
     */

    Tcl_ResetResult(interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_WinfoCmd --
 *
 *	This procedure is invoked to process the "winfo" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tk_WinfoCmd(clientData, interp, argc, args)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    size_t length;
    char c, *argName;
    Tk_Window window;
    register TkWindow *winPtr;

#define SETUP(name) \
    if (argc != 3) {\
	argName = name; \
	goto wrongArgs; \
    } \
    window = Tk_NameToWindow(interp, LangString(args[2]), tkwin); \
    if (window == NULL) { \
	return TCL_ERROR; \
    }

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		LangString(args[0]), " option ?arg?\"",          NULL);
	return TCL_ERROR;
    }
    c = LangString(args[1])[0];
    length = strlen(LangString(args[1]));
    if ((c == 'a') && (strcmp(LangString(args[1]), "atom") == 0)) {
	char *atomName;

	if (argc == 3) {
	    atomName = LangString(args[2]);
	} else if (argc == 5) {
	    atomName = LangString(args[4]);
	    tkwin = GetDisplayOf(interp, tkwin, args+2);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    LangString(args[0]), " atom ?-displayof window? name\"",
		             NULL);
	    return TCL_ERROR;
	}
	Tcl_SprintfResult(interp, "%ld", Tk_InternAtom(tkwin, atomName));
    } else if ((c == 'a') && (strncmp(LangString(args[1]), "atomname", length) == 0)
	    && (length >= 5)) {
	Atom atom;
	char *name;
	Arg id;

	if (argc == 3) {
	    id = args[2];
	} else if (argc == 5) {
	    id = args[4];
	    tkwin = GetDisplayOf(interp, tkwin, args+2);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    LangString(args[0]), " atomname ?-displayof window? id\"",
		             NULL);
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, id, (int *) &atom) != TCL_OK) {
	    return TCL_ERROR;
	}
	name = Tk_GetAtomName(tkwin, atom);
	if (strcmp(name, "?bad atom?") == 0) {
	    Tcl_AppendResult(interp, "no atom exists with id \"",
		    LangString(args[2]), "\"",          NULL);
	    return TCL_ERROR;
	}
	Tcl_SetResult(interp, name,TCL_STATIC);
    } else if ((c == 'c') && (strncmp(LangString(args[1]), "cells", length) == 0)
	    && (length >= 2)) {
	SETUP("cells");
	Tcl_IntResults(interp,1,0, Tk_Visual(window)->map_entries);
    } else if ((c == 'c') && (strncmp(LangString(args[1]), "children", length) == 0)
	    && (length >= 2)) {
	SETUP("children");
	for (winPtr = ((TkWindow *) window)->childList; winPtr != NULL;
		winPtr = winPtr->nextPtr) {
	    Tcl_AppendArg(interp, LangWidgetArg(interp, (Tk_Window) winPtr));
	}
    } else if ((c == 'c') && (strncmp(LangString(args[1]), "class", length) == 0)
	    && (length >= 2)) {
	SETUP("class");
	Tcl_SetResult(interp, Tk_Class(window),TCL_STATIC);
    } else if ((c == 'c') && (strncmp(LangString(args[1]), "colormapfull", length) == 0)
	    && (length >= 3)) {
	SETUP("colormapfull");
	Tcl_SetResult(interp, (TkCmapStressed(window, Tk_Colormap(window))) ? "1" : "0",TCL_STATIC);

    } else if ((c == 'c') && (strncmp(LangString(args[1]), "containing", length) == 0)
	    && (length >= 3)) {
	int rootX, rootY, index;

	if (argc == 4) {
	    index = 2;
	} else if (argc == 6) {
	    index = 4;
	    tkwin = GetDisplayOf(interp, tkwin, args+2);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    LangString(args[0]), " containing ?-displayof window? rootX rootY\"",
		             NULL);
	    return TCL_ERROR;
	}
	if ((Tk_GetPixels(interp, tkwin, LangString(args[index]), &rootX) != TCL_OK)
		|| (Tk_GetPixels(interp, tkwin, LangString(args[index+1]), &rootY)
		!= TCL_OK)) {
	    return TCL_ERROR;
	}
	window = Tk_CoordsToWindow(rootX, rootY, tkwin);
	if (window != NULL) {
	    Tcl_ArgResult(interp,LangWidgetArg(interp,window));
	}
    } else if ((c == 'd') && (strncmp(LangString(args[1]), "depth", length) == 0)) {
	SETUP("depth");
	Tcl_IntResults(interp,1,0, Tk_Depth(window));
    } else if ((c == 'e') && (strncmp(LangString(args[1]), "exists", length) == 0)) {
	if (argc != 3) {
	    argName = "exists";
	    goto wrongArgs;
	}
	window = Tk_NameToWindow(interp, LangString(args[2]), tkwin);
	if ((window == NULL)
		|| (((TkWindow *) window)->flags & TK_ALREADY_DEAD)) {
	    Tcl_SetResult(interp, "0",TCL_STATIC);
	} else {
	    Tcl_SetResult(interp, "1",TCL_STATIC);
	}
    } else if ((c == 'f') && (strncmp(LangString(args[1]), "fpixels", length) == 0)
	    && (length >= 2)) {
	double mm, pixels;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    LangString(args[0]), " fpixels window number\"",          NULL);
	    return TCL_ERROR;
	}
	window = Tk_NameToWindow(interp, LangString(args[2]), tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	if (Tk_GetScreenMM(interp, window, LangString(args[3]), &mm) != TCL_OK) {
	    return TCL_ERROR;
	}
	pixels = mm * WidthOfScreen(Tk_Screen(window))
		/ WidthMMOfScreen(Tk_Screen(window));
	Tcl_DoubleResults(interp, 1, 0, pixels);
    } else if ((c == 'g') && (strncmp(LangString(args[1]), "geometry", length) == 0)) {
	SETUP("geometry");
	Tcl_SprintfResult(interp, "%dx%d+%d+%d", Tk_Width(window),
		Tk_Height(window), Tk_X(window), Tk_Y(window));
    } else if ((c == 'h') && (strncmp(LangString(args[1]), "height", length) == 0)) {
	SETUP("height");
	Tcl_IntResults(interp,1,0, Tk_Height(window));
    } else if ((c == 'i') && (strcmp(LangString(args[1]), "id") == 0)) {
	SETUP("id");
	Tk_MakeWindowExist(window);
	Tcl_SprintfResult(interp, "0x%x", (unsigned int) Tk_WindowId(window));
    } else if ((c == 'i') && (strncmp(LangString(args[1]), "interps", length) == 0)
	    && (length >= 2)) {
	if (argc == 4) {
	    tkwin = GetDisplayOf(interp, tkwin, args+2);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	} else if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    LangString(args[0]), " interps ?-displayof window?\"",
		             NULL);
	    return TCL_ERROR;
	}
	return TkGetInterpNames(interp, tkwin);
    } else if ((c == 'i') && (strncmp(LangString(args[1]), "ismapped", length) == 0)
	    && (length >= 2)) {
	SETUP("ismapped");
	Tcl_SetResult(interp, Tk_IsMapped(window) ? "1" : "0",TCL_STATIC);
    } else if ((c == 'm') && (strncmp(LangString(args[1]), "manager", length) == 0)) {
	SETUP("manager");
	winPtr = (TkWindow *) window;
	if (winPtr->geomMgrPtr != NULL) {
	    Tcl_SetResult(interp, winPtr->geomMgrPtr->name,TCL_STATIC);
	}
    } else if ((c == 'n') && (strncmp(LangString(args[1]), "name", length) == 0)) {
	SETUP("name");
	Tcl_SetResult(interp, Tk_Name(window),TCL_STATIC);
    } else if ((c == 'p') && (strncmp(LangString(args[1]), "parent", length) == 0)) {
	SETUP("parent");
	winPtr = (TkWindow *) window;
	if (winPtr->parentPtr != NULL) {
	    Tcl_ArgResult(interp,LangWidgetArg(interp,(Tk_Window)(winPtr->parentPtr)));
	}
    } else if ((c == 'p') && (strncmp(LangString(args[1]), "pathname", length) == 0)
	    && (length >= 2)) {
	int index, id;

	if (argc == 3) {
	    index = 2;
	} else if (argc == 5) {
	    index = 4;
	    tkwin = GetDisplayOf(interp, tkwin, args+2);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    LangString(args[0]), " pathname ?-displayof window? id\"",
		             NULL);
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, args[index], &id) != TCL_OK) {
	    return TCL_ERROR;
	}
	window = Tk_IdToWindow(Tk_Display(tkwin), (Window) id);
	if ((window == NULL) || (((TkWindow *) window)->mainPtr
		!= ((TkWindow *) tkwin)->mainPtr)) {
	    Tcl_AppendResult(interp, "window id \"", LangString(args[index]),
		    "\" doesn't exist in this application",          NULL);
	    return TCL_ERROR;
	}
	Tcl_ArgResult(interp,LangWidgetArg(interp,window));
    } else if ((c == 'p') && (strncmp(LangString(args[1]), "pixels", length) == 0)
	    && (length >= 2)) {
	int pixels;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    LangString(args[0]), " pixels window number\"",          NULL);
	    return TCL_ERROR;
	}
	window = Tk_NameToWindow(interp, LangString(args[2]), tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	if (Tk_GetPixels(interp, window, LangString(args[3]), &pixels) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_IntResults(interp,1,0, pixels);
    } else if ((c == 'p') && (strcmp(LangString(args[1]), "pointerx") == 0)) {
	int x, y;

	SETUP("pointerx");
	TkGetPointerCoords(window, &x, &y);
	Tcl_IntResults(interp,1,0, x);
    } else if ((c == 'p') && (strcmp(LangString(args[1]), "pointerxy") == 0)) {
	int x, y;

	SETUP("pointerxy");
	TkGetPointerCoords(window, &x, &y);
	Tcl_IntResults(interp,2,0, x, y);
    } else if ((c == 'p') && (strcmp(LangString(args[1]), "pointery") == 0)) {
	int x, y;

	SETUP("pointery");
	TkGetPointerCoords(window, &x, &y);
	Tcl_IntResults(interp,1,0, y);
    } else if ((c == 'r') && (strncmp(LangString(args[1]), "reqheight", length) == 0)
	    && (length >= 4)) {
	SETUP("reqheight");
	Tcl_IntResults(interp,1,0, Tk_ReqHeight(window));
    } else if ((c == 'r') && (strncmp(LangString(args[1]), "reqwidth", length) == 0)
	    && (length >= 4)) {
	SETUP("reqwidth");
	Tcl_IntResults(interp,1,0, Tk_ReqWidth(window));
    } else if ((c == 'r') && (strncmp(LangString(args[1]), "rgb", length) == 0)
	    && (length >= 2)) {
	XColor *colorPtr;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    LangString(args[0]), " rgb window colorName\"",          NULL);
	    return TCL_ERROR;
	}
	window = Tk_NameToWindow(interp, LangString(args[2]), tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	colorPtr = Tk_GetColor(interp, window, LangString(args[3]));
	if (colorPtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_IntResults(interp,3,0, colorPtr->red, colorPtr->green,
		colorPtr->blue);
	Tk_FreeColor(colorPtr);
    } else if ((c == 'r') && (strcmp(LangString(args[1]), "rootx") == 0)) {
	int x, y;

	SETUP("rootx");
	Tk_GetRootCoords(window, &x, &y);
	Tcl_IntResults(interp,1,0, x);
    } else if ((c == 'r') && (strcmp(LangString(args[1]), "rooty") == 0)) {
	int x, y;

	SETUP("rooty");
	Tk_GetRootCoords(window, &x, &y);
	Tcl_IntResults(interp,1,0, y);
    } else if ((c == 's') && (strcmp(LangString(args[1]), "screen") == 0)) {
	char string[20];

	SETUP("screen");
	sprintf(string, "%d", Tk_ScreenNumber(window));
	Tcl_AppendResult(interp, Tk_DisplayName(window), ".", string,
		         NULL);
    } else if ((c == 's') && (strncmp(LangString(args[1]), "screencells", length) == 0)
	    && (length >= 7)) {
	SETUP("screencells");
	Tcl_IntResults(interp,1,0, CellsOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(LangString(args[1]), "screendepth", length) == 0)
	    && (length >= 7)) {
	SETUP("screendepth");
	Tcl_IntResults(interp,1,0, DefaultDepthOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(LangString(args[1]), "screenheight", length) == 0)
	    && (length >= 7)) {
	SETUP("screenheight");
	Tcl_IntResults(interp,1,0,  HeightOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(LangString(args[1]), "screenmmheight", length) == 0)
	    && (length >= 9)) {
	SETUP("screenmmheight");
	Tcl_IntResults(interp,1,0,  HeightMMOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(LangString(args[1]), "screenmmwidth", length) == 0)
	    && (length >= 9)) {
	SETUP("screenmmwidth");
	Tcl_IntResults(interp,1,0,  WidthMMOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(LangString(args[1]), "screenvisual", length) == 0)
	    && (length >= 7)) {
	SETUP("screenvisual");
	switch (DefaultVisualOfScreen(Tk_Screen(window))->class) {
	    case PseudoColor:	Tcl_SetResult(interp, "pseudocolor",TCL_STATIC); break;
	    case GrayScale:	Tcl_SetResult(interp, "grayscale",TCL_STATIC); break;
	    case DirectColor:	Tcl_SetResult(interp, "directcolor",TCL_STATIC); break;
	    case TrueColor:	Tcl_SetResult(interp, "truecolor",TCL_STATIC); break;
	    case StaticColor:	Tcl_SetResult(interp, "staticcolor",TCL_STATIC); break;
	    case StaticGray:	Tcl_SetResult(interp, "staticgray",TCL_STATIC); break;
	    default:		Tcl_SetResult(interp, "unknown",TCL_STATIC); break;
	}
    } else if ((c == 's') && (strncmp(LangString(args[1]), "screenwidth", length) == 0)
	    && (length >= 7)) {
	SETUP("screenwidth");
	Tcl_IntResults(interp,1,0,  WidthOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(LangString(args[1]), "server", length) == 0)
	    && (length >= 2)) {
	SETUP("server");
	TkGetServerInfo(interp, window);
    } else if ((c == 't') && (strncmp(LangString(args[1]), "toplevel", length) == 0)) {
	SETUP("toplevel");
	for (winPtr = (TkWindow *) window; winPtr != NULL;
		winPtr = winPtr->parentPtr) {
	    if (winPtr->flags & TK_TOP_LEVEL) {
		Tcl_ArgResult(interp,LangWidgetArg(interp,(Tk_Window)(winPtr)));
		break;
	    }
	}
    } else if ((c == 'v') && (strncmp(LangString(args[1]), "viewable", length) == 0)
	    && (length >= 3)) {
	SETUP("viewable");
	for (winPtr = (TkWindow *) window; ; winPtr = winPtr->parentPtr) {
	    if ((winPtr == NULL) || !(winPtr->flags & TK_MAPPED)) {
		Tcl_SetResult(interp, "0",TCL_STATIC);
		break;
	    }
	    if (winPtr->flags & TK_TOP_LEVEL) {
		Tcl_SetResult(interp, "1",TCL_STATIC);
		break;
	    }
	}
    } else if ((c == 'v') && (strcmp(LangString(args[1]), "visual") == 0)) {
	SETUP("visual");
	switch (Tk_Visual(window)->class) {
	    case PseudoColor:	Tcl_SetResult(interp, "pseudocolor",TCL_STATIC); break;
	    case GrayScale:	Tcl_SetResult(interp, "grayscale",TCL_STATIC); break;
	    case DirectColor:	Tcl_SetResult(interp, "directcolor",TCL_STATIC); break;
	    case TrueColor:	Tcl_SetResult(interp, "truecolor",TCL_STATIC); break;
	    case StaticColor:	Tcl_SetResult(interp, "staticcolor",TCL_STATIC); break;
	    case StaticGray:	Tcl_SetResult(interp, "staticgray",TCL_STATIC); break;
	    default:		Tcl_SetResult(interp, "unknown",TCL_STATIC); break;
	}
    } else if ((c == 'v') && (strncmp(LangString(args[1]), "visualsavailable", length) == 0)
	    && (length >= 7)) {
	XVisualInfo template, *visInfoPtr;
	int count, i;
	char string[50], *fmt;

	SETUP("visualsavailable");
	template.screen = Tk_ScreenNumber(window);
	visInfoPtr = XGetVisualInfo(Tk_Display(window), VisualScreenMask,
		&template, &count);
	if (visInfoPtr == NULL) {
	    Tcl_SetResult(interp, "can't find any visuals for screen",TCL_STATIC);
	    return TCL_ERROR;
	}
	for (i = 0; i < count; i++) {
	    switch (visInfoPtr[i].class) {
		case PseudoColor:	fmt = "pseudocolor %d"; break;
		case GrayScale:		fmt = "grayscale %d"; break;
		case DirectColor:	fmt = "directcolor %d"; break;
		case TrueColor:		fmt = "truecolor %d"; break;
		case StaticColor:	fmt = "staticcolor %d"; break;
		case StaticGray:	fmt = "staticgray %d"; break;
		default:		fmt = "unknown"; break;
	    }
	    sprintf(string, fmt, visInfoPtr[i].depth);
	    Tcl_AppendElement(interp, string);
	}
	XFree((char *) visInfoPtr);
    } else if ((c == 'v') && (strncmp(LangString(args[1]), "vrootheight", length) == 0)
	    && (length >= 6)) {
	int x, y;
	int width, height;

	SETUP("vrootheight");
	Tk_GetVRootGeometry(window, &x, &y, &width, &height);
	Tcl_IntResults(interp,1,0, height);
    } else if ((c == 'v') && (strncmp(LangString(args[1]), "vrootwidth", length) == 0)
	    && (length >= 6)) {
	int x, y;
	int width, height;

	SETUP("vrootwidth");
	Tk_GetVRootGeometry(window, &x, &y, &width, &height);
	Tcl_IntResults(interp,1,0, width);
    } else if ((c == 'v') && (strcmp(LangString(args[1]), "vrootx") == 0)) {
	int x, y;
	int width, height;

	SETUP("vrootx");
	Tk_GetVRootGeometry(window, &x, &y, &width, &height);
	Tcl_IntResults(interp,1,0, x);
    } else if ((c == 'v') && (strcmp(LangString(args[1]), "vrooty") == 0)) {
	int x, y;
	int width, height;

	SETUP("vrooty");
	Tk_GetVRootGeometry(window, &x, &y, &width, &height);
	Tcl_IntResults(interp,1,0, y);
    } else if ((c == 'w') && (strncmp(LangString(args[1]), "width", length) == 0)) {
	SETUP("width");
	Tcl_IntResults(interp,1,0, Tk_Width(window));
    } else if ((c == 'x') && (LangString(args[1])[1] == '\0')) {
	SETUP("x");
	Tcl_IntResults(interp,1,0, Tk_X(window));
    } else if ((c == 'y') && (LangString(args[1])[1] == '\0')) {
	SETUP("y");
	Tcl_IntResults(interp,1,0, Tk_Y(window));
    } else {
	Tcl_AppendResult(interp, "bad option \"", LangString(args[1]),
		"\": must be atom, atomname, cells, children, ",
		"class, colormapfull, containing, depth, exists, fpixels, ",
		"geometry, height, ",
		"id, interps, ismapped, manager, name, parent, pathname, ",
		"pixels, pointerx, pointerxy, pointery, reqheight, ",
		"reqwidth, rgb, ",
		"rootx, rooty, ",
		"screen, screencells, screendepth, screenheight, ",
		"screenmmheight, screenmmwidth, screenvisual, ",
		"screenwidth, server, ",
		"toplevel, viewable, visual, visualsavailable, ",
		"vrootheight, vrootwidth, vrootx, vrooty, ",
		"width, x, or y",          NULL);
	return TCL_ERROR;
    }
    return TCL_OK;

    wrongArgs:
    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
	    LangString(args[0]), " ", argName, " window\"",          NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GetDisplayOf --
 *
 *	Parses a "-displayof" option for the "winfo" command.
 *
 * Results:
 *	The return value is a token for the window specified in
 *	LangString(args[1]).  If LangString(args[0]) and LangString(args[1]) couldn't be parsed, NULL
 *	is returned and an error is left in Tcl_GetResult(interp).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tk_Window
GetDisplayOf(interp, tkwin, args)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    Tk_Window tkwin;		/* Window to use for looking up window
				 * given in LangString(args[1]). */
    Arg *args;		/* Array of two strings.   First must be
				 * "-displayof" or an abbreviation, second
				 * must be window name. */
{
    size_t length;

    length = strlen(LangString(args[0]));
    if ((length < 2) || (LangCmpOpt("-displayof", LangString(args[0]), length) != 0)) {
	Tcl_AppendResult(interp, "bad argument \"", LangString(args[0]),
		"\": must be -displayof",          NULL);
	return (Tk_Window) NULL;
    }
    return Tk_NameToWindow(interp, LangString(args[1]), tkwin);
}

/*
 *----------------------------------------------------------------------
 *
 * TkDeadAppCmd --
 *
 *	If an application has been deleted then all Tk commands will be
 *	re-bound to this procedure.
 *
 * Results:
 *	A standard Tcl error is reported to let the user know that
 *	the application is dead.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
TkDeadAppCmd(clientData, interp, argc, args)
    ClientData clientData;	/* Dummy. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    Arg *args;		/* Argument strings. */
{
    Tcl_AppendResult(interp, "can't invoke \"", LangString(args[0]),
	    "\" command:  application has been destroyed",          NULL);
    return TCL_ERROR;
}
