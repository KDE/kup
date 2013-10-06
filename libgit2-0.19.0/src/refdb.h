/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_refdb_h__
#define INCLUDE_refdb_h__

#include "git2/refdb.h"
#include "repository.h"

struct git_refdb {
	git_refcount rc;
	git_repository *repo;
	git_refdb_backend *backend;
};

void git_refdb__free(git_refdb *db);

int git_refdb_exists(
	int *exists,
	git_refdb *refdb,
	const char *ref_name);

int git_refdb_lookup(
	git_reference **out,
	git_refdb *refdb,
	const char *ref_name);

int git_refdb_rename(
	git_reference **out,
	git_refdb *db,
	const char *old_name,
	const char *new_name,
	int force);

int git_refdb_iterator(git_reference_iterator **out, git_refdb *db, const char *glob);
int git_refdb_iterator_next(git_reference **out, git_reference_iterator *iter);
int git_refdb_iterator_next_name(const char **out, git_reference_iterator *iter);
void git_refdb_iterator_free(git_reference_iterator *iter);

int git_refdb_write(git_refdb *refdb, git_reference *ref, int force);
int git_refdb_delete(git_refdb *refdb, const char *ref_name);

#endif
