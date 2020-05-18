#ifndef FDB_H
#define FDB_H

/* used by fdb_bank_scan */
#define FDBT_ANY  3
#define FDBT_REG  1
#define FDBT_BANK 2

/* root and defaults */
#define FDB_ROOT (fdb_bank) { realpath: NULL }
#define FDB_DEFAULT_ENV  "FDB_ROOT"
#define FDB_DEFAULT_ROOT "/var/fdb"

#include <stddef.h>

typedef struct { char *realpath; int fd; size_t size; void *map; } fdb_reg;
typedef struct { char *realpath; } fdb_bank;

typedef struct {
	int type;
	union {
		fdb_bank bank;
		fdb_reg reg;
	} entry;
} fdb_entry;


/** 
 * This function must be called before any other.
 *
 * It creates and initializes the root of FDB to the path in the environment
 * variable `root_env' (or $FDB_ROOT by default).
 *
 * If no path is defined there, it defaults to `/var/fdb'
 */
int fdb_init (const char *root_env);

/**
 * Call this function after the execution of all fdb functions
 */
void fdb_finalize();

/**
 * Creates a new bank in `parent'.
 *
 * If `parent' is FDB_ROOT, it is created in the root.
 *
 * If the bank has been created returns 0, and the new bank in *bank only if bank is not NULL.
 * If the bank exists,  returns 1 and leaves *bank unmodified.
 * If an error occured (or if the path exists and is not a bank),
 *     returns -1, leaves *bank unmodified and sets errno accordingly.
 */
int fdb_bank_create (const fdb_bank parent, const char *name, fdb_bank *bank);
/**
 * Searches for a registry in a bank.
 *
 * If the registry exists returns 0, and the registry found in *reg only if reg is not NULL.
 * If the path to the registry exists but is not a registry returns -1 and leaves *reg unmodified
 * If an error occured returns -1, leaves *reg unmodified and sets errno accordingly.
 */
int fdb_bank_getreg (const fdb_bank parent, const char *name, fdb_reg *reg);
/**
 * Searches for a bank in a bank.
 *
 * If the bank exists returns 0, and the bank found in *bank only if bank is not NULL.
 * If the path to the bank exists but is not a bank returns -1 and leaves *bank unmodified
 * If an error occured returns -1, leaves *bank unmodified and sets errno accordingly.
 */
int fdb_bank_getbank (const fdb_bank parent, const char *name, fdb_bank *bank);
/**
 * Scans entries in a bank (see fdb_entry)
 *
 * Returns the number of entries, and an array of entries in *list only if list is not NULL.
 * Only entries of type type will be included in the list. type can be any of the following, or their bitwise or:
 *  - FDBT_BANK: only include banks
 *  - FDBT_REG:  only include registries
 *  - FDBT_ANY:  include both banks and registries
 *
 * The returned array must be freed with a call to free(3), only after destroying
 * its entries with the appropiate calls to fdb_reg_destroy and fdb_bank_destroy.
 *
 * If there was an error, returns -1 and leaves *list unmodified
 */
int fdb_bank_scan (const fdb_bank parent, fdb_entry **list, int type);
/**
 * remove (recursively) the bank
 */
int fdb_bank_remove   (fdb_bank *bank);
/** 
 * Free resources allocated for the bank variable
 */
void fdb_bank_destroy (fdb_bank *bank);

/**
 * This funcion is similar to fdb_reg_bank
 */
int fdb_reg_create (const fdb_bank parent, const char *name, fdb_reg *reg);
/**
 * Opens the registry with the given size.
 * This function maps the regitry (file) to memory.
 */
int fdb_reg_open  (fdb_reg *reg, size_t s);
/**
 * Read the contents of the registry into buf
 *
 * buf must be of a size greater or equal than the registry
 */
int fdb_reg_read  (fdb_reg *reg, void *buf);
/**
 * Write the contents of reg into the registry
 *
 * buf must be of a size greater or equal than the registry
 */
int fdb_reg_write (fdb_reg *reg, void *buf);
/** 
 * Close the registry and unmap the file
 */
int fdb_reg_close (fdb_reg *reg);
/**
 * Remove (unlink) the file of the registry
 */
int fdb_reg_remove (fdb_reg *reg);
/** 
 * Free resources allocated for the registry variable
 */
void fdb_reg_destroy (fdb_reg *reg);

#endif /* FDB_H*/
