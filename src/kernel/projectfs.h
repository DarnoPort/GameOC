#ifndef PROJECTFS_H
#define PROJECTFS_H

#include "types.h"

#define PROJECTFS_NAME_MAX       19u
#define PROJECTFS_MAX_FILE_SIZE  65536u
#define PROJECTFS_MAX_FILES      128u

/* Initialise from the attached IDE disk. Does not format invalid media. */
int projectfs_init(void);
int projectfs_is_ready(void);
int projectfs_count(void);
int projectfs_name_at(uint32_t index, char *name_out, uint32_t capacity);
int32_t projectfs_size(const char *name);
int32_t projectfs_read(const char *name, void *buffer, uint32_t capacity);
int projectfs_write(const char *name, const void *buffer, uint32_t size);
int projectfs_delete(const char *name);
int projectfs_rename(const char *old_name, const char *new_name);

#endif
