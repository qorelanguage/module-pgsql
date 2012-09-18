/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QorePGConnection.h
  
  Qore Programming Language

  Copyright 2003 - 2011 David Nichols

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

#ifndef _QORE_QOREPGCONNECTION_H
#define _QORE_QOREPGCONNECTION_H

#include <qore/safe_dslist>

// necessary in order to avoid conflicts with qore's int64 type
#define HAVE_INT64

// undefine package constants to avoid extraneous warnings
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include <postgres_ext.h>         // for most basic types                                            
#ifdef POSTGRESQL_SERVER_INCLUDES
#include <utils/nabstime.h>   // for abstime (AbsoluteTime), reltime (RelativeTime), tinterval (TimeInterval)
#include <utils/date.h>       // for date (DateADT)
#include <utils/timestamp.h>  // for interval (Interval*)
#include <storage/itemptr.h>  // for tid (ItemPointer)
#include <utils/date.h>       // for time (TimeADT), time with time zone (TimeTzADT), 
#include <utils/timestamp.h>  // for timestamp (Timestamp*)
#include <utils/numeric.h>
#include <utils/inet.h>
#include <utils/geo_decls.h>  // for point, etc
#include <catalog/pg_type.h>
#include <pgtypes_numeric.h>
#else
// the following definitions are taken from PostgreSQL server header files
#define BOOLOID			16
#define BYTEAOID		17
#define CHAROID			18
#define NAMEOID			19
#define INT8OID			20
#define INT2OID			21
#define INT2VECTOROID	        22
#define INT4OID			23
#define REGPROCOID		24
#define TEXTOID			25
#define OIDOID			26
#define TIDOID		        27
#define XIDOID                  28
#define CIDOID                  29
#define OIDVECTOROID	        30
#define XMLOID                  142
#define POINTOID		600
#define LSEGOID			601
#define PATHOID			602
#define BOXOID			603
#define POLYGONOID		604
#define LINEOID			628
#define FLOAT4OID               700
#define FLOAT8OID               701
#define ABSTIMEOID		702
#define RELTIMEOID		703
#define TINTERVALOID	        704
#define UNKNOWNOID		705
#define CIRCLEOID		718
#define CASHOID                 790
#define MACADDROID              829
#define INETOID                 869
#define CIDROID                 650
#define INT4ARRAYOID		1007
#define TEXTARRAYOID		1009
#define FLOAT4ARRAYOID          1021
#define ACLITEMOID		1033
#define CSTRINGARRAYOID		1263
#define BPCHAROID		1042
#define VARCHAROID		1043
#define DATEOID			1082
#define TIMEOID			1083
#define TIMESTAMPOID	        1114
#define TIMESTAMPTZOID	        1184
#define INTERVALOID		1186
#define TIMETZOID		1266
#define BITOID	                1560
#define VARBITOID	        1562
#define NUMERICOID		1700
#define REFCURSOROID	        1790
#define REGPROCEDUREOID         2202
#define REGOPEROID		2203
#define REGOPERATOROID	        2204
#define REGCLASSOID		2205
#define REGTYPEOID		2206
#define REGTYPEARRAYOID         2211
#define TSVECTOROID		3614
#define GTSVECTOROID	        3642
#define TSQUERYOID		3615
#define REGCONFIGOID            3734
#define REGDICTIONARYOID	3769
#define RECORDOID		2249
#define RECORDARRAYOID	        2287
#define CSTRINGOID		2275
#define ANYOID			2276
#define ANYARRAYOID		2277
#define VOIDOID			2278
#define TRIGGEROID		2279
#define LANGUAGE_HANDLEROID	2280
#define INTERNALOID		2281
#define OPAQUEOID		2282
#define ANYELEMENTOID	        2283
#define ANYNONARRAYOID	        2776
#define ANYENUMOID		3500

typedef struct {
   double x, y;
} Point;

#define MAXDIM 6

typedef signed int int32;

typedef int32 AbsoluteTime;
typedef int32 RelativeTime;

typedef struct {
   int32 status;
   AbsoluteTime data[2];
} TimeIntervalData;

typedef TimeIntervalData *TimeInterval;

