#include "fdb.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#define NEWBANK(rpath) (fdb_bank) { realpath: rpath }
#define NEWREG(rpath) (fdb_reg) { realpath: rpath, fd: -1, map: NULL }

static fdb_bank root;

static char * create_rpath (const char *parent, const char *child);
static int remove_directory (const char *path);
static inline int check_bank (const char *rpath);
static inline int check_reg  (const char *rpath);

static char * create_rpath (const char *rparent, const char *child)
{
	//TODO comprobar otros caracteres invalidos
	if (strchr(child, '/') != NULL) {
		errno = EINVAL;
		return NULL;
	}
	size_t len = (strlen(child)+strlen(rparent)+2); // +2 por '/' y '\0'
	char *rpath = malloc(len * sizeof(char));
	if (rpath == NULL) {
		perror ("create_rpath: malloc");
		return NULL;
	}
	if(sprintf(rpath, "%s/%s", rparent, child) < len-1) {
		free(rpath);
		perror ("create_rpath: sprintf");
		return NULL;
	}
	return rpath;
}

static int remove_directory(const char *path)
{
	DIR *d = opendir(path);
	size_t path_len = strlen(path);
	int r = -1;

	if (d) {
		struct dirent *p;
		r = 0;
		while (!r && (p=readdir(d))) {
			int r2 = -1;
			char *buf;
			size_t len;

			/* Skip the names "." and ".." as we don't want to recurse on them. */
			if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
				continue;
			}
			len = path_len + strlen(p->d_name) + 2;
			buf = malloc(len);

			if (buf) {
				struct stat statbuf;
				snprintf(buf, len, "%s/%s", path, p->d_name);
				if (!stat(buf, &statbuf)) {
					if (S_ISDIR(statbuf.st_mode)) {
						r2 = remove_directory(buf);
					} else {
						r2 = unlink(buf);
					}
				}
				free(buf);
			}
			r = r2;
		}
		closedir(d);
	}
	if (!r) {
		r = rmdir(path);
	}
	return r;
}
/**
 * Devuelve 0 si existe, 1 si no es un banco y -1 si no existe u otro error
 */
static inline int check_bank (const char *rpath)
{
	int err = 0;
	struct stat st;
	if (!stat(rpath, &st)) {
		if (!S_ISDIR(st.st_mode)) {
			err = 1;
		}
	} else {
		err = -1;
	}
	return err;
}
/**
 * Devuelve 0 si existe, 1 si no es un registro y -1 si no existe u otro error
 */
static inline int check_reg  (const char *rpath)
{
	int err = 0;
	struct stat st;
	if (!stat(rpath, &st)) {
		if (!S_ISREG(st.st_mode)) {
			err = 1;
		}
	} else {
		err = -1;
	}
	return err;
}


int fdb_init (const char *root_env)
{
	if (root_env == NULL)
		root_env = FDB_DEFAULT_ENV;

	char *p = getenv(root_env);

	if (p == NULL || strlen(p) == 0) {
		root.realpath = FDB_DEFAULT_ROOT;
	} else {
		root.realpath = p;
	}

	if (mkdir(root.realpath, S_IRWXU | S_IRWXG)) {
		if (errno != EEXIST) {
			return -1;
		} else{
			errno = 0;
		}
	}

	root.realpath = realpath(root.realpath, NULL);

	return 0;
}

void fdb_finalize () {
	free(root.realpath);
}

int fdb_bank_create (const fdb_bank parent, const char *name, fdb_bank *bank)
{
	char *rpath;
	if (parent.realpath == NULL)
		rpath = create_rpath(root.realpath, name);
	else
		rpath = create_rpath(parent.realpath, name);
	if (rpath == NULL) return -1;

	int err = 0;
	if (mkdir (rpath, 0700)) {
		if (errno == EEXIST) {
			err = check_bank(rpath);
			if (err == 0) err = 1; // existe el banco
			else err = -1; // otro error (path no disponible)
		} else {
			err = -1;
		}
	}

	if (!err && bank != NULL) {
		*bank = NEWBANK(rpath);
	} else {
		free(rpath);
	}

	return err;
}
int fdb_bank_getreg (const fdb_bank parent, const char *name, fdb_reg *reg)
{
	char *rpath;
	if (parent.realpath == NULL)
		rpath = create_rpath(root.realpath, name);
	else
		rpath = create_rpath(parent.realpath, name);
	if (rpath == NULL) return -1;

	int err = check_reg(rpath);

	if (!err && reg != NULL) {
		*reg = NEWREG(rpath);
	} else {
		free (rpath);
	}

	return err;
}
int fdb_bank_getbank (const fdb_bank parent, const char *name, fdb_bank *bank)
{
	char *rpath;
	if (parent.realpath == NULL)
		rpath = create_rpath(root.realpath, name);
	else
		rpath = create_rpath(parent.realpath, name);
	if (rpath == NULL) return -1;

	int err = check_bank(rpath);

	if (!err && bank != NULL) {
		*bank = NEWBANK(rpath);
	} else {
		free (rpath);
	}
	return err;
}

