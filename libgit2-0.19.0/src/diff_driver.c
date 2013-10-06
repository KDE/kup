/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"

#include "git2/attr.h"

#include "diff.h"
#include "diff_patch.h"
#include "diff_driver.h"
#include "strmap.h"
#include "map.h"
#include "buf_text.h"
#include "repository.h"

GIT__USE_STRMAP;

typedef enum {
	DIFF_DRIVER_AUTO = 0,
	DIFF_DRIVER_BINARY = 1,
	DIFF_DRIVER_TEXT = 2,
	DIFF_DRIVER_PATTERNLIST = 3,
} git_diff_driver_t;

enum {
	DIFF_CONTEXT_FIND_NORMAL = 0,
	DIFF_CONTEXT_FIND_ICASE = (1 << 0),
	DIFF_CONTEXT_FIND_EXT = (1 << 1),
};

/* data for finding function context for a given file type */
struct git_diff_driver {
	git_diff_driver_t type;
	uint32_t binary_flags;
	uint32_t other_flags;
	git_array_t(regex_t) fn_patterns;
	regex_t  word_pattern;
	char name[GIT_FLEX_ARRAY];
};

struct git_diff_driver_registry {
	git_strmap *drivers;
};

#define FORCE_DIFFABLE (GIT_DIFF_FORCE_TEXT | GIT_DIFF_FORCE_BINARY)

static git_diff_driver global_drivers[3] = {
	{ DIFF_DRIVER_AUTO,   0, 0, },
	{ DIFF_DRIVER_BINARY, GIT_DIFF_FORCE_BINARY, 0 },
	{ DIFF_DRIVER_TEXT,   GIT_DIFF_FORCE_TEXT, 0 },
};

git_diff_driver_registry *git_diff_driver_registry_new()
{
	git_diff_driver_registry *reg =
		git__calloc(1, sizeof(git_diff_driver_registry));
	if (!reg)
		return NULL;

	if ((reg->drivers = git_strmap_alloc()) == NULL) {
		git_diff_driver_registry_free(reg);
		return NULL;
	}

	return reg;
}

void git_diff_driver_registry_free(git_diff_driver_registry *reg)
{
	git_diff_driver *drv;

	if (!reg)
		return;

	git_strmap_foreach_value(reg->drivers, drv, git_diff_driver_free(drv));
	git_strmap_free(reg->drivers);
	git__free(reg);
}

static int diff_driver_add_funcname(
	git_diff_driver *drv, const char *name, int regex_flags)
{
	int error;
	regex_t re, *re_ptr;

	if ((error = regcomp(&re, name, regex_flags)) != 0) {
		/* TODO: warning about bad regex instead of failure */
		error = giterr_set_regex(&re, error);
		regfree(&re);
		return error;
	}

	re_ptr = git_array_alloc(drv->fn_patterns);
	GITERR_CHECK_ALLOC(re_ptr);

	memcpy(re_ptr, &re, sizeof(re));
	return 0;
}

static int diff_driver_xfuncname(const git_config_entry *entry, void *payload)
{
	return diff_driver_add_funcname(payload, entry->value, REG_EXTENDED);
}

static int diff_driver_funcname(const git_config_entry *entry, void *payload)
{
	return diff_driver_add_funcname(payload, entry->value, 0);
}

static git_diff_driver_registry *git_repository_driver_registry(
	git_repository *repo)
{
	if (!repo->diff_drivers) {
		git_diff_driver_registry *reg = git_diff_driver_registry_new();
		reg = git__compare_and_swap(&repo->diff_drivers, NULL, reg);

		if (reg != NULL) /* if we race, free losing allocation */
			git_diff_driver_registry_free(reg);
	}

	if (!repo->diff_drivers)
		giterr_set(GITERR_REPOSITORY, "Unable to create diff driver registry");

	return repo->diff_drivers;
}

