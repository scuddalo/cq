/*
 * entry.h
 * $Id: entry.h 28029 2007-08-18 15:59:59Z sfiera@macports.org $
 *
 * Copyright (c) 2007 Chris Pickel <sfiera@macports.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _CENTRY_H
#define _CENTRY_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <sqlite3.h>
#include <cregistry/registry.h>

typedef enum {
    reg_strategy_exact = 1,
    reg_strategy_glob = 2,
    reg_strategy_regexp = 3
} reg_strategy;

typedef struct {
    sqlite_int64 id; /* rowid in database */
    reg_registry* reg; /* associated registry */
    char* proc; /* name of Tcl proc, if using Tcl */
} reg_entry;

reg_entry* reg_entry_create(reg_registry* reg, char* name, char* version,
        char* revision, char* variants, char* epoch, reg_error* errPtr);

reg_entry* reg_entry_open(reg_registry* reg, char* name, char* version,
        char* revision, char* variants, char* epoch, reg_error* errPtr);

int reg_entry_delete(reg_entry* entry, reg_error* errPtr);

void reg_entry_free(reg_entry* entry);

int reg_entry_search(reg_registry* reg, char** keys, char** vals, int key_count,
        int strategy, reg_entry*** entries, reg_error* errPtr);

int reg_entry_imaged(reg_registry* reg, const char* name, const char* version,
        const char* revision, const char* variants, reg_entry*** entries,
        reg_error* errPtr);
int reg_entry_installed(reg_registry* reg, char* name, reg_entry*** entries,
        reg_error* errPtr);

int reg_entry_owner(reg_registry* reg, char* path, reg_entry** entry,
        reg_error* errPtr);

int reg_entry_propget(reg_entry* entry, char* key, char** value,
        reg_error* errPtr);
int reg_entry_propset(reg_entry* entry, char* key, char* value,
        reg_error* errPtr);

int reg_entry_map(reg_entry* entry, char** files, int file_count,
        reg_error* errPtr);
int reg_entry_unmap(reg_entry* entry, char** files, int file_count,
        reg_error* errPtr);

int reg_entry_files(reg_entry* entry, char*** files, reg_error* errPtr);
int reg_entry_imagefiles(reg_entry* entry, char*** files, reg_error* errPtr);

int reg_entry_activate(reg_entry* entry, char** files, char** as_files,
        int file_count, reg_error* errPtr);
int reg_entry_deactivate(reg_entry* entry, char** files, int file_count,
        reg_error* errPtr);

int reg_entry_dependents(reg_entry* entry, reg_entry*** dependents,
        reg_error* errPtr);
int reg_entry_dependencies(reg_entry* entry, reg_entry*** dependencies,
        reg_error* errPtr);
int reg_entry_depends(reg_entry* entry, char* name, reg_error* errPtr);

int reg_all_open_entries(reg_registry* reg, reg_entry*** entries);

#endif /* _CENTRY_H */
