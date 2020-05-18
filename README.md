# fdb - filesystem database
fdb provides a simle model for creating databases, as well as an thread safe and POSIX compliant interface implementing such model, all using an underlying tree-structured file system.

## fdb database model
fdb structures data in a tree (just as the tree-structured filesystem) based on two main concepts:

- **root**: bank not in a bank (the root of the tree).
- **bank**: collection of banks identified by a string and the bank or root it is in (middle node in the tree).
- **registry**: chunk of data identified by a string and the bank they are in (leaf node in the tree).

As it is apparent, in the file system *banks* are represented by *directories* and *registries* by *files*.
Therefore, the underlying filesystem must have both to be able to support a database following this model.

## fdb library
The fdb interface is defined in the file `fdb.h`. It provides functions to manage banks and registries as expected,
which are all described in the file.

To access and modify the data in registries, the implementation maps them to memory to allow easy,
thread-safe cache implementations, delegating cache management and policies to the operating system.

The path of the root bank is `/var/fdb` by default, but can be specified through an environment variable.
By default, fdb will use the variable `FDB_ROOT`, but this can be changed passing a non-null
string to `fdb_init` with the name of the environment variable where to read the path of the root from.

### Usage example
```
fsh_bank bank, fdb_reg reg;

char *bank_name = "bank_name";
char *reg_name  = "reg_name";

struct data reg_data;

fdb_init("EXAMPLE_ROOT"); // read root path from $EXAMPLE_ROOT

// create bank in the root bank
if (fdb_bank_create (FDB_ROOT, bank_name, &bank) != -1) {
    if (fdb_bank_getreg (bank, reg_name, &reg) == 0) {
        // if the regisry exists, read its data
        fdb_reg_open (&reg, sizeof (reg_data));
        fdb_reg_read (&reg, &reg_data);
        fdb_reg_close (&reg);
        fdb_reg_destroy (&reg);
    } else if (fdb_reg_create (bank, reg_name, &reg) == 0) {
        // if the registry does not exist, create it and write the data
        fdb_reg_open (&reg, sizeof (reg_data));
        fdb_reg_write (&reg, &reg_data);
        fdb_reg_close (&reg);
        fdb_reg_destroy (&reg);
    } else {
        // something failed opening or creating the registry
        perror(reg_name);
    }
} else {
    // something failed
    perror("bank_create");
}
```

### Building fdb
Use the `all` target of the makefile to generate the library `libfdb.a`. Use the `install` target
to copy the library and headers in `/usr`.
```
$> make all
$> sudo make install
```
