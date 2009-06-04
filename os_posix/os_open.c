/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008 WiredTiger Software.
 *	All rights reserved.
 *
 * $Id$
 */

#include "wt_internal.h"

/*
 * __wt_open --
 *	Open a file handle.
 */
int
__wt_open(ENV *env,
    const char *name, mode_t mode, u_int32_t flags, WT_FH **fhp)
{
	DB *db;
	IDB *idb;
	WT_FH *fh;
	int f, fd, ret;

	fh = NULL;

	if (FLD_ISSET(env->verbose, WT_VERB_FILEOPS | WT_VERB_FILEOPS_ALL))
		__wt_env_errx(env, "fileops: %s: open", name);

	/* Increment the reference count if we already have the file open. */
	TAILQ_FOREACH(db, &env->dbqh, q) {
		idb = db->idb;
		if ((fh = idb->fh) == NULL)
			continue;
		if (strcmp(name, idb->dbname) == 0) {
			++fh->refcnt;
			*fhp = fh;
			return (0);
		}
	}

	f = O_RDWR;
#ifdef O_BINARY
	/* Windows clones: we always want to treat the file as a binary. */
	f |= O_BINARY;
#endif
	if (LF_ISSET(WT_CREATE))
		f |= O_CREAT;

	if ((fd = open(name, f, mode)) == -1) {
		__wt_env_err(env, errno, "%s", name);
		return (WT_ERROR);
	}

	if ((ret = __wt_calloc(env, 1, sizeof(WT_FH), &fh)) != 0)
		return (ret);
	if ((ret = __wt_stat_alloc_fh_stats(env, &fh->stats)) != 0)
		goto err;
	if ((ret = __wt_strdup(env, name, &fh->name)) != 0)
		goto err;

#if defined(HAVE_FCNTL) && defined(FD_CLOEXEC)
	/*
	 * Security:
	 * The application may spawn a new process, and we don't want another
	 * process to have access to our file handles.  There's an obvious
	 * race here...
	 */
	if ((f = fcntl(fd, F_GETFD)) == -1 ||
	    fcntl(fd, F_SETFD, f | FD_CLOEXEC) == -1) {
		__wt_env_err(env, errno, "%s: fcntl", name);
		goto err;
	}
#endif

	fh->fd = fd;
	fh->refcnt = 1;
	*fhp = fh;

	/* Set the file's size. */
	if ((ret = __wt_filesize(env, fh, &fh->file_size)) != 0)
		goto err;

	return (0);

err:	if (fh != NULL) {
		if (fh->name != NULL)
			__wt_free(env, fh->name);
		__wt_free(env, fh);
	}
	(void)close(fd);
	return (WT_ERROR);
}

/*
 * __wt_close --
 *	Close a file handle.
 */
int
__wt_close(ENV *env, WT_FH *fh)
{
	if (--fh->refcnt == 0) {
		__wt_free(env, fh->name);
		__wt_free(env, fh->stats);
		__wt_free(env, fh);
	}
	return (0);
}