#define PGSQL_AF_INET	(AF_INET + 0)
#define PGSQL_AF_INET6	(AF_INET + 1)

typedef struct {
   Point p[2];
   double m;
} LSEG;

typedef struct {
   Point high, low;
} BOX;

typedef struct {
   Point center;
   double radius;
} CIRCLE;

typedef unsigned char NumericDigit;
#endif

#include <libpq-fe.h>

#include <map>

// missing defines for array types (from catalog/pg_type.h)
#define QPGT_CIRCLEARRAYOID       719
#define QPGT_MONEYARRAYOID        791
#define QPGT_BOOLARRAYOID         1000
#define QPGT_BYTEAARRAYOID        1001
#define QPGT_NAMEARRAYOID         1003
#define QPGT_INT2ARRAYOID         1005
#define QPGT_INT2VECTORARRAYOID   1006 
#ifdef INT4ARRAYOID
#define QPGT_INT4ARRAYOID         INT4ARRAYOID
#else
#define QPGT_INT4ARRAYOID         1007
#endif
#define QPGT_REGPROCARRAYOID      1008 
#define QPGT_TEXTARRAYOID         1009
#define QPGT_OIDARRAYOID          1028
#define QPGT_TIDARRAYOID          1010
#define QPGT_XIDARRAYOID          1011
#define QPGT_CIDARRAYOID          1012
#define QPGT_OIDVECTORARRAYOID    1013
#define QPGT_BPCHARARRAYOID       1014
#define QPGT_VARCHARARRAYOID      1015
#define QPGT_INT8ARRAYOID         1016
#define QPGT_POINTARRAYOID        1017
#define QPGT_LSEGARRAYOID         1018
#define QPGT_PATHARRAYOID         1019
#define QPGT_BOXARRAYOID          1020
#define QPGT_FLOAT4ARRAYOID       1021
#define QPGT_FLOAT8ARRAYOID       1022
#define QPGT_ABSTIMEARRAYOID      1023
#define QPGT_RELTIMEARRAYOID      1024
#define QPGT_TINTERVALARRAYOID    1025
#define QPGT_POLYGONARRAYOID      1027
#define QPGT_ACLITEMARRAYOID      1034
#define QPGT_MACADDRARRAYOID      1040
#define QPGT_INETARRAYOID         1041
#define QPGT_CIDRARRAYOID         651
#define QPGT_TIMESTAMPARRAYOID    1115
#define QPGT_DATEARRAYOID         1182
#define QPGT_TIMEARRAYOID         1183
#define QPGT_TIMESTAMPTZARRAYOID  1185 
#define QPGT_INTERVALARRAYOID     1187
#define QPGT_NUMERICARRAYOID      1231
#define QPGT_TIMETZARRAYOID       1270
#define QPGT_BITARRAYOID          1561
#define QPGT_VARBITARRAYOID       1563
#define QPGT_REFCURSORARRAYOID    2201
#define QPGT_REGPROCEDUREARRAYOID 2207
#define QPGT_REGOPERARRAYOID      2208
#define QPGT_REGOPERATORARRAYOID  2209
#define QPGT_REGCLASSARRAYOID     2210
#define QPGT_REGTYPEARRAYOID      2211
#define QPGT_ANYARRAYOID          2277

// NOTE: this seems to be the binary format for inet/cidr data from PGSQL
// however I can't find this definition anywhere in the header files!!!
// server/utils/inet.h has a different definition
struct qore_pg_inet_struct {
   unsigned char family;      // PGSQL_AF_INET or PGSQL_AF_INET6
   unsigned char bits;        // number of bits in netmask
   unsigned char type;        // 0 = inet, 1 = cidr */
   unsigned char length;      // length of the following field (NOTE: I added this field)
   unsigned char ipaddr[16];  // up to 128 bits of address
};

struct qore_pg_tuple_id {
   unsigned int block;
   unsigned short index;
};

struct qore_pg_bit {
   unsigned int size;
   unsigned char data[1];
};

struct qore_pg_tinterval {
   int status, t1, t2;
};

struct qore_pg_array_info {
   int dim;
   int lBound;
};

struct qore_pg_array_header {
   int ndim, flags, oid;
   struct qore_pg_array_info info[1];
};