static int git_diff_driver_load(
	git_diff_driver **out, git_repository *repo, const char *driver_name)
{
	int error = 0, bval;
	git_diff_driver_registry *reg;
	git_diff_driver *drv;
	size_t namelen = strlen(driver_name);
	khiter_t pos;
	git_config *cfg;
	git_buf name = GIT_BUF_INIT;
	const char *val;
	bool found_driver = false;

	reg = git_repository_driver_registry(repo);
	if (!reg)
		return -1;
	else {
		pos = git_strmap_lookup_index(reg->drivers, driver_name);
		if (git_strmap_valid_index(reg->drivers, pos)) {
			*out = git_strmap_value_at(reg->drivers, pos);
			return 0;
		}
	}

	/* if you can't read config for repo, just use default driver */
	if (git_repository_config__weakptr(&cfg, repo) < 0) {
		giterr_clear();
		return GIT_ENOTFOUND;
	}

	drv = git__calloc(1, sizeof(git_diff_driver) + namelen + 1);
	GITERR_CHECK_ALLOC(drv);
	drv->type = DIFF_DRIVER_AUTO;
	memcpy(drv->name, driver_name, namelen);

	if ((error = git_buf_printf(&name, "diff.%s.binary", driver_name)) < 0)
		goto done;
	if ((error = git_config_get_string(&val, cfg, name.ptr)) < 0) {
		if (error != GIT_ENOTFOUND)
			goto done;
		/* diff.<driver>.binary unspecified, so just continue */
		giterr_clear();
	} else if (git_config_parse_bool(&bval, val) < 0) {
		/* TODO: warn that diff.<driver>.binary has invalid value */
		giterr_clear();
	} else if (bval) {
		/* if diff.<driver>.binary is true, just return the binary driver */
		*out = &global_drivers[DIFF_DRIVER_BINARY];
		goto done;
	} else {
		/* if diff.<driver>.binary is false, force binary checks off */
		/* but still may have custom function context patterns, etc. */
		drv->binary_flags = GIT_DIFF_FORCE_TEXT;
		found_driver = true;
	}

	/* TODO: warn if diff.<name>.command or diff.<name>.textconv are set */

	git_buf_truncate(&name, namelen + strlen("diff.."));
	git_buf_put(&name, "xfuncname", strlen("xfuncname"));
	if ((error = git_config_get_multivar(
			cfg, name.ptr, NULL, diff_driver_xfuncname, drv)) < 0) {
		if (error != GIT_ENOTFOUND)
			goto done;
		giterr_clear(); /* no diff.<driver>.xfuncname, so just continue */
	}

	git_buf_truncate(&name, namelen + strlen("diff.."));
	git_buf_put(&name, "funcname", strlen("funcname"));
	if ((error = git_config_get_multivar(
			cfg, name.ptr, NULL, diff_driver_funcname, drv)) < 0) {
		if (error != GIT_ENOTFOUND)
			goto done;
		giterr_clear(); /* no diff.<driver>.funcname, so just continue */
	}

	/* if we found any patterns, set driver type to use correct callback */
	if (git_array_size(drv->fn_patterns) > 0) {
		drv->type = DIFF_DRIVER_PATTERNLIST;
		found_driver = true;
	}

	git_buf_truncate(&name, namelen + strlen("diff.."));
	git_buf_put(&name, "wordregex", strlen("wordregex"));
	if ((error = git_config_get_string(&val, cfg, name.ptr)) < 0) {
		if (error != GIT_ENOTFOUND)
			goto done;
		giterr_clear(); /* no diff.<driver>.wordregex, so just continue */
	} else if ((error = regcomp(&drv->word_pattern, val, REG_EXTENDED)) != 0) {
		/* TODO: warning about bad regex instead of failure */
		error = giterr_set_regex(&drv->word_pattern, error);
		goto done;
	} else {
		found_driver = true;
	}

	/* TODO: look up diff.<driver>.algorithm to turn on minimal / patience
	 * diff in drv->other_flags
	 */

	/* if no driver config found at all, fall back on AUTO driver */
	if (!found_driver)
		goto done;

	/* store driver in registry */
	git_strmap_insert(reg->drivers, drv->name, drv, error);
	if (error < 0)
		goto done;

	*out = drv;

done:
	git_buf_free(&name);

	if (!*out)
		*out = &global_drivers[DIFF_DRIVER_AUTO];

	if (drv && drv != *out)
		git_diff_driver_free(drv);

	return error;
}

