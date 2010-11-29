/*
 * Pextlib.c
 * $Id: Pextlib.c 54693 2009-07-31 20:04:53Z toby@macports.org $
 *
 * Copyright (c) 2002 - 2003 Apple Computer, Inc.
 * Copyright (c) 2004 - 2005 Paul Guyot <pguyot@kallisys.net>
 * Copyright (c) 2004 Landon Fuller <landonf@macports.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Apple Computer, Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_STRINGS_H
#include <strings.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#include <pwd.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <tcl.h>

#include "Pextlib.h"

#include "md5cmd.h"
#include "sha1cmd.h"
#include "rmd160cmd.h"
#include "fs-traverse.h"
#include "filemap.h"
#include "curl.h"
#include "xinstall.h"
#include "vercomp.h"
#include "readline.h"
#include "uid.h"
#include "tracelib.h"
#include "tty.h"
#include "strsed.h"
#include "readdir.h"
#include "pipe.h"
#include "flock.h"
#include "system.h"

#if HAVE_CRT_EXTERNS_H
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char **environ;
#endif

#if !HAVE_BZERO
#if HAVE_MEMSET
#define bzero(b, len) (void)memset(b, 0x00, len)
#endif
#endif

#if !HAVE_FGETLN
char *fgetln(FILE *stream, size_t *len);
#endif

static char *
ui_escape(const char *source)
{
	char *d, *dest;
	const char *s;
	int slen, dlen;

	s = source;
	slen = dlen = strlen(source) * 2 + 1;
	d = dest = malloc(dlen);
	if (dest == NULL) {
		return NULL;
	}
	while(*s != '\0') {
		switch(*s) {
			case '\\':
			case '}':
			case '{':
				*d = '\\';
				d++;
				*d = *s;
				d++;
				s++;
				break;
			case '\n':
				s++;
				break;
			default:
				*d = *s;
				d++;
				s++;
				break;
		}
	}
	*d = '\0';
	return dest;
}

int
ui_info(Tcl_Interp *interp, char *mesg)
{
	const char ui_proc_start[] = "ui_info [subst -nocommands -novariables {";
	const char ui_proc_end[] = "}]";
	char *script, *string, *p;
	int scriptlen, len, rval;

	string = ui_escape(mesg);
	if (string == NULL)
		return TCL_ERROR;

	len = strlen(string);
	scriptlen = sizeof(ui_proc_start) + len + sizeof(ui_proc_end) - 1;
	script = malloc(scriptlen);
	if (script == NULL)
		return TCL_ERROR;
	else
		p = script;

	memcpy(script, ui_proc_start, sizeof(ui_proc_start));
	strcat(script, string);
	strcat(script, ui_proc_end);
	free(string);
	rval = Tcl_EvalEx(interp, script, scriptlen - 1, 0);
	free(script);
	return rval;
}

int StrsedCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	char *pattern, *string, *res;
	int range[2];
	Tcl_Obj *tcl_result;

	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "string pattern");
		return TCL_ERROR;
	}

	string = Tcl_GetString(objv[1]);
	pattern = Tcl_GetString(objv[2]);
	res = strsed(string, pattern, range);
	if (!res) {
		Tcl_SetResult(interp, "strsed failed", TCL_STATIC);
		return TCL_ERROR;
	}
	tcl_result = Tcl_NewStringObj(res, -1);
	Tcl_SetObjResult(interp, tcl_result);
	free(res);
	return TCL_OK;
}

int MkdtempCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	char *template, *sp;
	Tcl_Obj *tcl_result;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "template");
		return TCL_ERROR;
	}

	template = strdup(Tcl_GetString(objv[1]));
	if (template == NULL)
		return TCL_ERROR;

	if ((sp = mkdtemp(template)) == NULL) {
		Tcl_AppendResult(interp, "mkdtemp failed: ", strerror(errno), NULL);
		free(template);
		return TCL_ERROR;
	}

	tcl_result = Tcl_NewStringObj(sp, -1);
	Tcl_SetObjResult(interp, tcl_result);
	free(template);
	return TCL_OK;
}

int MktempCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	char *template, *sp;
	Tcl_Obj *tcl_result;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "template");
		return TCL_ERROR;
	}

	template = strdup(Tcl_GetString(objv[1]));
	if (template == NULL)
		return TCL_ERROR;

	if ((sp = mktemp(template)) == NULL) {
		Tcl_AppendResult(interp, "mktemp failed: ", strerror(errno), NULL);
		free(template);
		return TCL_ERROR;
	}

	tcl_result = Tcl_NewStringObj(sp, -1);
	Tcl_SetObjResult(interp, tcl_result);
	free(template);
	return TCL_OK;
}

int MkstempCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Channel channel;
	char *template, *channelname;
	int fd;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "template");
		return TCL_ERROR;
	}

	template = strdup(Tcl_GetString(objv[1]));
	if (template == NULL)
		return TCL_ERROR;

	if ((fd = mkstemp(template)) < 0) {
		Tcl_AppendResult(interp, "mkstemp failed: ", strerror(errno), NULL);
		free(template);
		return TCL_ERROR;
	}

	channel = Tcl_MakeFileChannel((ClientData)(intptr_t)fd, TCL_READABLE|TCL_WRITABLE);
	Tcl_RegisterChannel(interp, channel);
	channelname = (char *)Tcl_GetChannelName(channel);
	Tcl_AppendResult(interp, channelname, " ", template, NULL);
	free(template);
	return TCL_OK;
}

int ExistsuserCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *tcl_result;
	struct passwd *pwent;
	char *user;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "user");
		return TCL_ERROR;
	}

	user = strdup(Tcl_GetString(objv[1]));
	if (isdigit(*(user)))
		pwent = getpwuid(strtol(user, 0, 0));
	else
		pwent = getpwnam(user);
	free(user);

	if (pwent == NULL)
		tcl_result = Tcl_NewIntObj(0);
	else
		tcl_result = Tcl_NewIntObj(pwent->pw_uid);

	Tcl_SetObjResult(interp, tcl_result);
	return TCL_OK;
}

int ExistsgroupCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *tcl_result;
	struct group *grent;
	char *group;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "groupname");
		return TCL_ERROR;
	}

	group = strdup(Tcl_GetString(objv[1]));
	if (isdigit(*(group)))
		grent = getgrgid(strtol(group, 0, 0));
	else
		grent = getgrnam(group);
	free(group);

	if (grent == NULL)
		tcl_result = Tcl_NewIntObj(0);
	else
		tcl_result = Tcl_NewIntObj(grent->gr_gid);

	Tcl_SetObjResult(interp, tcl_result);
	return TCL_OK;
}

/* Find the first unused UID > 100
   previously this would find the highest used UID and add 1
   but UIDs > 500 are visible on the login screen of OS X */
int NextuidCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc UNUSED, Tcl_Obj *CONST objv[] UNUSED)
{
	Tcl_Obj *tcl_result;
	int cur;

	cur = MIN_USABLE_UID;

	while (getpwuid(cur) != NULL) {
		cur++;
	}

	tcl_result = Tcl_NewIntObj(cur);
	Tcl_SetObjResult(interp, tcl_result);
	return TCL_OK;
}

/* Just as with NextuidCmd, return the first unused gid > 100
   groups aren't visible on the login screen, but I see no reason
   to create group 502 when I can create group 100 */
int NextgidCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc UNUSED, Tcl_Obj *CONST objv[] UNUSED)
{
	Tcl_Obj *tcl_result;
	int cur;

	cur = MIN_USABLE_GID;

	while (getgrgid(cur) != NULL) {
		cur++;
	}

	tcl_result = Tcl_NewIntObj(cur);
	Tcl_SetObjResult(interp, tcl_result);
	return TCL_OK;
}

int UmaskCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc UNUSED, Tcl_Obj *CONST objv[] UNUSED)
{
	Tcl_Obj *tcl_result;
	char *tcl_mask, *p;
	const size_t stringlen = 4; /* 3 digits & \0 */
	int i;
	mode_t *set;
	mode_t newmode;
	mode_t oldmode;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "mode");
		return TCL_ERROR;
	}

	tcl_mask = Tcl_GetString(objv[1]);
	if ((set = setmode(tcl_mask)) == NULL) {
		Tcl_SetResult(interp, "Invalid umask mode", TCL_STATIC);
		return TCL_ERROR;
	}

	newmode = getmode(set, 0);
	free(set);

	oldmode = umask(newmode);

	tcl_mask = malloc(stringlen); /* 3 digits & \0 */
	if (!tcl_mask) {
		return TCL_ERROR;
	}

	/* Totally gross and cool */
	p = tcl_mask + stringlen;
	*p = '\0';
	for (i = stringlen - 1; i > 0; i--) {
		p--;
		*p = (oldmode & 7) + '0';
		oldmode >>= 3;
	}
	if (*p != '0') {
		p--;
		*p = '0';
	}

	tcl_result = Tcl_NewStringObj(p, -1);
	free(tcl_mask);

	Tcl_SetObjResult(interp, tcl_result);
	return TCL_OK;
}

/**
 * symlink value target
 * Create a symbolic link at target pointing to value
 * See symlink(2) for possible errors
 */
int CreateSymlinkCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    char *value, *target;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "value target");
        return TCL_ERROR;
    }

    value = Tcl_GetString(objv[1]);
    target = Tcl_GetString(objv[2]);

    if (symlink(value, target) != 0) {
        Tcl_SetResult(interp, (char *)Tcl_PosixError(interp), TCL_STATIC);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/**
 * deletes environment variable
 *
 * Syntax is:
 * unsetenv name (* for all)
 */
int UnsetEnvCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    char *name;
    char **envp;
    char *equals;
    size_t len;
    Tcl_Obj *tclList;
    int listLength;
    Tcl_Obj **listArray;
    int loopCounter;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "name");
        return TCL_ERROR;
    }

    name = Tcl_GetString(objv[1]);
    if (strchr(name, '=') != NULL) {
        Tcl_SetResult(interp, "only the name should be given", TCL_STATIC);
        return TCL_ERROR;
    }

    if (strcmp(name, "*") == 0) {
#ifndef HAVE_CLEARENV
        /* unset all current environment variables; it'd be best to use
           clearenv() but that is not yet standardized, instead use Tcl's
           list capability to easily build an array of strings for each
           env name, then loop through that list to unsetenv() each one */
        tclList = Tcl_NewListObj( 0, NULL );
        Tcl_IncrRefCount( tclList );
        /* unset all current environment variables */
        for (envp = environ; *envp != NULL; envp++) {
            equals = strchr(*envp, '=');
            if (equals != NULL) {
                len = equals - *envp;
                Tcl_ListObjAppendElement(interp, tclList, Tcl_NewStringObj(*envp, len));
            }
        }
        Tcl_ListObjGetElements(interp, tclList, &listLength, &listArray);
        for (loopCounter = 0; loopCounter < listLength; loopCounter++) {
            unsetenv(Tcl_GetString(listArray[loopCounter]));
        }
        Tcl_DecrRefCount( tclList );
#else
        clearenv();
#endif
    } else {
        (void) unsetenv(name);
    }
    /* Tcl appears to become out of sync with the environment when we
       unset things, eg, 'info exists env(CC)' will succeed where
       'puts $env(CC)' will fail since it doesn't actually exist after
       being unset here.  This forces Tcl to resync to the current state
       (don't care about the actual result, so reset it) */
    Tcl_Eval(interp, "array get env");
    Tcl_ResetResult(interp);

    return TCL_OK;
}

/**
 *
 * Tcl wrapper around lchown() to allow changing ownership of symlinks
 * ('file attributes' follows the symlink).
 *
 * Synopsis: lchown filename user ?group?
 */
int lchownCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    CONST char *path;
    long user;
    long group = -1;

    if (objc != 3 && objc != 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "filename user ?group?");
        return TCL_ERROR;
    }

    path = Tcl_GetString(objv[1]);
    if (Tcl_GetLongFromObj(NULL, objv[2], &user) != TCL_OK) {
        CONST char *userString = Tcl_GetString(objv[2]);
        struct passwd *pwent = getpwnam(userString);
        if (pwent == NULL) {
            Tcl_SetResult(interp, "Unknown user given", TCL_STATIC);
            return TCL_ERROR;
        }
        user = pwent->pw_uid;
    }
    if (objc == 4) {
        if (Tcl_GetLongFromObj(NULL, objv[3], &group) != TCL_OK) {
           CONST char *groupString = Tcl_GetString(objv[3]);
           struct group *grent = getgrnam(groupString);
           if (grent == NULL) {
               Tcl_SetResult(interp, "Unknown group given", TCL_STATIC);
               return TCL_ERROR;
           }
           group = grent->gr_gid;
        }
    }
    if (lchown(path, (uid_t) user, (gid_t) group) != 0) {
        Tcl_SetResult(interp, (char *)Tcl_PosixError(interp), TCL_STATIC);
        return TCL_ERROR;
    }

    return TCL_OK;
}

int Pextlib_Init(Tcl_Interp *interp)
{
	if (Tcl_InitStubs(interp, "8.4", 0) == NULL)
		return TCL_ERROR;

	Tcl_CreateObjCommand(interp, "system", SystemCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "flock", FlockCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "readdir", ReaddirCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "strsed", StrsedCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "mkstemp", MkstempCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "mktemp", MktempCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "mkdtemp", MkdtempCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "existsuser", ExistsuserCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "existsgroup", ExistsgroupCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "nextuid", NextuidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "nextgid", NextgidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "md5", MD5Cmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "xinstall", InstallCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "fs-traverse", FsTraverseCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "filemap", FilemapCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "rpm-vercomp", RPMVercompCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "rmd160", RMD160Cmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "sha1", SHA1Cmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "umask", UmaskCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "pipe", PipeCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "curl", CurlCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "symlink", CreateSymlinkCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "unsetenv", UnsetEnvCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "lchown", lchownCmd, NULL, NULL);

	Tcl_CreateObjCommand(interp, "readline", ReadlineCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "rl_history", RLHistoryCmd, NULL, NULL);

	Tcl_CreateObjCommand(interp, "getuid", getuidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "geteuid", geteuidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "getgid", getgidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "getegid", getegidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "setuid", setuidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "seteuid", seteuidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "setgid", setgidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "setegid", setegidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "name_to_uid", name_to_uidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "uid_to_name", uid_to_nameCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "uname_to_gid", uname_to_gidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "name_to_gid", name_to_gidCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "gid_to_name", gid_to_nameCmd, NULL, NULL);

	Tcl_CreateObjCommand(interp, "tracelib", TracelibCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "isatty", IsattyCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "term_get_size", TermGetSizeCmd, NULL, NULL);

	if (Tcl_PkgProvide(interp, "Pextlib", "1.0") != TCL_OK)
		return TCL_ERROR;

	/* init libcurl */
	CurlInit(interp);

	return TCL_OK;
}