struct qore_pg_numeric_base {
   short ndigits;
   short weight;
   short sign;
   short dscale;

   DLLLOCAL qore_pg_numeric_base() : ndigits(0), weight(0), sign(0), dscale(0) {
   }
};

struct qore_pg_numeric : public qore_pg_numeric_base {
   unsigned short digits[1];

   DLLLOCAL void convertToHost() {
      ndigits = ntohs(ndigits);
      weight = ntohs(weight);
      sign = ntohs(sign);
      dscale = ntohs(dscale);

#ifdef DEBUG
      printd(0, "qpg_data_numeric::convertToHost() %d ndigits: %hd, weight: %hd, sign: %hd, dscale: %hd\n", sizeof(NumericDigit), ndigits, weight, sign, dscale);
      for (unsigned i = 0; i < ndigits; ++i)
         printd(0, " + %hu\n", ntohs(digits[i]));
#endif
   }

   DLLLOCAL void toStr(QoreString& str) const {
      if (!ndigits) {
         str.concat('0');
         return;
      }

      if (sign)
         str.concat('-');
   
      int i;
      for (i = 0; i < ndigits; ++i) {
         if (i == weight + 1)
            str.concat('.');
         if (i)
            str.sprintf("%04d", ntohs(digits[i]));
         else
            str.sprintf("%d", ntohs(digits[i]));
         //printd(5, "qore_pg_numeric::toStr() digit %d: %d\n", i, ntohs(nd.digits[i]));
      }

      //printd(0, "qore_pg_numeric::toStr() i: %d weight: %d\n", i, weight);

      // now add significant zeros for remaining decimal places
      if (weight >= i)
         str.addch('0', (weight - i + 1) * 4);
   }
};

#define QORE_MAX_DIGITS 50
struct qore_pg_numeric_out : public qore_pg_numeric_base {
   unsigned short digits[QORE_MAX_DIGITS];

   DLLLOCAL void convertToNet() {
      printd(0, "qore_pg_numeric_out::convertToNet() ndigits: %hd weight: %hd sign: %hd dscale: %hd\n", ndigits, weight, sign, dscale);
      assert(ndigits < QORE_MAX_DIGITS);
      ndigits = htons(ndigits);
      weight = htons(weight);
      sign = htons(sign);
      dscale = htons(dscale);
      for (unsigned i = 0; i < ndigits; ++i)
         digits[i] = htons(digits[i]);
   }
};

union qore_pg_time {
   int64 i;
   double f;
};

struct qore_pg_interval {
   qore_pg_time time;
   union {
      int month;
      struct {
	 int day;
	 int month;
      } with_day;
   } rest;
};

struct qore_pg_time_tz_adt {
   qore_pg_time time;
   int zone;
};

/*
  typedef void (*qore_pg_bind_func_t)(AbstractQoreNode *v, char *data);
  typedef int (*qore_pg_bind_size_func_t)(AbstractQoreNode *v);

  struct qore_bind_info {
  qore_pg_bind_func func;
  qore_pg_bind_size_func_t size_func;
  int size;
  bool in_place;
  };

  typedef std::map<qore_type_t , qore_bind_info> qore_pg_bind_map;
*/

class QorePGConnection;
typedef AbstractQoreNode *(*qore_pg_data_func_t)(char *data, int type, int size, QorePGConnection *conn, const QoreEncoding *enc);

typedef std::map<int, qore_pg_data_func_t> qore_pg_data_map_t;
typedef std::pair<int, qore_pg_data_func_t> qore_pg_array_data_info_t;
typedef std::map<int, qore_pg_array_data_info_t> qore_pg_array_data_map_t;
typedef std::map<int, int> qore_pg_array_type_map_t;

static inline void assign_point(Point &p, Point *raw) {
   p.x = MSBf8(raw->x);
   p.y = MSBf8(raw->y);
};

class QorePGConnection {
protected:
   PGconn *pc;
   bool interval_has_day, integer_datetimes;

public:
   DLLLOCAL QorePGConnection(const char *str, ExceptionSink *xsink);
   DLLLOCAL ~QorePGConnection();
   DLLLOCAL int setPGEncoding(const char *enc, ExceptionSink *xsink);