int git_diff_driver_lookup(
	git_diff_driver **out, git_repository *repo, const char *path)
{
	int error = 0;
	const char *value;

	assert(out);

	if (!repo || !path || !strlen(path))
		goto use_auto;

	if ((error = git_attr_get(&value, repo, 0, path, "diff")) < 0)
		return error;

	if (GIT_ATTR_UNSPECIFIED(value))
		/* just use the auto value */;
	else if (GIT_ATTR_FALSE(value))
		*out = &global_drivers[DIFF_DRIVER_BINARY];
	else if (GIT_ATTR_TRUE(value))
		*out = &global_drivers[DIFF_DRIVER_TEXT];

	/* otherwise look for driver information in config and build driver */
	else if ((error = git_diff_driver_load(out, repo, value)) < 0) {
		if (error != GIT_ENOTFOUND)
			return error;
		else
			giterr_clear();
	}

use_auto:
	if (!*out)
		*out = &global_drivers[DIFF_DRIVER_AUTO];

	return 0;
}

void git_diff_driver_free(git_diff_driver *driver)
{
	size_t i;

	if (!driver)
		return;

	for (i = 0; i < git_array_size(driver->fn_patterns); ++i)
		regfree(git_array_get(driver->fn_patterns, i));
	git_array_clear(driver->fn_patterns);

	regfree(&driver->word_pattern);

	git__free(driver);
}

void git_diff_driver_update_options(
	uint32_t *option_flags, git_diff_driver *driver)
{
	if ((*option_flags & FORCE_DIFFABLE) == 0)
		*option_flags |= driver->binary_flags;

	*option_flags |= driver->other_flags;
}

int git_diff_driver_content_is_binary(
	git_diff_driver *driver, const char *content, size_t content_len)
{
	const git_buf search = { (char *)content, 0, min(content_len, 4000) };

	GIT_UNUSED(driver);

	/* TODO: provide encoding / binary detection callbacks that can
	 * be UTF-8 aware, etc.  For now, instead of trying to be smart,
	 * let's just use the simple NUL-byte detection that core git uses.
	 */

	/* previously was: if (git_buf_text_is_binary(&search)) */
	if (git_buf_text_contains_nul(&search))
		return 1;

	return 0;
}

static int diff_context_line__simple(
	git_diff_driver *driver, const char *line, size_t line_len)
{
	GIT_UNUSED(driver);
	GIT_UNUSED(line_len);
	return (git__isalpha(*line) || *line == '_' || *line == '$');
}

static int diff_context_line__pattern_match(
	git_diff_driver *driver, const char *line, size_t line_len)
{
	size_t i;

	GIT_UNUSED(line_len);

	for (i = 0; i < git_array_size(driver->fn_patterns); ++i) {
		if (!regexec(git_array_get(driver->fn_patterns, i), line, 0, NULL, 0))
			return true;
	}

	return false;
}

static long diff_context_find(
	const char *line,
	long line_len,
	char *out,
	long out_size,
	void *payload)
{
	git_diff_find_context_payload *ctxt = payload;

	if (git_buf_set(&ctxt->line, line, (size_t)line_len) < 0)
		return -1;
	git_buf_rtrim(&ctxt->line);

	if (!ctxt->line.size)
		return -1;

	if (!ctxt->match_line ||
		!ctxt->match_line(ctxt->driver, ctxt->line.ptr, ctxt->line.size))
		return -1;

	git_buf_truncate(&ctxt->line, (size_t)out_size);
	git_buf_copy_cstr(out, (size_t)out_size, &ctxt->line);

	return (long)ctxt->line.size;
}

void git_diff_find_context_init(
	git_diff_find_context_fn *findfn_out,
	git_diff_find_context_payload *payload_out,
	git_diff_driver *driver)
{
	*findfn_out = driver ? diff_context_find : NULL;

	memset(payload_out, 0, sizeof(*payload_out));
	if (driver) {
		payload_out->driver = driver;
		payload_out->match_line = (driver->type == DIFF_DRIVER_PATTERNLIST) ?
			diff_context_line__pattern_match : diff_context_line__simple;
		git_buf_init(&payload_out->line, 0);
	}
}

void git_diff_find_context_clear(git_diff_find_context_payload *payload)
{
	if (payload) {
		git_buf_free(&payload->line);
		payload->driver = NULL;
	}
}

