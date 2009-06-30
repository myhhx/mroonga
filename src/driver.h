#ifndef _driver_h
#define _driver_h

#include <stdlib.h>
#include <stdio.h>
#include <groonga.h>

#define MRN_MAX_KEY_LEN 1024
#define MRN_DB_FILE_PATH "groonga.db"
#define MRN_LOG_FILE_NAME "groonga.log"
#define MRN_LEXICON_TABLE_NAME "lexicon"

typedef struct _mrn_charset_map {
  const char *csname_mysql;
  grn_encoding csname_groonga;
} MRN_CHARSET_MAP;

typedef struct _mrn_field {
  const char *name;
  uint name_len;
  grn_obj *obj;
  grn_obj *index;
  //grn_id gid;
  uint field_no;
} mrn_field;

typedef struct _mrn_table {
  const char *name;
  uint name_len;
  uint use_count;
  grn_obj *obj;
  //grn_id gid;
  mrn_field **field;
  uint fields;
  uint pkey_field;
} mrn_table;


typedef struct _mrn_column_info {
  char *name;
  uint name_size;
  char *path;
  grn_obj_flags flags;
  grn_obj *type;
} mrn_column_info;

typedef struct _mrn_table_info {
  char *name;
  uint name_size;
  char *path;
  grn_obj_flags flags;
  grn_obj *key_type;
  uint value_size;
} mrn_table_info;

typedef struct _mrn_create_info {
  mrn_table_info table;
  mrn_column_info *columns;
  uint n_columns;
} mrn_create_info;

/* macro */
#define MRN_MALLOC(size) malloc(size)
#define MRN_FREE(ptr) free(ptr)

#define MRN_HANDLER_NAME(obj_name) (obj_name - 2)
#define MRN_TABLE_NAME(name) (name + 2)

/* functions */
int mrn_init();
int mrn_deinit();
void mrn_logger_func(int level, const char *time, const char *title,
		     const char *msg, const char *location, void *func_arg);
int mrn_flush_logs(grn_ctx *ctx);

int mrn_hash_put(grn_ctx *ctx, const char *key, void *value);
int mrn_hash_get(grn_ctx *ctx, const char *key, void **value);
int mrn_hash_remove(grn_ctx *ctx, const char *key);
mrn_create_info*  mrn_init_create_info(grn_ctx *ctx, uint n_columns);
int mrn_deinit_create_info(grn_ctx *ctx, mrn_create_info *info);
int mrn_create(grn_ctx *ctx, mrn_create_info *info);

/* static variables */
extern grn_hash *mrn_hash;
extern grn_obj *mrn_db, *mrn_lexicon;
extern pthread_mutex_t *mrn_lock;
extern const char *mrn_logfile_name;
extern FILE *mrn_logfile;

extern grn_logger_info mrn_logger_info;

#endif /* _driver_h */
