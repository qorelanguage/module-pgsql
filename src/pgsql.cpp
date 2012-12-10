/*
  pgsql.cpp
  
  Qore Programming Language

  Copyright 2003 - 2012 David Nichols

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "pgsql.h"

#include "QorePGConnection.h"
#include "QorePGMapper.h"

#include <libpq-fe.h>

void init_pgsql_functions(QoreNamespace& ns);
void init_pgsql_constants(QoreNamespace& ns);

static QoreStringNode* pgsql_module_init();
static void pgsql_module_ns_init(QoreNamespace* rns, QoreNamespace* qns);
static void pgsql_module_delete();

static QoreNamespace pgsql_ns("PGSQL");

DLLEXPORT char qore_module_name[] = "pgsql";
DLLEXPORT char qore_module_version[] = QORE_MODULE_PACKAGE_VERSION;
DLLEXPORT char qore_module_description[] = "PostgreSQL module";
DLLEXPORT char qore_module_author[] = "David Nichols";
DLLEXPORT char qore_module_url[] = "http://qore.org";
DLLEXPORT int qore_module_api_major = QORE_MODULE_API_MAJOR;
DLLEXPORT int qore_module_api_minor = QORE_MODULE_API_MINOR;
DLLEXPORT qore_module_init_t qore_module_init = pgsql_module_init;
DLLEXPORT qore_module_ns_init_t qore_module_ns_init = pgsql_module_ns_init;
DLLEXPORT qore_module_delete_t qore_module_delete = pgsql_module_delete;
DLLEXPORT qore_license_t qore_module_license = QL_LGPL;

static int pgsql_caps = DBI_CAP_TRANSACTION_MANAGEMENT 
   | DBI_CAP_CHARSET_SUPPORT
   | DBI_CAP_STORED_PROCEDURES 
   | DBI_CAP_LOB_SUPPORT
   | DBI_CAP_BIND_BY_VALUE
#ifdef _QORE_HAS_DBI_EXECRAW
   | DBI_CAP_HAS_EXECRAW
#endif
#ifdef _QORE_HAS_TIME_ZONES
   | DBI_CAP_TIME_ZONE_SUPPORT
#endif
#ifdef _QORE_HAS_NUMBER_TYPE
   | DBI_CAP_HAS_NUMBER_SUPPORT
#endif
#ifdef _QORE_HAS_FIND_CREATE_TIMEZONE
   |DBI_CAP_SERVER_TIME_ZONE
#endif
#ifdef DBI_CAP_AUTORECONNECT
   |DBI_CAP_AUTORECONNECT
#endif
;

DBIDriver *DBID_PGSQL = NULL;

static int qore_pgsql_commit(Datasource* ds, ExceptionSink* xsink) {
   QorePGConnection *pc = (QorePGConnection *)ds->getPrivateData();

   return pc->commit(xsink);
}

static int qore_pgsql_rollback(Datasource* ds, ExceptionSink* xsink) {
   QorePGConnection *pc = (QorePGConnection *)ds->getPrivateData();

   return pc->rollback(xsink);
}

static int qore_pgsql_begin_transaction(Datasource* ds, ExceptionSink* xsink) {
   QorePGConnection *pc = (QorePGConnection *)ds->getPrivateData();

   return pc->begin_transaction(xsink);
}

static AbstractQoreNode* qore_pgsql_select_rows(Datasource* ds, const QoreString *qstr, const QoreListNode* args, ExceptionSink* xsink) {
   QorePGConnection *pc = (QorePGConnection *)ds->getPrivateData();

   return pc->selectRows(qstr, args, xsink);
}

#ifdef _QORE_HAS_DBI_SELECT_ROW
static QoreHashNode* qore_pgsql_select_row(Datasource* ds, const QoreString *qstr, const QoreListNode* args, ExceptionSink* xsink) {
   QorePGConnection *pc = (QorePGConnection *)ds->getPrivateData();

   return pc->selectRow(qstr, args, xsink);
}
#endif

static AbstractQoreNode* qore_pgsql_exec(Datasource* ds, const QoreString *qstr, const QoreListNode* args, ExceptionSink* xsink) {
   QorePGConnection *pc = (QorePGConnection *)ds->getPrivateData();

   return pc->exec(qstr, args, xsink);
}

#ifdef _QORE_HAS_DBI_EXECRAW
static AbstractQoreNode* qore_pgsql_execRaw(Datasource* ds, const QoreString *qstr, ExceptionSink* xsink) {
   QorePGConnection *pc = (QorePGConnection *)ds->getPrivateData();
   return pc->execRaw(qstr, xsink);
}
#endif

static int qore_pgsql_open(Datasource* ds, ExceptionSink* xsink) {
   printd(5, "qore_pgsql_open() datasource %08p for DB=%s\n", ds, ds->getDBName() ? ds->getDBName() : "unknown");

   // string for connection arguments
   QoreString lstr;
   if (ds->getUsername())
      lstr.sprintf("user='%s' ", ds->getUsername());

   if (ds->getPassword())
      lstr.sprintf("password='%s' ", ds->getPassword());

   if (ds->getDBName())
      lstr.sprintf("dbname='%s' ", ds->getDBName());
   
   if (ds->getHostName())
      lstr.sprintf("host='%s' ", ds->getHostName());

#ifdef QORE_HAS_DATASOURCE_PORT
   if (ds->getPort())
      lstr.sprintf("port=%d ", ds->getPort());
#endif

   if (ds->getDBEncoding()) {
      const QoreEncoding *enc = QorePGMapper::getQoreEncoding(ds->getDBEncoding());
      ds->setQoreEncoding(enc);
   }
   else {
      char *enc = (char *)QorePGMapper::getPGEncoding(QCS_DEFAULT);
      if (!enc) {
         xsink->raiseException("DBI:PGSQL:UNKNOWN-CHARACTER-SET", "cannot find the PostgreSQL character encoding equivalent for '%s'", QCS_DEFAULT->getCode());
         return -1;
      }
      ds->setDBEncoding(enc);
      ds->setQoreEncoding(QCS_DEFAULT);
   }

   lstr.concat("options='-c client_min_messages=error'");

   QorePGConnection *pc = new QorePGConnection(ds, lstr.getBuffer(), xsink);

   if (*xsink) {
      delete pc;
      return -1;
   }

   ds->setPrivateData((void *)pc);
   return 0;
}

static int qore_pgsql_close(Datasource* ds) {
   QorePGConnection *pc = (QorePGConnection *)ds->getPrivateData();

   delete pc;
   ds->setPrivateData(NULL);
   return 0;
}

static AbstractQoreNode* qore_pgsql_get_server_version(Datasource* ds, ExceptionSink* xsink) {
   QorePGConnection *pc = (QorePGConnection *)ds->getPrivateData();
   return new QoreBigIntNode(pc->get_server_version());
}

static AbstractQoreNode* qore_pgsql_get_client_version(const Datasource* ds, ExceptionSink* xsink) {
#ifdef HAVE_PQLIBVERSION
   return new QoreBigIntNode(PQlibVersion());
#else
   xsink->raiseException("DBI:PGSQL-GET-CLIENT-VERSION-ERROR", "the version of the PostgreSQL client library that this module was compiled against did not support the PQlibVersion() function");
   return 0;
#endif
}

#ifdef _QORE_HAS_PREPARED_STATMENT_API
static int pgsql_stmt_prepare(SQLStatement* stmt, const QoreString& str, const QoreListNode* args, ExceptionSink* xsink) {
   assert(!stmt->getPrivateData());

   QorePgsqlPreparedStatement* bg = new QorePgsqlPreparedStatement(stmt->getDatasource());
   stmt->setPrivateData(bg);

   return bg->prepare(str, args, true, xsink);
}

static int pgsql_stmt_prepare_raw(SQLStatement* stmt, const QoreString& str, ExceptionSink* xsink) {
   assert(!stmt->getPrivateData());

   QorePgsqlPreparedStatement* bg = new QorePgsqlPreparedStatement(stmt->getDatasource());
   stmt->setPrivateData(bg);

   return bg->prepare(str, 0, false, xsink);
}

static int pgsql_stmt_bind(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink) {
   QorePgsqlPreparedStatement* bg = (QorePgsqlPreparedStatement* )stmt->getPrivateData();
   assert(bg);

   return bg->bind(l, xsink);
}

static int pgsql_stmt_bind_placeholders(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink) {
   xsink->raiseException("DBI:PGSQL-BIND-PLACEHHODERS-ERROR", "binding placeholders is not necessary or supported with the pgsql driver");
   return -1;
}

static int pgsql_stmt_bind_values(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink) {
   QorePgsqlPreparedStatement* bg = (QorePgsqlPreparedStatement* )stmt->getPrivateData();
   assert(bg);

   return bg->bind(l, xsink);
}

static int pgsql_stmt_exec(SQLStatement* stmt, ExceptionSink* xsink) {
   QorePgsqlPreparedStatement* bg = (QorePgsqlPreparedStatement* )stmt->getPrivateData();
   assert(bg);

   return bg->exec(xsink);
}

static int pgsql_stmt_define(SQLStatement* stmt, ExceptionSink* xsink) {
   // define is a noop in the pgsql driver
   return 0;
}

static int pgsql_stmt_affected_rows(SQLStatement* stmt, ExceptionSink* xsink) {
   QorePgsqlPreparedStatement* bg = (QorePgsqlPreparedStatement*)stmt->getPrivateData();
   assert(bg);

   return bg->rowsAffected();
}

static QoreHashNode* pgsql_stmt_get_output(SQLStatement* stmt, ExceptionSink* xsink) {
   QorePgsqlPreparedStatement* bg = (QorePgsqlPreparedStatement*)stmt->getPrivateData();
   assert(bg);

   return bg->getOutputHash(xsink);
}

static QoreHashNode* pgsql_stmt_get_output_rows(SQLStatement* stmt, ExceptionSink* xsink) {
   QorePgsqlPreparedStatement* bg = (QorePgsqlPreparedStatement*)stmt->getPrivateData();
   assert(bg);

   return bg->getOutputHash(xsink);
}

static QoreHashNode* pgsql_stmt_fetch_row(SQLStatement* stmt, ExceptionSink* xsink) {
   QorePgsqlPreparedStatement* bg = (QorePgsqlPreparedStatement*)stmt->getPrivateData();
   assert(bg);

   return bg->fetchRow(xsink);
}

static QoreListNode* pgsql_stmt_fetch_rows(SQLStatement* stmt, int rows, ExceptionSink* xsink) {
   QorePgsqlPreparedStatement* bg = (QorePgsqlPreparedStatement*)stmt->getPrivateData();
   assert(bg);

   return bg->fetchRows(rows, xsink);
}

static QoreHashNode* pgsql_stmt_fetch_columns(SQLStatement* stmt, int rows, ExceptionSink* xsink) {
   QorePgsqlPreparedStatement* bg = (QorePgsqlPreparedStatement*)stmt->getPrivateData();
   assert(bg);

   return bg->fetchColumns(rows, xsink);
}

static bool pgsql_stmt_next(SQLStatement* stmt, ExceptionSink* xsink) {
   QorePgsqlPreparedStatement* bg = (QorePgsqlPreparedStatement*)stmt->getPrivateData();
   assert(bg);

   return bg->next();
}

static int pgsql_stmt_close(SQLStatement* stmt, ExceptionSink* xsink) {
   QorePgsqlPreparedStatement* bg = (QorePgsqlPreparedStatement*)stmt->getPrivateData();
   assert(bg);

   bg->reset(xsink);
   delete bg;
   stmt->setPrivateData(0);
   return *xsink ? -1 : 0;
}
#endif // _QORE_HAS_PREPARED_STATMENT_API

#ifdef _QORE_HAS_DBI_OPTIONS
static int pgsql_opt_set(Datasource* ds, const char* opt, const AbstractQoreNode* val, ExceptionSink* xsink) {
   QorePGConnection* pc = (QorePGConnection*)ds->getPrivateData();
   return pc->setOption(opt, val, xsink);
}

static AbstractQoreNode* pgsql_opt_get(const Datasource* ds, const char* opt) {
   QorePGConnection* pc = (QorePGConnection*)ds->getPrivateData();
   return pc->getOption(opt);
}
#endif

static QoreStringNode* pgsql_module_init() {
#ifdef HAVE_PQISTHREADSAFE
   if (!PQisthreadsafe())
      return QoreStringNode("cannot load pgsql module; the PostgreSQL library on this system is not thread-safe");
#endif

   init_pgsql_functions(pgsql_ns);
   init_pgsql_constants(pgsql_ns);

   QorePGMapper::static_init();
   QorePgsqlStatement::static_init();

   // register database functions with DBI subsystem
   qore_dbi_method_list methods;
   methods.add(QDBI_METHOD_OPEN, qore_pgsql_open);
   methods.add(QDBI_METHOD_CLOSE, qore_pgsql_close);
   methods.add(QDBI_METHOD_SELECT, qore_pgsql_exec);
   methods.add(QDBI_METHOD_SELECT_ROWS, qore_pgsql_select_rows);
#ifdef _QORE_HAS_DBI_SELECT_ROW
   methods.add(QDBI_METHOD_SELECT_ROW, qore_pgsql_select_row);
#endif
   methods.add(QDBI_METHOD_EXEC, qore_pgsql_exec);
#ifdef _QORE_HAS_DBI_EXECRAW
   methods.add(QDBI_METHOD_EXECRAW, qore_pgsql_execRaw);
#endif
   methods.add(QDBI_METHOD_COMMIT, qore_pgsql_commit);
   methods.add(QDBI_METHOD_ROLLBACK, qore_pgsql_rollback);
   methods.add(QDBI_METHOD_BEGIN_TRANSACTION, qore_pgsql_begin_transaction);
   methods.add(QDBI_METHOD_ABORT_TRANSACTION_START, qore_pgsql_rollback);
   methods.add(QDBI_METHOD_GET_SERVER_VERSION, qore_pgsql_get_server_version);
   methods.add(QDBI_METHOD_GET_CLIENT_VERSION, qore_pgsql_get_client_version);

#ifdef _QORE_HAS_PREPARED_STATMENT_API
   methods.add(QDBI_METHOD_STMT_PREPARE, pgsql_stmt_prepare);
   methods.add(QDBI_METHOD_STMT_PREPARE_RAW, pgsql_stmt_prepare_raw);
   methods.add(QDBI_METHOD_STMT_BIND, pgsql_stmt_bind);
   methods.add(QDBI_METHOD_STMT_BIND_PLACEHOLDERS, pgsql_stmt_bind_placeholders);
   methods.add(QDBI_METHOD_STMT_BIND_VALUES, pgsql_stmt_bind_values);
   methods.add(QDBI_METHOD_STMT_EXEC, pgsql_stmt_exec);
   methods.add(QDBI_METHOD_STMT_DEFINE, pgsql_stmt_define);
   methods.add(QDBI_METHOD_STMT_FETCH_ROW, pgsql_stmt_fetch_row);
   methods.add(QDBI_METHOD_STMT_FETCH_ROWS, pgsql_stmt_fetch_rows);
   methods.add(QDBI_METHOD_STMT_FETCH_COLUMNS, pgsql_stmt_fetch_columns);
   methods.add(QDBI_METHOD_STMT_NEXT, pgsql_stmt_next);
   methods.add(QDBI_METHOD_STMT_CLOSE, pgsql_stmt_close);
   methods.add(QDBI_METHOD_STMT_AFFECTED_ROWS, pgsql_stmt_affected_rows);
   methods.add(QDBI_METHOD_STMT_GET_OUTPUT, pgsql_stmt_get_output);
   methods.add(QDBI_METHOD_STMT_GET_OUTPUT_ROWS, pgsql_stmt_get_output_rows);
#endif // _QORE_HAS_PREPARED_STATMENT_API

#ifdef _QORE_HAS_DBI_OPTIONS
   methods.add(QDBI_METHOD_OPT_SET, pgsql_opt_set);
   methods.add(QDBI_METHOD_OPT_GET, pgsql_opt_get);

   methods.registerOption(DBI_OPT_NUMBER_OPT, "when set, numeric/decimal values are returned as integers if possible, otherwise as arbitrary-precision number values; the argument is ignored; setting this option turns it on and turns off 'string-numbers' and 'numeric-numbers'");
   methods.registerOption(DBI_OPT_NUMBER_STRING, "when set, numeric/decimal values are returned as strings for backwards-compatibility; the argument is ignored; setting this option turns it on and turns off 'optimal-numbers' and 'numeric-numbers'");
   methods.registerOption(DBI_OPT_NUMBER_NUMERIC, "when set, numeric/decimal values are returned as arbitrary-precision number values; the argument is ignored; setting this option turns it on and turns off 'string-numbers' and 'optimal-numbers'");
   methods.registerOption(DBI_OPT_TIMEZONE, "set the server-side timezone, value must be a string in the format accepted by Timezone::constructor() on the client (ie either a region name or a UTC offset like \"+01:00\"), if not set the server's time zone will be assumed to be the same as the client's", stringTypeInfo);
#endif

   DBID_PGSQL = DBI.registerDriver("pgsql", methods, pgsql_caps);

   return 0;
}

static void pgsql_module_ns_init(QoreNamespace* rns, QoreNamespace* qns) {
   qns->addInitialNamespace(pgsql_ns.copy());
}

static void pgsql_module_delete() {
}
