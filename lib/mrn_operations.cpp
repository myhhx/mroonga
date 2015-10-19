/* -*- c-basic-offset: 2 -*- */
/*
  Copyright(C) 2015 Kouhei Sutou <kou@clear-code.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <string.h>

#include "mrn_operations.hpp"
#include "mrn_smart_grn_obj.hpp"
#include "mrn_path_mapper.hpp"

// for debug
#define MRN_CLASS_NAME "mrn::Operations"

#define TABLE_NAME "mroonga_operations"
#define COLUMN_TYPE_NAME   "type"
#define COLUMN_TABLE_NAME  "table"
#define COLUMN_RECORD_NAME "record"

namespace mrn {
  Operations::Operations(grn_ctx *ctx)
    : ctx_(ctx) {
    MRN_DBUG_ENTER_METHOD();

    GRN_TEXT_INIT(&text_buffer_, GRN_OBJ_DO_SHALLOW_COPY);
    GRN_UINT32_INIT(&id_buffer_, 0);

    table_ = grn_ctx_get(ctx_, TABLE_NAME, -1);
    if (!table_) {
      table_ = grn_table_create(ctx_,
                                TABLE_NAME, strlen(TABLE_NAME),
                                NULL,
                                GRN_OBJ_TABLE_NO_KEY | GRN_OBJ_PERSISTENT,
                                NULL, NULL);
      columns_.type_ =
        grn_column_create(ctx_, table_,
                          COLUMN_TYPE_NAME, strlen(COLUMN_TYPE_NAME),
                          NULL,
                          GRN_OBJ_COLUMN_SCALAR | GRN_OBJ_PERSISTENT,
                          grn_ctx_at(ctx_, GRN_DB_SHORT_TEXT));
      columns_.table_ =
        grn_column_create(ctx_, table_,
                          COLUMN_TABLE_NAME, strlen(COLUMN_TABLE_NAME),
                          NULL,
                          GRN_OBJ_COLUMN_SCALAR | GRN_OBJ_PERSISTENT,
                          grn_ctx_at(ctx_, GRN_DB_SHORT_TEXT));
      columns_.record_ =
        grn_column_create(ctx_, table_,
                          COLUMN_RECORD_NAME, strlen(COLUMN_RECORD_NAME),
                          NULL,
                          GRN_OBJ_COLUMN_SCALAR | GRN_OBJ_PERSISTENT,
                          grn_ctx_at(ctx_, GRN_DB_UINT32));
    } else {
      columns_.type_   = grn_ctx_get(ctx_, TABLE_NAME "." COLUMN_TYPE_NAME, -1);
      columns_.table_  = grn_ctx_get(ctx_, TABLE_NAME "." COLUMN_TABLE_NAME, -1);
      columns_.record_ = grn_ctx_get(ctx_, TABLE_NAME "." COLUMN_RECORD_NAME, -1);
    }

    DBUG_VOID_RETURN;
  }

  Operations::~Operations() {
    MRN_DBUG_ENTER_METHOD();

    GRN_OBJ_FIN(ctx_, &id_buffer_);
    GRN_OBJ_FIN(ctx_, &text_buffer_);

    DBUG_VOID_RETURN;
  }

  grn_id Operations::start(const char *type,
                           const char *table_name, size_t table_name_size) {
    MRN_DBUG_ENTER_METHOD();

    grn_id id = grn_table_add(ctx_, table_, NULL, 0, NULL);

    GRN_TEXT_SETS(ctx_, &text_buffer_, type);
    grn_obj_set_value(ctx_, columns_.type_,  id, &text_buffer_, GRN_OBJ_SET);

    GRN_TEXT_SET(ctx_, &text_buffer_, table_name, table_name_size);
    grn_obj_set_value(ctx_, columns_.table_, id, &text_buffer_, GRN_OBJ_SET);

    DBUG_RETURN(id);
  }

  void Operations::record_target(grn_id id, grn_id record_id) {
    MRN_DBUG_ENTER_METHOD();

    GRN_UINT32_SET(ctx_, &id_buffer_, record_id);
    grn_obj_set_value(ctx_, columns_.record_,  id, &id_buffer_, GRN_OBJ_SET);

    DBUG_VOID_RETURN;
  }

  void Operations::finish(grn_id id) {
    MRN_DBUG_ENTER_METHOD();

    grn_table_delete_by_id(ctx_, table_, id);

    DBUG_VOID_RETURN;
  }

  bool Operations::is_remain(const char *table_name, size_t table_name_size) {
    MRN_DBUG_ENTER_METHOD();

    grn_table_cursor *cursor;
    cursor = grn_table_cursor_open(ctx_, table_, NULL, 0, NULL, 0, 0, -1, 0);
    if (!cursor) {
      DBUG_RETURN(false);
    }

    bool have_operation = false;
    grn_id id;
    while ((id = grn_table_cursor_next(ctx_, cursor))) {
      GRN_BULK_REWIND(&text_buffer_);
      grn_obj_get_value(ctx_, columns_.table_, id, &text_buffer_);
      if ((static_cast<size_t>(GRN_TEXT_LEN(&text_buffer_)) ==
           table_name_size) &&
          memcmp(GRN_TEXT_VALUE(&text_buffer_),
                 table_name,
                 table_name_size) == 0) {
        have_operation = true;
        break;
      }
    }
    grn_table_cursor_close(ctx_, cursor);

    DBUG_RETURN(have_operation);
  }

  int Operations::repair(const char *table_name, size_t table_name_size) {
    MRN_DBUG_ENTER_METHOD();

    int error = 0;

    grn_table_cursor *cursor;
    cursor = grn_table_cursor_open(ctx_, table_, NULL, 0, NULL, 0, 0, -1, 0);
    if (!cursor) {
      error = HA_ERR_CRASHED_ON_USAGE;
      if (ctx_->rc) {
        my_message(error, ctx_->errbuf, MYF(0));
      } else {
        my_message(error,
                   "mroonga: repair: "
                   "failed to open cursor for operations table",
                   MYF(0));
      }
      DBUG_RETURN(error);
    }

    char terminated_table_name[MRN_MAX_PATH_SIZE];
    memcpy(terminated_table_name, table_name, table_name_size);
    terminated_table_name[table_name_size] = '\0';
    mrn::PathMapper mapper(terminated_table_name);
    mrn::SmartGrnObj target_table(ctx_, mapper.table_name());

    grn_id id;
    while ((id = grn_table_cursor_next(ctx_, cursor))) {
      GRN_BULK_REWIND(&text_buffer_);
      grn_obj_get_value(ctx_, columns_.table_, id, &text_buffer_);
      if (!((static_cast<size_t>(GRN_TEXT_LEN(&text_buffer_)) ==
             table_name_size) &&
            memcmp(GRN_TEXT_VALUE(&text_buffer_),
                   table_name,
                   table_name_size) == 0)) {
        continue;
      }

      GRN_BULK_REWIND(&id_buffer_);
      grn_obj_get_value(ctx_, columns_.record_, id, &id_buffer_);
      grn_id record_id = GRN_UINT32_VALUE(&id_buffer_);
      if (record_id == GRN_ID_NIL) {
        grn_table_cursor_delete(ctx_, cursor);
        continue;
      }

      GRN_BULK_REWIND(&text_buffer_);
      grn_obj_get_value(ctx_, columns_.type_, id, &text_buffer_);
      GRN_TEXT_PUTC(ctx_, &text_buffer_, '\0');
      if (strcmp(GRN_TEXT_VALUE(&text_buffer_), "write") == 0 ||
          strcmp(GRN_TEXT_VALUE(&text_buffer_), "delete") == 0) {
        grn_table_delete_by_id(ctx_, target_table.get(), record_id);
        grn_table_cursor_delete(ctx_, cursor);
      } else if (strcmp(GRN_TEXT_VALUE(&text_buffer_), "update") == 0) {
        error = HA_ERR_CRASHED_ON_USAGE;
        my_message(error,
                   "mroonga: repair: can't recover from crash while updating",
                   MYF(0));
        break;
      } else {
        error = HA_ERR_CRASHED_ON_USAGE;
        char error_message[MRN_MESSAGE_BUFFER_SIZE];
        snprintf(error_message, MRN_MESSAGE_BUFFER_SIZE,
                 "mroonga: repair: unknown operation type: <%s>",
                 GRN_TEXT_VALUE(&text_buffer_));
        my_message(error, error_message, MYF(0));
        break;
      }
    }
    grn_table_cursor_close(ctx_, cursor);

    DBUG_RETURN(error);
  }

  void Operations::clear(const char *table_name, size_t table_name_size) {
    MRN_DBUG_ENTER_METHOD();

    grn_table_cursor *cursor;
    cursor = grn_table_cursor_open(ctx_, table_, NULL, 0, NULL, 0, 0, -1, 0);
    if (!cursor) {
      DBUG_VOID_RETURN;
    }

    grn_id id;
    while ((id = grn_table_cursor_next(ctx_, cursor))) {
      GRN_BULK_REWIND(&text_buffer_);
      grn_obj_get_value(ctx_, columns_.table_, id, &text_buffer_);
      if ((static_cast<size_t>(GRN_TEXT_LEN(&text_buffer_)) ==
           table_name_size) &&
          memcmp(GRN_TEXT_VALUE(&text_buffer_),
                 table_name,
                 table_name_size) == 0) {
        grn_table_cursor_delete(ctx_, cursor);
      }
    }
    grn_table_cursor_close(ctx_, cursor);

    DBUG_VOID_RETURN;
  }
}