   DLLLOCAL int commit(Datasource *ds, ExceptionSink *xsink);
   DLLLOCAL int rollback(Datasource *ds, ExceptionSink *xsink);
   DLLLOCAL QoreListNode* selectRows(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink);
#ifdef _QORE_HAS_DBI_SELECT_ROW
   DLLLOCAL QoreHashNode* selectRow(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink);
#endif
   DLLLOCAL AbstractQoreNode *exec(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink);
#ifdef _QORE_HAS_DBI_EXECRAW
   DLLLOCAL AbstractQoreNode *execRaw(Datasource *ds, const QoreString *qstr, ExceptionSink *xsink);
#endif
   DLLLOCAL int begin_transaction(Datasource *ds, ExceptionSink *xsink);
   DLLLOCAL bool has_interval_day() const { return interval_has_day; }
   DLLLOCAL bool has_integer_datetimes() const { return integer_datetimes; }
   DLLLOCAL int get_server_version() const;

   DLLLOCAL int checkResult(PGresult* res, ExceptionSink* xsink) {
      ExecStatusType rc = PQresultStatus(res);
      if (rc != PGRES_COMMAND_OK && rc != PGRES_TUPLES_OK)
	 return doError(xsink);
      return 0;
   }

   DLLLOCAL int checkClearResult(PGresult*& res, ExceptionSink* xsink) {
      int rc = checkResult(res, xsink);
      if (rc) {
	 PQclear(res);
	 res = 0;
      }
      return rc;
   }

   DLLLOCAL int doError(ExceptionSink *xsink) {
      const char *err = PQerrorMessage(pc);
      const char *e = (!strncmp(err, "ERROR:  ", 8) || !strncmp(err, "FATAL:  ", 8)) ? err + 8 : err;
      QoreStringNode *desc = new QoreStringNode(e);
      desc->chomp();
      xsink->raiseException("DBI:PGSQL:ERROR", desc);
      return -1;
   }

   DLLLOCAL PGconn* get() const {
      return pc;
   }
};

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

union parambuf {
   bool b;
   short i2;
   int i4;
   int64 i8;
   double f8;
   char* str;
   void* ptr;
   qore_pg_numeric_out* num;
   qore_pg_interval iv;
   
   DLLLOCAL void assign(short i) {
      i2 = htons(i);
   }
   DLLLOCAL void assign(int i) {
      i4 = htonl(i);
   }
   DLLLOCAL void assign(int64 i) {
      i8 = i8MSB(i);
   }
   DLLLOCAL void assign(double f) {
      f8 = f8MSB(f);
   }
};

class parambuf_list_t : public safe_dslist<parambuf *> {
public:
   DLLLOCAL ~parambuf_list_t() {
      std::for_each(begin(), end(), simple_delete<parambuf>());
   }
};

class QorePgsqlStatement {
protected:
   static qore_pg_data_map_t data_map;
   static qore_pg_array_data_map_t array_data_map;

   PGresult* res;
   int nParams, allocated;
   Oid *paramTypes;
   char **paramValues;
   int *paramLengths, *paramFormats;
   int *paramArray;
   parambuf_list_t parambuf_list;
   QorePGConnection *conn;
   const QoreEncoding *enc;

   DLLLOCAL AbstractQoreNode *getNode(int row, int col, ExceptionSink *xsink);
   // returns 0 for OK, -1 for error
   DLLLOCAL int parse(QoreString *str, const QoreListNode *args, ExceptionSink *xsink);
   DLLLOCAL int add(const AbstractQoreNode *v, ExceptionSink *xsink);
   DLLLOCAL QoreListNode* getArray(int type, qore_pg_data_func_t func, char *&array_data, int current, int ndim, int dim[]);
   DLLLOCAL void reset();
   DLLLOCAL QoreHashNode* getSingleRowIntern(ExceptionSink* xsink, int row = 0);

public:
   static qore_pg_array_type_map_t array_type_map;

   DLLLOCAL QorePgsqlStatement(QorePGConnection *r_conn, const QoreEncoding *r_enc);
   DLLLOCAL QorePgsqlStatement(Datasource* ds);
   DLLLOCAL ~QorePgsqlStatement();