int fdb_bank_scan (const fdb_bank bank, fdb_entry **list, int type)
{
	fdb_bank parent;
	if (bank.realpath == NULL)
		parent = root;
	else
		parent = bank;

	fdb_entry *arr = NULL;
	struct dirent **namelist = NULL;
	
	int err = 0, ret = 0, null = (list == NULL);
	if (type & FDBT_ANY) {
		int n = scandir(parent.realpath, &namelist, NULL, alphasort);
		if (n < 0) {
			perror (parent.realpath);
			err = -1;
		} else {
			if (!null) {
				arr = calloc (n, sizeof(fdb_entry));
				if (arr == NULL) {
					perror ("fdb_bank_scan: calloc");
					err = -1;
				}
			}
			for (int i = 0; i < n; i++) {
				if (!err && strcmp(".", namelist[i]->d_name) != 0 && strcmp("..", namelist[i]->d_name) != 0) {
					char *rpath = create_rpath (parent.realpath, namelist[i]->d_name);
					struct stat st;
					if (rpath == NULL) {
						err = -1;
					} else {
						if (stat (rpath, &st)) {
							free (rpath);
							err = -1;
						} else {
							int assigned = 0;
							if ((type & FDBT_REG) && S_ISREG(st.st_mode)) {
								if (!null) {
									arr[ret].type = FDBT_REG;
									arr[ret].entry.reg = NEWREG(rpath);
									assigned = 1;
								}
								ret++;
							} else if ((type & FDBT_BANK) && S_ISDIR(st.st_mode)) {
								if (!null) {
									arr[ret].type = FDBT_BANK;
									arr[ret].entry.bank = NEWBANK(rpath);
									assigned = 1;
								}
								ret++;
							}
							if (!assigned) {
								free (rpath);
							}
						}
					}
				}
				free (namelist[i]);
			}
			free (namelist);
		}
	} else {
		errno = EINVAL;
		err = -1;
	}

	if (!err) {
		if (!null) {
			fdb_entry *real = realloc(arr, ret * sizeof(fdb_entry));
			if (real == NULL) {
				err = -1;
			} else {
				*list = real;
			}
		}
	} else {
		if (arr != NULL) {
			for (int i = 0; i < ret; i++) {
				switch (arr[i].type) {
					case FDBT_REG:
						fdb_reg_destroy(&arr[i].entry.reg);
						break;
					case FDBT_BANK:
						fdb_bank_destroy(&arr[i].entry.bank);
						break;
				}
			}
			free (arr);
		}
	}

	return ret;
}

int fdb_bank_remove (fdb_bank *bank)
{
	int r = remove_directory (bank->realpath);
	return r;
}

void fdb_bank_destroy (fdb_bank *bank)
{
	free(bank->realpath);
	bank->realpath = NULL;
}

int fdb_reg_create (const fdb_bank parent, const char *name, fdb_reg *reg)
{
	char *rpath;
	if (parent.realpath == NULL)
		rpath = create_rpath(root.realpath, name);
	else
		rpath = create_rpath(parent.realpath, name);
	if (rpath == NULL) return -1;

	int err = 0;
	int fd;
	if ((fd = open (rpath, O_CREAT | O_TRUNC | O_EXCL, 0700)) < 0) {
		if (errno == EEXIST) {
			err = check_reg (rpath);
			if (err == 0) err = 1; // existe el registro
			else err = -1; // otro error (path no disponible)
		} else {
			err = -1;
		}
	} else while (close(fd)) {
		// no puede quedarse el fichero abierto
		perror (rpath);
	}

	if (!err) {
		*reg = NEWREG(rpath);
	} else {
		free (rpath);
	}

	return err;
}
int fdb_reg_open  (fdb_reg *reg, size_t s)
{
	int err = 0;
	int fd = open(reg->realpath, O_RDWR);
	void *map;

	if ((fd < 0) || (ftruncate(fd, s) < 0)) {
		err = -1;
	} else {
		map = mmap(NULL, s, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (map == MAP_FAILED) {
			err = -1;
		}
	}

	if (!err) {
		reg->fd = fd;
		reg->map = map;
		reg->size = s;
	} else {
		if (fd >= 0) {
			// no puede salir sin cerrar el fichero
			while (close(fd)) perror(reg->realpath);
		}
	}

	return err;
}
int fdb_reg_read  (fdb_reg *reg, void *buf)
{
	if (reg->map == NULL) return -1;
	memcpy (buf, reg->map, reg->size);
	return 0;
}
int fdb_reg_write (fdb_reg *reg, void *buf)
{
	if (reg->map == NULL) return -1;
	memcpy (reg->map, buf, reg->size);
	return 0;
}
int fdb_reg_close (fdb_reg *reg)
{
	int err = 0;
	if (reg->fd < 0 || close(reg->fd)) {
		err = -1;
	} else if (reg->map == NULL || munmap(reg->map, reg->size)) {
		err = -1;
	}

	return err;
}
int fdb_reg_remove (fdb_reg *reg) {
	int r = unlink(reg->realpath);
	return r;
}
void fdb_reg_destroy (fdb_reg *reg)
{
	free (reg->realpath);
	reg->fd = -1;
	reg->map = NULL;
}