   // returns 0 for OK, -1 for error
   DLLLOCAL int exec(PGconn *pc, const QoreString *str, const QoreListNode *args, ExceptionSink *xsink);

   // returns 0 for OK, -1 for error
   DLLLOCAL int exec(PGconn* pc, const char* cmd, ExceptionSink* xsink);
   DLLLOCAL QoreHashNode* getOutputHash(ExceptionSink* xsink, int* start = 0, int maxrows = -1);
   DLLLOCAL QoreListNode* getOutputList(ExceptionSink* xsink, int* start = 0, int maxrows = -1);
   DLLLOCAL QoreHashNode* getSingleRow(ExceptionSink *xsink, int row = 0);
   DLLLOCAL int rowsAffected();
   DLLLOCAL bool hasResultData();
   DLLLOCAL bool checkIntegerDateTimes(PGconn *pc, ExceptionSink *xsink);

   // static functions
   static DLLLOCAL void static_init();
};

#ifdef _QORE_HAS_PREPARED_STATMENT_API
class QorePgsqlPreparedStatement : public QorePgsqlStatement {
protected:
   QoreString* sql;
   QoreListNode* targs;
   // current row
   int crow;
   bool do_parse;


   DLLLOCAL int prepareIntern(const QoreListNode* args, ExceptionSink* xsink);

public:
   DLLLOCAL QorePgsqlPreparedStatement(Datasource* ds) : QorePgsqlStatement(ds), sql(0), targs(0), crow(-1), do_parse(false) {
   }

   DLLLOCAL ~QorePgsqlPreparedStatement() {
      assert(!sql);
   }

   // returns 0 for OK, -1 for error
   DLLLOCAL int prepare(const QoreString& sql, const QoreListNode* args, bool n_parse, ExceptionSink* xsink);
   DLLLOCAL int bind(const QoreListNode& l, ExceptionSink* xsink);
   DLLLOCAL int exec(ExceptionSink* xsink);
   DLLLOCAL QoreHashNode* fetchRow(ExceptionSink* xsink);
   DLLLOCAL QoreListNode* fetchRows(int rows, ExceptionSink* xsink);
   DLLLOCAL QoreHashNode* fetchColumns(int rows, ExceptionSink* xsink);
   DLLLOCAL bool next();

   DLLLOCAL void reset(ExceptionSink *xsink);
};
#endif

class QorePGBindArray {
private:
   int ndim, size, allocated, elements;
   int dim[MAXDIM];
   char *ptr;
   qore_pg_array_header *hdr;
   qore_type_t type;
   int oid, arrayoid, format;
   QorePGConnection *conn;

   // returns -1 for exception, 0 for OK
   DLLLOCAL int check_type(const AbstractQoreNode *n, ExceptionSink *xsink);
   // returns -1 for exception, 0 for OK
   DLLLOCAL int check_oid(const QoreHashNode *h, ExceptionSink *xsink);
   // returns -1 for exception, 0 for OK
   DLLLOCAL int new_dimension(const QoreListNode *l, int current, ExceptionSink *xsink);
   // returns -1 for exception, 0 for OK
   DLLLOCAL int process_list(const QoreListNode *l, int current, const QoreEncoding *enc, ExceptionSink *xsink);
   // returns -1 for exception, 0 for OK
   DLLLOCAL int bind(const AbstractQoreNode *n, const QoreEncoding *enc, ExceptionSink *xsink);
   DLLLOCAL void check_size(int size);

public:
   DLLLOCAL QorePGBindArray(QorePGConnection *r_conn);
   DLLLOCAL ~QorePGBindArray();
   // returns -1 for exception, 0 for OK
   DLLLOCAL int create_data(const QoreListNode *l, int current, const QoreEncoding *enc, ExceptionSink *xsink);
   DLLLOCAL int getOid() const;
   DLLLOCAL int getArrayOid() const;
   DLLLOCAL int getSize() const;
   DLLLOCAL int getFormat() const { return format; }
   DLLLOCAL qore_pg_array_header *getHeader();
};

#endif
