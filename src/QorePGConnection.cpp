/*
  QorePGConnection.cpp

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

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
#include <winsock2.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <memory>
#include <typeinfo>

// postgresql uses an epoch starting at 2000-01-01, which is
// 10,957 days after the UNIX and Qore epoch of 1970-01-01
// there are 86,400 seconds in the average day (w/o DST changes)
#define PGSQL_EPOCH_OFFSET (10957 * 86400)

// declare static members
qore_pg_data_map_t QorePgsqlStatement::data_map;
qore_pg_array_data_map_t QorePgsqlStatement::array_data_map;
qore_pg_array_type_map_t QorePgsqlStatement::array_type_map;

// bind functions
static AbstractQoreNode *qpg_data_bool(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   return get_bool_node(*((bool *)data));
}

static AbstractQoreNode *qpg_data_bytea(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   void *dc = malloc(len);
   memcpy(dc, data, len);
   return new BinaryNode(dc, len);
}

static AbstractQoreNode *qpg_data_char(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   char *nstr = (char *)malloc(sizeof(char) * (len + 1));
   strncpy(nstr, (char *)data, len);
   nstr[len] = '\0';
   remove_trailing_blanks(nstr);
   return new QoreStringNode(nstr, len, len + 1, enc);
}

static AbstractQoreNode *qpg_data_int8(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   return new QoreBigIntNode(MSBi8(*((uint64_t *)data)));
}

static AbstractQoreNode *qpg_data_int4(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   return new QoreBigIntNode(ntohl(*((uint32_t *)data)));
}

static AbstractQoreNode *qpg_data_int2(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   return new QoreBigIntNode(ntohs(*((uint16_t *)data)));
}

static AbstractQoreNode *qpg_data_text(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   return new QoreStringNode((char *)data, len, enc);
}

static AbstractQoreNode *qpg_data_float4(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   float fv = MSBf4(*((float *)data));
   return new QoreFloatNode((double)fv);
}

static AbstractQoreNode *qpg_data_float8(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   double fv = MSBf8(*((double *)data));
   return new QoreFloatNode(fv);
}

static AbstractQoreNode *qpg_data_abstime(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   int val = ntohl(*((uint32_t *)data));
   return new DateTimeNode((int64)val);
}

static AbstractQoreNode *qpg_data_reltime(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   int val = ntohl(*((uint32_t *)data));
   return new DateTimeNode(0, 0, 0, 0, 0, val, 0, true);
}

#ifdef _QORE_HAS_TIME_ZONES
static AbstractQoreNode *qpg_data_timestamptz(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   if (conn->has_integer_datetimes()) {
      int64 val = MSBi8(*((uint64_t *)data));
      int64 secs = val / 1000000;
      int us = val % 1000000;
      secs += PGSQL_EPOCH_OFFSET;
      return DateTimeNode::makeAbsolute(currentTZ(), secs, us);
   }
   double fv = MSBf8(*((double *)data));
   int64 nv = (int64)fv;
   int us = (int)((fv - (double)nv) * 1000000.0);
   nv += PGSQL_EPOCH_OFFSET;
   return DateTimeNode::makeAbsolute(currentTZ(), nv, us);
}
#endif

static AbstractQoreNode *qpg_data_timestamp(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   if (conn->has_integer_datetimes()) {
      int64 val = MSBi8(*((uint64_t *)data));
   
      // convert from u-secs to seconds and milliseconds
      int64 secs = val / 1000000;
#ifdef _QORE_HAS_TIME_ZONES
      int us = val % 1000000;
      secs += PGSQL_EPOCH_OFFSET;
      return DateTimeNode::makeAbsoluteLocal(currentTZ(), secs, us);
#else
      int ms = val / 1000 - secs * 1000;
      secs += PGSQL_EPOCH_OFFSET;
      return new DateTimeNode(secs, ms);
#endif
   }
   double fv = MSBf8(*((double *)data));
   int64 nv = (int64)fv;
#ifdef _QORE_HAS_TIME_ZONES
   int us = (int)((fv - (double)nv) * 1000000.0);
   nv += PGSQL_EPOCH_OFFSET;
   return DateTimeNode::makeAbsolute(currentTZ(), nv, us);
#else
   int ms = (int)((fv - (double)nv) * 1000.0);
   nv += PGSQL_EPOCH_OFFSET;
   //printd(5, "time=%lld.%03d seconds\n", val, ms);
   return new DateTimeNode(nv, ms);
#endif
}

static AbstractQoreNode *qpg_data_date(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   int val = (ntohl(*((uint32_t *)data)) + 10957) * 86400;      
   return new DateTimeNode((int64)val);
}

static AbstractQoreNode *qpg_data_interval(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   int64 secs;

   qore_pg_interval *iv = (qore_pg_interval *)data;
#ifdef _QORE_HAS_TIME_ZONES
   // create interval with microsecond resolution
   int64 us;
   if (conn->has_integer_datetimes()) {
      us = MSBi8(iv->time.i);
      secs = us / 1000000;
      us %= 1000000;
      //printd(5, "got interval %lld: secs=%lld us=%lld (day: %d)\n", MSBi8(iv->time.i), secs, us, conn->has_interval_day());
   }
   else {
      double f = MSBf8(iv->time.f);
      secs = (int64)f;
      us = (int)((f - (double)secs) * 1000000.0);   
   }
   if (conn->has_interval_day())
      return DateTimeNode::makeRelative(0, ntohl(iv->rest.with_day.month), ntohl(iv->rest.with_day.day), 0, 0, secs, us);
   else
      return DateTimeNode::makeRelative(0, ntohl(iv->rest.month), 0, 0, 0, secs, us);
#else
   // create interval with millisecond resolution
   int ms;
   if (conn->has_integer_datetimes()) {
      int64 us = MSBi8(iv->time.i);
      secs = us / 1000000;
      ms = us / 1000 - (secs * 1000);
   }
   else {
      double f = MSBf8(iv->time.f);
      secs = (int64)f;
      ms = (int)((f - (double)secs) * 1000.0);   
   }
   if (conn->has_interval_day())
      return new DateTimeNode(0, ntohl(iv->rest.with_day.month), ntohl(iv->rest.with_day.day), 0, 0, secs, ms, true);
   else
      return new DateTimeNode(0, ntohl(iv->rest.month), 0, 0, 0, secs, ms, true);
#endif
}

static AbstractQoreNode *qpg_data_time(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   int64 secs;
#ifdef _QORE_HAS_TIME_ZONES
   int us;
   if (conn->has_integer_datetimes()) {
      int64 val = MSBi8(*((uint64_t *)data));
      secs = val / 1000000;
      us = val % 1000000;
   }
   else {
      double val = MSBf8(*((double *)data));
      secs = (int64)val;
      us = (int)((val - (double)secs) * 1000000.0);
   }
   //printd(5, "qpg_data_time() %lld.%06d\n", secs, us);
   // create the date/time value from an offset in the current time zone
   return DateTimeNode::makeAbsoluteLocal(currentTZ(), secs, us);
#else
   int ms;
   if (conn->has_integer_datetimes()) {
      int64 val = MSBi8(*((uint64_t *)data));
      secs = val / 1000000;
      ms = val / 1000 - secs * 1000;
   }
   else {
      double val = MSBf8(*((double *)data));
      secs = (int64)val;
      ms = (int)((val - (double)secs) * 1000.0);
   }
   //printd(5, "qpg_data_time() %lld.%03d\n", secs, ms);
   return new DateTimeNode(secs, ms);
#endif
}

static AbstractQoreNode *qpg_data_timetz(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   qore_pg_time_tz_adt *tm = (qore_pg_time_tz_adt *)data;
   int64 secs;
#ifdef _QORE_HAS_TIME_ZONES
   // postgresql gives the time zone in seconds west of UTC
   int zone = ntohl(tm->zone);

   int us;
   if (conn->has_integer_datetimes()) {
      int64 val = MSBi8(tm->time.i);
      secs = val / 1000000;
      us = val % 1000000;
   }
   else {
      double val = MSBf8(tm->time.f);
      secs = (int64)val;
      us = (int)((val - (double)secs) * 1000000.0);
   }
   //printd(5, "zone=%d secs=%lld.%06d\n", zone, secs, us);

   return DateTimeNode::makeAbsoluteLocal(findCreateOffsetZone(-zone), secs, us);
#else
   // NOTE! timezone is ignored when qore has no time zone support
   int ms;
   if (conn->has_integer_datetimes()) {
      int64 val = MSBi8(tm->time.i);
      secs = val / 1000000;
      ms = val / 1000 - secs * 1000;
   }
   else {
      double val = MSBf8(tm->time.f);
      secs = (int64)val;
      ms = (int)((val - (double)secs) * 1000.0);
   }
   return new DateTimeNode(secs, ms);
#endif
}

static AbstractQoreNode *qpg_data_tinterval(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   //printd(5, "QorePgsqlStatement::getNode(row=%d, col=%d, type=%d) this=%p len=%d\n", row, col, type, this, len);
   TimeIntervalData *td = (TimeIntervalData *)data;

   DateTime dt((int64)(int)ntohl(td->data[0]));
   QoreStringNode *str = new QoreStringNode();
   str->sprintf("[\"%04d-%02d-%02d %02d:%02d:%02d\" ", dt.getYear(), dt.getMonth(), dt.getDay(), dt.getHour(), dt.getMinute(), dt.getSecond()); 
   dt.setDate((int64)ntohl(td->data[1]));
   str->sprintf("\"%04d-%02d-%02d %02d:%02d:%02d\"]", dt.getYear(), dt.getMonth(), dt.getDay(), dt.getHour(), dt.getMinute(), dt.getSecond()); 

   // NOTE: ignoring tinverval->status, it is assumed that any value here will be valid
   //printd(5, "status=%d s0=%d s1=%d\n", ntohl(td->status), s0, s1);

   return str;
}

static AbstractQoreNode *qpg_data_numeric(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   // note: we write directly to the data here
   qore_pg_numeric *nd = (qore_pg_numeric *)data;
   nd->ndigits = ntohs(nd->ndigits);
   nd->weight = ntohs(nd->weight);
   nd->sign = ntohs(nd->sign);
   nd->dscale = ntohs(nd->dscale);

   printd(5, "(%d) ndigits=%d, weight=%d, sign=%d, dscale=%d\n", sizeof(NumericDigit), nd->ndigits, nd->weight, nd->sign, nd->dscale);
   QoreStringNode *str = new QoreStringNode();
   if (!nd->ndigits)
      str->concat('0');
   else {
      if (nd->sign < 0)
	 str->concat('-');

      int i;
      for (i = 0; i < nd->ndigits; i++) {
	 if (i == nd->weight + 1)
	    str->concat('.');
	 if (i)
	    str->sprintf("%04d", ntohs(nd->digits[i]));
	 else
	    str->sprintf("%d", ntohs(nd->digits[i]));
	 printd(5, "digit %d: %d\n", i, ntohs(nd->digits[i]));
      }
      
      // now add significant zeros for remainaing decimal places
      while (i++ <= nd->weight)
	 str->concat("0000");
   }
   printd(5, "************ returning %s\n", str->getBuffer());
   return str;
}

static AbstractQoreNode *qpg_data_cash(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   double f = (double)ntohl(*((uint32_t *)data)) / 100.0;
   //printd(5, "qpg_data_cash() f=%g\n", f);
   return new QoreFloatNode(f);
}

static AbstractQoreNode *qpg_data_macaddr(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   QoreStringNode *str = new QoreStringNode;
   for (int i = 0; i < 5; i++) {
      str->concatHex((char *)data + i, 1);
      str->concat(':');
   }
   str->concatHex((char *)data+5, 1);
   return str;
}

static AbstractQoreNode *qpg_data_inet(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   qore_pg_inet_struct *is = (qore_pg_inet_struct *)data;

   QoreStringNode *str = new QoreStringNode();
   if (is->family == PGSQL_AF_INET) {
      for (int i = 0, e = is->length - 1; i < e; i++)
	 str->sprintf("%d.", is->ipaddr[i]);
      str->sprintf("%d/%d", is->ipaddr[3], is->bits);
   }
   else {
      short *sp;
      int i, val, e, last = 0;
      if (type == CIDROID)
	 e = is->bits / 8;
      else
	 e = is->length;
      if (e == 16) {
	 e -= 2;
	 last = 1;
      }
      for (i = 0; i < e; i += 2) {
	 sp = (short *)&is->ipaddr[i];
	 val = ntohs(*sp);
	 str->sprintf("%x:", val);
      }
      if (last) {
	 sp = (short *)&is->ipaddr[i];
	 val = ntohs(*sp);
	 str->sprintf("%x", val);
      }
      else
	 str->concat(':');
      str->sprintf("/%d", is->bits);
   }
   return str;
}

static AbstractQoreNode *qpg_data_tid(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   qore_pg_tuple_id *ti = (qore_pg_tuple_id *)data;
   unsigned block = ntohl(ti->block);
   unsigned index = ntohs(ti->index);
   QoreStringNode *str = new QoreStringNode();
   str->sprintf("(%u,%u)", block, index);
   return str;
}

static AbstractQoreNode *qpg_data_bit(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   qore_pg_bit *bp = (qore_pg_bit *)data;
   int num = (ntohl(bp->size) - 1) / 8 + 1;
   BinaryNode *b = new BinaryNode();
   b->append(bp->data, num);
   return b;
}

static AbstractQoreNode *qpg_data_point(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   Point p;
   assign_point(p, (Point *)data);
   QoreStringNode *str = new QoreStringNode;
   str->sprintf("%g,%g", p.x, p.y);
   return str;
}

static AbstractQoreNode *qpg_data_lseg(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   Point p;
   assign_point(p, &((LSEG *)data)->p[0]);
   QoreStringNode *str = new QoreStringNode;
   str->sprintf("(%g,%g),", p.x, p.y);
   assign_point(p, &((LSEG *)data)->p[1]);
   str->sprintf("(%g,%g)", p.x, p.y);
   return str;
}

// NOTE: This is functionally identical to LSEG above
static AbstractQoreNode *qpg_data_box(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   Point p0, p1;
   assign_point(p0, &((BOX *)data)->high);
   assign_point(p1, &((BOX *)data)->low);
   QoreStringNode *str = new QoreStringNode;
   str->sprintf("(%g,%g),(%g,%g)", p0.x, p0.y, p1.x, p1.y);
   return str;
}

static AbstractQoreNode *qpg_data_path(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   unsigned npts = ntohl(*((int *)((char *)data + 1)));
   bool closed = ntohl(*((char *)data));
   //printd(5, "npts=%d closed=%d\n", npts, closed);
   QoreStringNode *str = new QoreStringNode();
   str->concat(closed ? '(' : '[');
   Point p;
   for (unsigned i = 0; i < npts; ++i) {
      assign_point(p, (Point *)(((char *)data) + 5 + (sizeof(Point)) * i));
      str->sprintf("(%g,%g)", p.x, p.y);
      if (i != (npts - 1))
	 str->concat(',');
   }
   str->concat(closed ? ')' : ']');
   return str;
}

static AbstractQoreNode *qpg_data_polygon(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   unsigned npts = ntohl(*((int *)data));
   QoreStringNode *str = new QoreStringNode('(');
   Point p;
   for (unsigned i = 0; i < npts; ++i) {
      assign_point(p, (Point *)(((char *)data) + 4 + (sizeof(Point)) * i));
      str->sprintf("(%g,%g)", p.x, p.y);
      if (i != (npts - 1))
	 str->concat(',');
   }
   str->concat(')');
   return str;
}

static AbstractQoreNode *qpg_data_circle(char *data, int type, int len, QorePGConnection *conn, const QoreEncoding *enc) {
   //printd(5, "QorePgsqlStatement::getNode(row=%d, col=%d, type=%d) this=%p len=%d\n", row, col, type, this, len);
   QoreStringNode *str = new QoreStringNode;
   Point p;
   assign_point(p, &((CIRCLE *)data)->center);
   double radius = MSBf8(((CIRCLE *)data)->radius);
   str->sprintf("<(%g,%g),%g>", p.x, p.y, radius);
   return str;
}

// static initialization
void QorePgsqlStatement::static_init() {
   data_map[BOOLOID]        = qpg_data_bool;
   data_map[BYTEAOID]       = qpg_data_bytea;
   data_map[CHAROID]        = qpg_data_char;
   data_map[BPCHAROID]      = qpg_data_char;
   data_map[INT8OID]        = qpg_data_int8;
   data_map[INT4OID]        = qpg_data_int4;
   data_map[OIDOID]         = qpg_data_int4;
   data_map[XIDOID]         = qpg_data_int4;
   data_map[CIDOID]         = qpg_data_int4;
   //data_map[REGPROCOID]     = qpg_data_int4;
   data_map[INT2OID]        = qpg_data_int2;
   data_map[TEXTOID]        = qpg_data_text;
   data_map[VARCHAROID]     = qpg_data_text;
   data_map[NAMEOID]        = qpg_data_text;
   data_map[FLOAT4OID]      = qpg_data_float4;
   data_map[FLOAT8OID]      = qpg_data_float8;
   data_map[ABSTIMEOID]     = qpg_data_abstime;
   data_map[RELTIMEOID]     = qpg_data_reltime;

   data_map[TIMESTAMPOID]   = qpg_data_timestamp;
#ifdef _QORE_HAS_TIME_ZONES
   data_map[TIMESTAMPTZOID] = qpg_data_timestamptz;
#else
   data_map[TIMESTAMPTZOID] = qpg_data_timestamp;
#endif
   data_map[DATEOID]        = qpg_data_date;
   data_map[INTERVALOID]    = qpg_data_interval;
   data_map[TIMEOID]        = qpg_data_time;
   data_map[TIMETZOID]      = qpg_data_timetz;
   data_map[TINTERVALOID]   = qpg_data_tinterval;
   data_map[NUMERICOID]     = qpg_data_numeric;
   data_map[CASHOID]        = qpg_data_cash;
   data_map[MACADDROID]     = qpg_data_macaddr;
   data_map[INETOID]        = qpg_data_inet;
   data_map[CIDROID]        = qpg_data_inet;
   data_map[TIDOID]         = qpg_data_tid;
   data_map[BITOID]         = qpg_data_bit;
   data_map[VARBITOID]      = qpg_data_bit;
   data_map[POINTOID]       = qpg_data_point;
   data_map[LSEGOID]        = qpg_data_lseg;
   data_map[BOXOID]         = qpg_data_box;
   data_map[PATHOID]        = qpg_data_path;
   data_map[POLYGONOID]     = qpg_data_polygon;
   data_map[CIRCLEOID]      = qpg_data_circle;

   //data_map[INT2VECTOROID]  = qpg_data_int2vector;
   //data_map[OIDVECTOROID]   = qpg_data_oidvector;

   //array_data_map[INT2VECTOROID] = std::make_pair(INT2OID, qpg_data_int2);
   // NOTE: the casts are necessary with SunPro CC 5.8...
   array_data_map[QPGT_INT4ARRAYOID]         = std::make_pair(INT4OID, (qore_pg_data_func_t)qpg_data_int4);
   array_data_map[QPGT_CIRCLEARRAYOID]       = std::make_pair(CIRCLEOID, (qore_pg_data_func_t)qpg_data_circle);
   array_data_map[QPGT_MONEYARRAYOID]        = std::make_pair(CASHOID, (qore_pg_data_func_t)qpg_data_cash);
   array_data_map[QPGT_BOOLARRAYOID]         = std::make_pair(BOOLOID, (qore_pg_data_func_t)qpg_data_bool);
   array_data_map[QPGT_BYTEAARRAYOID]        = std::make_pair(BYTEAOID, (qore_pg_data_func_t)qpg_data_bytea);
   array_data_map[QPGT_NAMEARRAYOID]         = std::make_pair(NAMEOID, (qore_pg_data_func_t)qpg_data_text);
   array_data_map[QPGT_INT2ARRAYOID]         = std::make_pair(INT2OID, (qore_pg_data_func_t)qpg_data_int2);
   //array_data_map[QPGT_INT2VECTORARRAYOID]   = std::make_pair(OID, (qore_pg_data_func_t)qpg_data_);
   //array_data_map[QPGT_REGPROCARRAYOID]      = std::make_pair(OID, (qore_pg_data_func_t)qpg_data_);
   array_data_map[QPGT_TEXTARRAYOID]         = std::make_pair(TEXTOID, (qore_pg_data_func_t)qpg_data_text);
   array_data_map[QPGT_OIDARRAYOID]          = std::make_pair(OIDOID, (qore_pg_data_func_t)qpg_data_int4);
   array_data_map[QPGT_TIDARRAYOID]          = std::make_pair(TIDOID, (qore_pg_data_func_t)qpg_data_tid);
   array_data_map[QPGT_XIDARRAYOID]          = std::make_pair(XIDOID, (qore_pg_data_func_t)qpg_data_int4);
   array_data_map[QPGT_CIDARRAYOID]          = std::make_pair(CIDOID, (qore_pg_data_func_t)qpg_data_int4);
   //array_data_map[QPGT_OIDVECTORARRAYOID]    = std::make_pair(OID, (qore_pg_data_func_t)qpg_data_);
   array_data_map[QPGT_BPCHARARRAYOID]       = std::make_pair(BPCHAROID, (qore_pg_data_func_t)qpg_data_char);
   array_data_map[QPGT_VARCHARARRAYOID]      = std::make_pair(VARCHAROID, (qore_pg_data_func_t)qpg_data_text);
   array_data_map[QPGT_INT8ARRAYOID]         = std::make_pair(INT8OID, (qore_pg_data_func_t)qpg_data_int8);
   array_data_map[QPGT_POINTARRAYOID]        = std::make_pair(POINTOID, (qore_pg_data_func_t)qpg_data_point);
   array_data_map[QPGT_LSEGARRAYOID]         = std::make_pair(LSEGOID, (qore_pg_data_func_t)qpg_data_lseg);
   array_data_map[QPGT_PATHARRAYOID]         = std::make_pair(PATHOID, (qore_pg_data_func_t)qpg_data_path);
   array_data_map[QPGT_BOXARRAYOID]          = std::make_pair(BOXOID, (qore_pg_data_func_t)qpg_data_box);
   array_data_map[QPGT_FLOAT4ARRAYOID]       = std::make_pair(FLOAT4OID, (qore_pg_data_func_t)qpg_data_float4);
   array_data_map[QPGT_FLOAT8ARRAYOID]       = std::make_pair(FLOAT8OID, (qore_pg_data_func_t)qpg_data_float8);
   array_data_map[QPGT_ABSTIMEARRAYOID]      = std::make_pair(ABSTIMEOID, (qore_pg_data_func_t)qpg_data_abstime);
   array_data_map[QPGT_RELTIMEARRAYOID]      = std::make_pair(RELTIMEOID, (qore_pg_data_func_t)qpg_data_reltime);
   array_data_map[QPGT_TINTERVALARRAYOID]    = std::make_pair(TINTERVALOID, (qore_pg_data_func_t)qpg_data_tinterval);
   array_data_map[QPGT_POLYGONARRAYOID]      = std::make_pair(POLYGONOID, (qore_pg_data_func_t)qpg_data_polygon);
   //array_data_map[QPGT_ACLITEMARRAYOID]      = std::make_pair(OID, (qore_pg_data_func_t)qpg_data_);
   array_data_map[QPGT_MACADDRARRAYOID]      = std::make_pair(MACADDROID, (qore_pg_data_func_t)qpg_data_macaddr);
   array_data_map[QPGT_INETARRAYOID]         = std::make_pair(INETOID, (qore_pg_data_func_t)qpg_data_inet);
   array_data_map[QPGT_CIDRARRAYOID]         = std::make_pair(CIDROID, (qore_pg_data_func_t)qpg_data_inet);
   array_data_map[QPGT_TIMESTAMPARRAYOID]    = std::make_pair(TIMESTAMPOID, (qore_pg_data_func_t)qpg_data_timestamp);
   array_data_map[QPGT_DATEARRAYOID]         = std::make_pair(DATEOID, (qore_pg_data_func_t)qpg_data_date);
   array_data_map[QPGT_TIMEARRAYOID]         = std::make_pair(TIMEOID, (qore_pg_data_func_t)qpg_data_time);
#ifdef _QORE_HAS_TIME_ZONES
   array_data_map[QPGT_TIMESTAMPTZARRAYOID]  = std::make_pair(TIMESTAMPTZOID, (qore_pg_data_func_t)qpg_data_timestamptz);
#else
   array_data_map[QPGT_TIMESTAMPTZARRAYOID]  = std::make_pair(TIMESTAMPTZOID, (qore_pg_data_func_t)qpg_data_timestamp);
#endif
   array_data_map[QPGT_INTERVALARRAYOID]     = std::make_pair(INTERVALOID, (qore_pg_data_func_t)qpg_data_interval);
   array_data_map[QPGT_NUMERICARRAYOID]      = std::make_pair(NUMERICOID, (qore_pg_data_func_t)qpg_data_numeric);
   array_data_map[QPGT_TIMETZARRAYOID]       = std::make_pair(TIMETZOID, (qore_pg_data_func_t)qpg_data_timetz);
   array_data_map[QPGT_BITARRAYOID]          = std::make_pair(BITOID, (qore_pg_data_func_t)qpg_data_bit);
   array_data_map[QPGT_VARBITARRAYOID]       = std::make_pair(VARBITOID, (qore_pg_data_func_t)qpg_data_bit);
   //array_data_map[QPGT_REFCURSORARRAYOID]    = std::make_pair(OID, (qore_pg_data_func_t)qpg_data_);
   //array_data_map[QPGT_REGPROCEDUREARRAYOID] = std::make_pair(OID, (qore_pg_data_func_t)qpg_data_);
   //array_data_map[QPGT_REGOPERARRAYOID]      = std::make_pair(OID, (qore_pg_data_func_t)qpg_data_);
   //array_data_map[QPGT_REGOPERATORARRAYOID]  = std::make_pair(OID, (qore_pg_data_func_t)qpg_data_);
   //array_data_map[QPGT_REGCLASSARRAYOID]     = std::make_pair(OID, (qore_pg_data_func_t)qpg_data_);
   //array_data_map[QPGT_REGTYPEARRAYOID]      = std::make_pair(OID, (qore_pg_data_func_t)qpg_data_);
   //array_data_map[QPGT_ANYARRAYOID]          = std::make_pair(OID, (qore_pg_data_func_t)qpg_data_);

   array_type_map[INT4OID]                      = QPGT_INT4ARRAYOID;
   array_type_map[CIRCLEOID]                    = QPGT_CIRCLEARRAYOID;
   array_type_map[CASHOID]                      = QPGT_MONEYARRAYOID;
   array_type_map[BOOLOID]                      = QPGT_BOOLARRAYOID;
   array_type_map[BYTEAOID]                     = QPGT_BYTEAARRAYOID;
   array_type_map[NAMEOID]                      = QPGT_NAMEARRAYOID;
   array_type_map[INT2OID]                      = QPGT_INT2ARRAYOID;
   array_type_map[TEXTOID]                      = QPGT_TEXTARRAYOID;
   array_type_map[OIDOID]                       = QPGT_OIDARRAYOID;
   array_type_map[TIDOID]                       = QPGT_TIDARRAYOID;
   array_type_map[XIDOID]                       = QPGT_XIDARRAYOID;
   array_type_map[CIDOID]                       = QPGT_CIDARRAYOID;
   array_type_map[BPCHAROID]                    = QPGT_BPCHARARRAYOID;
   array_type_map[VARCHAROID]                   = QPGT_VARCHARARRAYOID;
   array_type_map[INT8OID]                      = QPGT_INT8ARRAYOID;
   array_type_map[POINTOID]                     = QPGT_POINTARRAYOID;
   array_type_map[LSEGOID]                      = QPGT_LSEGARRAYOID;
   array_type_map[PATHOID]                      = QPGT_PATHARRAYOID;
   array_type_map[BOXOID]                       = QPGT_BOXARRAYOID;
   array_type_map[FLOAT4OID]                    = QPGT_FLOAT4ARRAYOID;
   array_type_map[FLOAT8OID]                    = QPGT_FLOAT8ARRAYOID;
   array_type_map[ABSTIMEOID]                   = QPGT_ABSTIMEARRAYOID;
   array_type_map[RELTIMEOID]                   = QPGT_RELTIMEARRAYOID;
   array_type_map[TINTERVALOID]                 = QPGT_TINTERVALARRAYOID;
   array_type_map[POLYGONOID]                   = QPGT_POLYGONARRAYOID;
   array_type_map[MACADDROID]                   = QPGT_MACADDRARRAYOID;
   array_type_map[INETOID]                      = QPGT_INETARRAYOID;
   array_type_map[CIDROID]                      = QPGT_CIDRARRAYOID;
   array_type_map[TIMESTAMPOID]                 = QPGT_TIMESTAMPARRAYOID;
   array_type_map[DATEOID]                      = QPGT_DATEARRAYOID;
   array_type_map[TIMEOID]                      = QPGT_TIMEARRAYOID;
   array_type_map[TIMESTAMPTZOID]               = QPGT_TIMESTAMPTZARRAYOID;
   array_type_map[INTERVALOID]                  = QPGT_INTERVALARRAYOID;
   array_type_map[NUMERICOID]                   = QPGT_NUMERICARRAYOID;
   array_type_map[TIMETZOID]                    = QPGT_TIMETZARRAYOID;
   array_type_map[BITOID]                       = QPGT_BITARRAYOID;
   array_type_map[VARBITOID]                    = QPGT_VARBITARRAYOID;
}

QorePgsqlStatement::QorePgsqlStatement(QorePGConnection* r_conn, const QoreEncoding* r_enc) 
  : res(0), nParams(0), allocated(0), paramTypes(0), paramValues(0), 
    paramLengths(0), paramFormats(0), paramArray(0), conn(r_conn), enc(r_enc) {
}

QorePgsqlStatement::QorePgsqlStatement(Datasource* ds) 
  : res(0), nParams(0), allocated(0), paramTypes(0), paramValues(0), 
    paramLengths(0), paramFormats(0), paramArray(0), conn((QorePGConnection*)ds->getPrivateData()),
    enc(ds->getQoreEncoding()){
}

QorePgsqlStatement::~QorePgsqlStatement() {
   reset();
}

void QorePgsqlStatement::reset() {
   if (res) {
      PQclear(res);
      res = 0;
   }

   if (allocated) {
      parambuf_list_t::iterator i = parambuf_list.begin();
      for (int j = 0; j < nParams; ++i, ++j) {
	 if (paramTypes[j] == TEXTOID && (*i)->str)
	    free((*i)->str);
	 else if (paramArray[j] && (*i)->ptr)
	    free((*i)->ptr);
	 delete *i;
      }

      parambuf_list.clear();

      free(paramTypes);
      paramTypes = 0;

      free(paramValues);
      paramValues = 0;

      free(paramLengths);
      paramLengths = 0;

      free(paramFormats);
      paramFormats = 0;

      free(paramArray);
      paramArray = 0;

      allocated = 0;
      nParams = 0;
   }
}

int QorePgsqlStatement::rowsAffected() {
   assert(res);
   return atoi(PQcmdTuples(res));
}

bool QorePgsqlStatement::hasResultData() {
   return PQnfields(res);
}

QoreListNode* QorePgsqlStatement::getArray(int type, qore_pg_data_func_t func, char *&array_data, int current, int ndim, int dim[]) {
   //printd(5, "getArray(type=%d, array_data=%p, current=%d, ndim=%d, dim[%d]=%d)\n", type, array_data, current, ndim, current, dim[current]);
   QoreListNode *l = new QoreListNode;
   
   if (current != (ndim - 1)) {
      for (int i = 0; i < dim[current]; ++i)
	 l->push(getArray(type, func, array_data, current + 1, ndim, dim));
   }
   else
      for (int i = 0; i < dim[current]; ++i) {
	 int length = ntohl(*((uint32_t *)(array_data)));
	 //printd(5, "length=%d\n", length);
	 array_data += 4;
	 if (length == -1) // NULL value
	    l->push(null());
	 else {
	    l->push(func(array_data, type, length, conn, enc));
	    array_data += length;
	 }
      }

   return l;
}

// converts from PostgreSQL data types to Qore data
AbstractQoreNode *QorePgsqlStatement::getNode(int row, int col, ExceptionSink *xsink) {
   void *data = PQgetvalue(res, row, col);
   int type = PQftype(res, col);
   //int mod = PQfmod(res, col); 
   int len = PQgetlength(res, row, col);

   //printd(5, "QorePgsqlStatement::getNode(row: %d, col: %d) type: %d this: %p len: %d\n", row, col, type, this, len);
   assert(row >= 0);

   if (PQgetisnull(res, row, col))
      return null();

   qore_pg_data_map_t::const_iterator i = data_map.find(type);
   if (i != data_map.end())
      return i->second((char *)data, type, len, conn, enc);

   // otherwise, see if it's an array
   qore_pg_array_data_map_t::const_iterator ai = array_data_map.find(type);
   if (ai == array_data_map.end()) {
      xsink->raiseException("DBI:PGSQL:TYPE-ERROR", "don't know how to handle type ID: %d", type);      
      return 0;
   }

   //printd(5, "QorePgsqlStatement::getNode(row=%d, col=%d) ARRAY type=%d this=%p len=%d\n", row, col, type, this, len);
   qore_pg_array_header *ah = (qore_pg_array_header *)data;
   int ndim = ntohl(ah->ndim);
   //int oid  = ntohl(ah->oid);
   //printd(5, "array dimensions %d, oid=%d\n", ndim, oid);
   int *dim = new int[ndim];
   int *lBound = new int[ndim];
   for (int i = 0; i < ndim; ++i) {
      dim[i]    = ntohl(ah->info[i].dim);
      lBound[i] = ntohl(ah->info[i].lBound);
      //printd(5, "%d: dim=%d lBound=%d\n", i, dim[i], lBound[i]);
   }
   
   char *array_data = ((char *)data) + 12 + 8 * ndim;
   
   AbstractQoreNode *rv = getArray(ai->second.first, ai->second.second, array_data, 0, ndim, dim);
   delete [] dim;
   delete [] lBound;
   return rv;
}

QoreHashNode* QorePgsqlStatement::getOutputHash(ExceptionSink* xsink, int* start, int maxrows) {
   assert(res);
   ReferenceHolder<QoreHashNode> h(new QoreHashNode, xsink);

   int num_columns = PQnfields(res);

   for (int i = 0; i < num_columns; ++i)
      h->setKeyValue(PQfname(res, i), new QoreListNode, NULL);

   //printd(5, "QorePgsqlStatement::getOutputHash() num_columns=%d num_rows=%d\n", num_columns, PQntuples(res));

   int nt = PQntuples(res);
   int max = maxrows < 0 ? nt : (maxrows > nt ? nt : maxrows);
   
   int i;
   for (i = start ? *start : 0; i < max; ++i) {
      for (int j = 0; j < num_columns; ++j) {
	 ReferenceHolder<AbstractQoreNode> n(getNode(i, j, xsink), xsink);
	 if (!n || *xsink)
	    return 0;

	 QoreListNode *l = reinterpret_cast<QoreListNode *>(h->getKeyValue(PQfname(res, j)));
	 l->push(n.release());
      }
   }
   if (start)
      *start = i;
   return h.release();
}

#ifdef _QORE_HAS_DBI_SELECT_ROW
QoreHashNode *QorePgsqlStatement::getSingleRow(ExceptionSink* xsink, int row) {
   int e = PQntuples(res);
   if (!e)
      return 0;
   if (e > 1) {
      xsink->raiseException("PGSQL-SELECT-ROW-ERROR", "SQL passed to selectRow() returned more than 1 row (%d rows in result set)", e);
      return 0;
   }

   return getSingleRowIntern(xsink, row);
}

QoreHashNode *QorePgsqlStatement::getSingleRowIntern(ExceptionSink* xsink, int row) {
   ReferenceHolder<QoreHashNode> h(new QoreHashNode, xsink);

   int num_columns = PQnfields(res);
   
   for (int j = 0; j < num_columns; ++j) {
      ReferenceHolder<AbstractQoreNode> n(getNode(row, j, xsink), xsink);
      if (!n || *xsink)
	 return 0;

      h->setKeyValue(PQfname(res, j), n.release(), xsink);
   }
   return h.release();
}
#endif

QoreListNode *QorePgsqlStatement::getOutputList(ExceptionSink *xsink, int* start, int maxrows) {
   ReferenceHolder<QoreListNode> l(new QoreListNode(), xsink);

   int num_columns = PQnfields(res);

   printd(5, "QorePgsqlStatement::getOutputList() num_columns=%d num_rows=%d\n", num_columns, PQntuples(res));

   int nt = PQntuples(res);
   int max = maxrows < 0 ? nt : (maxrows > nt ? nt : maxrows);

   int i;
   for (i = start ? *start : 0; i < max; ++i) {
      ReferenceHolder<QoreHashNode> h(new QoreHashNode(), xsink);
      for (int j = 0; j < num_columns; ++j) {
	 ReferenceHolder<AbstractQoreNode> n(getNode(i, j, xsink), xsink);
	 if (!n || *xsink)
	    return 0;
	 h->setKeyValue(PQfname(res, j), n.release(), xsink);
      }
      l->push(h.release());
   }
   if (start)
      *start = i;

   return l.release();
}

static int check_hash_type(const QoreHashNode *h, ExceptionSink *xsink) {
   const AbstractQoreNode *t = h->getKeyValue("^pgtype^");
   if (is_nothing(t)) {
      xsink->raiseException("DBI:PGSQL:BIND-ERROR", "missing '^pgtype^' value in bind hash");
      return -1;
   }
   const QoreBigIntNode *b = dynamic_cast<const QoreBigIntNode *>(t);
   if (!b) {
      xsink->raiseException("DBI:PGSQL:BIND-ERROR", "'^pgtype^' key contains '%s' value, expecting integer", t->getTypeName());
      return -1;
   }
   return b->val;
}

int QorePgsqlStatement::add(const AbstractQoreNode *v, ExceptionSink *xsink) {
   parambuf *pb = new parambuf;
   parambuf_list.push_back(pb);

   //printd(5, "nparams=%d, v=%p, type=%s\n", nParams, v, v ? v->getTypeName() : "(null)");
   if (nParams == allocated) {
      if (!allocated)
	 allocated = 5;
      else
	 allocated <<= 1;
      paramTypes   = (Oid *)realloc(paramTypes,    sizeof(Oid) * allocated);
      paramValues  = (char **)realloc(paramValues, sizeof(char *) * allocated);
      paramLengths = (int *)realloc(paramLengths,  sizeof(int) * allocated);
      paramFormats = (int *)realloc(paramFormats,  sizeof(int) * allocated);
      paramArray   = (int *)realloc(paramArray,    sizeof(int) * allocated);
      //printd(5, "allocated=%d, nparams=%d\n", allocated, nParams);
   }

   paramArray[nParams] = 0;
   paramFormats[nParams] = 1;

   if (is_nothing(v) || is_null(v)) {
      paramTypes[nParams] = 0;
      paramValues[nParams] = 0;
      ++nParams;
      return 0;
   }

   qore_type_t ntype = v->getType();
   if (ntype == NT_INT) {
      const QoreBigIntNode *b = reinterpret_cast<const QoreBigIntNode *>(v);
      if (b->val <= 32767 && b->val > -32768) {
	 //printd(5, "i2: %d\n", (int)b->val);
	 paramTypes[nParams]   = INT2OID;
	 pb->assign((short)b->val);
	 paramValues[nParams]  = (char *)&pb->i2;
	 paramLengths[nParams] = sizeof(short);
      }
      else if (b->val <= 2147483647 && b->val >= -2147483647) {
	 //printd(5, "i4: %d (%d, %d)\n", (int)b->val, sizeof(uint32_t), sizeof(int));
	 paramTypes[nParams]   = INT4OID;
	 pb->assign((int)b->val);
	 //pb->i4 = htonl((int)b->val);
	 paramValues[nParams]  = (char *)&pb->i4;
	 paramLengths[nParams] = sizeof(int);
      }
      else {
	 //printd(5, "i8: %lld\n", b->val);
	 paramTypes[nParams]   = INT8OID;
	 pb->assign(b->val);
	 paramValues[nParams]  = (char *)&pb->i8;
	 paramLengths[nParams] = sizeof(int64);
      }
      ++nParams;
      return 0;
   }

   if (ntype == NT_FLOAT) {
      paramTypes[nParams]   = FLOAT8OID;
      pb->assign(reinterpret_cast<const QoreFloatNode *>(v)->f);
      paramValues[nParams]  = (char *)&pb->f8;
      paramLengths[nParams] = sizeof(double);

      ++nParams;
      return 0;
   }

   if (ntype == NT_STRING) {
      const QoreStringNode *str = reinterpret_cast<const QoreStringNode *>(v);
      paramTypes[nParams] = TEXTOID;
      pb->str = NULL;
      TempEncodingHelper tmp(str, enc, xsink);
      if (!tmp)
	 return -1;

      paramLengths[nParams] = tmp->strlen();
      paramValues[nParams]  = (char *)tmp->getBuffer();
      // grab and save the buffer if it's a temporary string to be free'd after the request
      if (tmp.is_temp())
	 pb->str = tmp.giveBuffer();
      paramFormats[nParams] = 0;
      
      ++nParams;
      return 0;
   }

   if (ntype == NT_BOOLEAN) {
      paramTypes[nParams]   = BOOLOID;
      pb->b = reinterpret_cast<const QoreBoolNode *>(v)->getValue();
      paramValues[nParams]  = (char *)&pb->b;
      paramLengths[nParams] = sizeof(bool);

      ++nParams;
      return 0;
   }

   if (ntype == NT_DATE) {
      const DateTimeNode *d = reinterpret_cast<const DateTimeNode *>(v);
      if (d->isRelative()) {
	 paramTypes[nParams] = INTERVALOID;
	 
	 int day_seconds;
	 if (conn->has_interval_day()) {
	    pb->iv.rest.with_day.month = htonl(d->getMonth());
	    pb->iv.rest.with_day.day   = htonl(d->getDay());
	    day_seconds = 0;
	 }
	 else {
	    pb->iv.rest.month = htonl(d->getMonth());
	    day_seconds = d->getDay() * 3600 * 24;
	 }
	 
#ifdef _QORE_HAS_TIME_ZONES
	 if (conn->has_integer_datetimes()) {
	    pb->iv.time.i = i8MSB((((int64)d->getYear() * 365 * 24 * 3600) + (int64)d->getHour() * 3600 + (int64)d->getMinute() * 60 + (int64)d->getSecond() + day_seconds) * 1000000 + (int64)d->getMicrosecond());
	    //printd(0, "binding interval %lld\n", MSBi8(pb->iv.time.i));
	 }
	 else
	    pb->iv.time.f = f8MSB((double)((d->getYear() * 365 * 24 * 3600) + d->getHour() * 3600 + d->getMinute() * 60 + d->getSecond() + day_seconds) + (double)d->getMicrosecond() / 1000000.0);
#else
	 if (conn->has_integer_datetimes()) {
	    pb->iv.time.i = i8MSB((((int64)d->getYear() * 365 * 24 * 3600) + (int64)d->getHour() * 3600 + (int64)d->getMinute() * 60 + (int64)d->getSecond() + day_seconds) * 1000000 + (int64)d->getMillisecond() * 1000);
	    //printd(0, "binding interval %lld\n", MSBi8(pb->iv.time.i));
	 }
	 else
	    pb->iv.time.f = f8MSB((double)((d->getYear() * 365 * 24 * 3600) + d->getHour() * 3600 + d->getMinute() * 60 + d->getSecond() + day_seconds) + (double)d->getMillisecond() / 1000.0);
#endif
	 
	 paramValues[nParams] = (char *)&pb->iv;
	 paramLengths[nParams] = conn->has_interval_day() ? 16 : 12;
      }
      else {
#ifdef _QORE_HAS_TIME_ZONES
	 paramTypes[nParams] = TIMESTAMPTZOID;
	 
	 if (conn->has_integer_datetimes()) {
	    // get number of seconds offset from jan 1 2000 then make it microseconds and add ms
	    int64 val = (d->getEpochSecondsUTC() - PGSQL_EPOCH_OFFSET) * 1000000 + d->getMicrosecond();
	    //printd(0, "timestamp int64 time=%lld\n", val);
	    pb->assign(val);
	    paramValues[nParams] = (char *)&pb->i8;
	    paramLengths[nParams] = sizeof(int64);
	 }
	 else {
	    double val = (double)((double)d->getEpochSecondsUTC() - PGSQL_EPOCH_OFFSET) + (double)(d->getMicrosecond() / 1000000.0);
	    //printd(0, "timestamp double time=%9g\n", val);
	    pb->assign(val);
	    paramValues[nParams] = (char *)&pb->f8;
	    paramLengths[nParams] = sizeof(double);
	 }
#else
	 paramTypes[nParams] = TIMESTAMPOID;
	 
	 if (conn->has_integer_datetimes()) {
	    // get number of seconds offset from jan 1 2000 then make it microseconds and add ms
	    int64 val = (d->getEpochSeconds() - PGSQL_EPOCH_OFFSET) * 1000000 + d->getMillisecond() * 1000;
	    //printd(0, "timestamp int64 time=%lld\n", val);
	    pb->assign(val);
	    paramValues[nParams] = (char *)&pb->i8;
	    paramLengths[nParams] = sizeof(int64);
	 }
	 else {
	    double val = (double)((double)d->getEpochSeconds() - PGSQL_EPOCH_OFFSET) + (double)(d->getMillisecond() / 1000.0);
	    //printd(0, "timestamp double time=%9g\n", val);
	    pb->assign(val);
	    paramValues[nParams] = (char *)&pb->f8;
	    paramLengths[nParams] = sizeof(double);
	 }
#endif
      }
      ++nParams;
      return 0;
   }

   if (ntype == NT_BINARY) {
      const BinaryNode *b = reinterpret_cast<const BinaryNode *>(v);
      paramTypes[nParams] = BYTEAOID;
      paramValues[nParams] = (char *)b->getPtr();
      paramLengths[nParams] = b->size();

      ++nParams;
      return 0;
   }

   if (ntype == NT_HASH) {
      const QoreHashNode *vh = reinterpret_cast<const QoreHashNode *>(v);
      Oid type = check_hash_type(vh, xsink);
      if ((int)type < 0)
	 return -1;
      const AbstractQoreNode *t = vh->getKeyValue("^value^");
      if (is_nothing(t) || is_null(t)) {
	 paramTypes[nParams] = 0;
	 paramValues[nParams] = 0;
      }
      else {
	 paramTypes[nParams] = type;
	 
	 QoreStringValueHelper str(t);
	 paramValues[nParams]  = (char *)str->getBuffer();
	 paramLengths[nParams] = str->strlen();
	 
	 // save the buffer for later deletion if it's a temporary string
	 if (str.is_temp()) {
	    TempString tstr(str.giveString());
	    pb->str = tstr->giveBuffer();
	 }
	 else
	    pb->str = 0;
      }
      paramFormats[nParams] = 0;
      
      ++nParams;
      return 0;
   }

   if (ntype == NT_LIST) {
      const QoreListNode *l = reinterpret_cast<const QoreListNode *>(v);
      int len = l->size();
      if (!len) {
	 paramTypes[nParams] = 0;
	 paramValues[nParams] = 0;
      }
      else {
	 std::auto_ptr<QorePGBindArray> ba(new QorePGBindArray(conn));
	 if (ba->create_data(l, 0, enc, xsink))
	    return -1;
	 
	 paramArray[nParams] = 1;
	 paramTypes[nParams] = ba->getArrayOid();
	 paramLengths[nParams] = ba->getSize();
	 pb->ptr = ba->getHeader();
	 paramValues[nParams] = (char *)pb->ptr;
	 paramFormats[nParams] = ba->getFormat();
	 //printd(5, "QorePgsqlStatement::add() array size=%d, arrayoid=%d, data=%p\n", ba->getSize(), ba->getArrayOid(), pb->ptr);
      }
      
      ++nParams;
      return 0;
   }

   paramTypes[nParams] = 0;
   paramValues[nParams] = 0;
   xsink->raiseException("DBI:PGSQL:EXEC-EXCEPTION", "don't know how to bind type '%s'", v->getTypeName());

   nParams++;
   return -1;
}

QorePGBindArray::QorePGBindArray(QorePGConnection *r_conn) : ndim(0), size(0), allocated(0), elements(0),
							     ptr(NULL), hdr(NULL), type(-1), oid(0), arrayoid(0),
							     format(1), conn(r_conn) {
}

QorePGBindArray::~QorePGBindArray() {
   if (hdr)
      free(hdr);
}

int QorePGBindArray::getOid() const {
   return oid;
}

int QorePGBindArray::getArrayOid() const {
   return arrayoid;
}

int QorePGBindArray::getSize() const {
   return size;
}

qore_pg_array_header *QorePGBindArray::getHeader() {
   qore_pg_array_header *rv = hdr;
   hdr = NULL;
   return rv;
}

int QorePGBindArray::check_type(const AbstractQoreNode *n, ExceptionSink *xsink) {
   // skip null types
   if (is_nothing(n) || is_null(n)) {
      //return 0;
      // FIXME: pgsql null binding in arrays should work according to the documentation
      // however with PG 8.1 it does not appear to work :-(
      xsink->raiseException("DBI:PGSQL:ARRAY-ERROR", "cannot bind NULL values within an array");
      return -1;
   }
   qore_type_t t = n->getType();
   if (type == -1) {
      type = t;
      // check that type is supported
      if (type == NT_INT) {
	 arrayoid = QPGT_INT8ARRAYOID;
	 oid = INT8OID;
	 return 0;
      }

      if (type == NT_FLOAT) {
	 arrayoid = QPGT_FLOAT8ARRAYOID;
	 oid = FLOAT8OID;
	 return 0;
      }

      if (type == NT_BOOLEAN) {
	 arrayoid = QPGT_BOOLARRAYOID;
	 oid = BOOLOID;
	 return 0;
      }

      if (type == NT_STRING) {
	 arrayoid = QPGT_TEXTARRAYOID;
	 oid = TEXTOID;
	 //format = 0;
	 return 0;
      }

      if (type == NT_DATE) {
	 const DateTimeNode *date = reinterpret_cast<const DateTimeNode *>(n);
	 if (date->isRelative()) {
	    arrayoid = QPGT_INTERVALARRAYOID;
	    oid = INTERVALOID;
	 }
	 else {
#ifdef _HAVE_QORE_TIME_ZONES
	    arrayoid = QPGT_TIMESTAMPTZARRAYOID;
	    oid = TIMESTAMPTZOID;
#else
	    arrayoid = QPGT_TIMESTAMPARRAYOID;
	    oid = TIMESTAMPOID;
#endif
	 }
	 return 0;
      }

      if (type == NT_BINARY) {
	 arrayoid = QPGT_BYTEAARRAYOID;
	 oid = BYTEAOID;
	 return 0;
      }

/*
      if (type == NT_HASH) {
	 format = 0;
	 oid = check_hash_type(n->valx.hash, xsink);
	 if (oid < 0)
	    return -1;
	 qore_pg_array_type_map_t::const_iterator i = QorePgsqlStatement::array_type_map.find(oid);
	 if (i == QorePgsqlStatement::array_type_map.end()) {
	    xsink->raiseException("DBI:PGSQL:ARRAY-ERROR", "don't know how to bind arrays of typeid %d", oid);
	    return -1;
	 }
	 arrayoid = i->second;
	 return 0;
      }
*/

      xsink->raiseException("DBI:PGSQL:ARRAY-ERROR", "don't know how to bind type '%s'", n->getTypeName());
      return -1;
   }

   if (t != type) {
      xsink->raiseException("DBI:PGSQL:ARRAY-ERROR", "array elements must be all of the same type for binding");
      return -1;
   }

   if (type == NT_DATE) {
      const DateTimeNode *date = reinterpret_cast<const DateTimeNode *>(n);
      if (date) {
	 if (date->isRelative() && (oid == TIMESTAMPOID || oid == TIMESTAMPTZOID)) {
	    xsink->raiseException("DBI:PGSQL:ARRAY-ERROR", "array type was set to TIMESTAMP or TIMESTAMPTZ, but a relative date/time is present in the list");
	    return -1;
	 }
	 if (date->isAbsolute() && (oid == INTERVALOID)) {
	    xsink->raiseException("DBI:PGSQL:ARRAY-ERROR", "array type was set to INTERVAL, but an absolute date/time is present in the list");
	    return -1;
	 }
      }
   }
   return 0;
}

int QorePGBindArray::check_oid(const QoreHashNode *h, ExceptionSink *xsink) {
   int o = check_hash_type(h, xsink);
   if (o < 0)
      return -1;

   if (!oid)
      oid = o;
   else if (o != oid) {
      xsink->raiseException("DBI:PGSQL:ARRAY-ERROR", "array elements must be all of the same type for binding");
      return -1;
   }
   return 0;
}

int QorePGBindArray::new_dimension(const QoreListNode *l, int current, ExceptionSink *xsink) {
   if (current >= MAXDIM) {
      xsink->raiseException("DBI:PGSQL:ARRAY-ERROR", "array exceeds maximum number of dimensions (%d)", MAXDIM);
      return -1;
   }
   ndim++;
   int len = l->size();
   if (!elements)
      elements = len;
   else
      elements *= len;
   dim[current] = len;
   return 0;
}

int QorePGBindArray::create_data(const QoreListNode *l, int current, const QoreEncoding *enc, ExceptionSink *xsink) {
   if (new_dimension(l, current, xsink))
      return -1;
   return process_list(l, current, enc, xsink);
}

void QorePGBindArray::check_size(int len) {
   int space = (len == -1 ? 4 : len + 4);
   if (allocated < (size + space)) {
      int offset = 12 + 8 * ndim;
      bool init = !allocated;
      if (init)
	 size = offset;
      allocated = size + space + (allocated / 3);
      hdr = (qore_pg_array_header *)realloc(hdr, allocated);
      // setup header
      //printd(5, "check_size(len=%d) ndim=%d, allocated=%d, offset=%d hdr=%p\n", len, ndim, allocated, 12 + 8 * ndim, hdr);
      if (init) {
	 hdr->ndim  = htonl(ndim);
	 hdr->flags = 0; // htonl(0);
	 hdr->oid   = htonl(oid);
	 for (int i = 0; i < ndim; i++) {
	    //printd(5, "this=%p initializing header addr=%p (offset=%d) ndim=%d, oid=%d, dim[%d]=%d, lBound[%d]=1\n", 
	    //       this, &hdr->info[i].dim, (char *)&hdr->info[i].dim - (char *)hdr, ndim, oid, i, dim[i], i);
	    hdr->info[i].dim = htonl(dim[i]);
	    hdr->info[i].lBound = htonl(1);
	 }
      }
      ptr = (char *)hdr + size;
   }
   //printd(5, "check_size(len=%d) space=%d ndim=%d, allocated=%d, size=%d (%d) offset=%d hdr=%p ptr=%p\n", len, space, ndim, allocated, size, ptr-(char *)hdr, 12 + 8 * ndim, hdr, ptr);
   int *length = (int *)ptr;
   *length = htonl(len);
   ptr += 4;
   size += space;
}

int QorePGBindArray::bind(const AbstractQoreNode *n, const QoreEncoding *enc, ExceptionSink *xsink) {
   // bind a NULL for NOTHING or NULL
   if (is_nothing(n) || is_null(n)) {
      check_size(-1);
      return 0;
   }

   if (type == NT_INT) {
      check_size(8);
      int64 *i8 = (int64 *)ptr;
      *i8 = i8MSB((reinterpret_cast<const QoreBigIntNode *>(n))->val);
      ptr += 8;
      return 0;
   }

   if (type == NT_FLOAT) {
      check_size(8);
      double *f8 = (double *)ptr;
      *f8 = f8MSB(reinterpret_cast<const QoreFloatNode *>(n)->f);
      ptr += 8;
      return 0;
   }

   if (type == NT_BOOLEAN) {
      check_size(sizeof(bool));
      bool *b = (bool *)ptr;
      *b = htonl(reinterpret_cast<const QoreBoolNode *>(n)->getValue());
      ptr += sizeof(bool);
      return 0;
   }   

   if (type == NT_STRING) {
      const QoreStringNode *str = reinterpret_cast<const QoreStringNode *>(n);
      TempEncodingHelper tmp(str, enc, xsink);
      if (!tmp)
	 return -1;
      
      int len = tmp->strlen();
      check_size(len);
      memcpy(ptr, tmp->getBuffer(), len);
      ptr += len;
      return 0;
   }

   if (type == NT_DATE) {
      const DateTimeNode *d = reinterpret_cast<const DateTimeNode *>(n);
      if (d->isRelative()) {
	 int d_size = conn->has_interval_day() ? 16 : 12;
	 check_size(d_size);
	 qore_pg_interval *i = (qore_pg_interval *)ptr;

	 if (conn->has_interval_day()) {
	    i->rest.with_day.month = htonl(d->getMonth()); 
	    i->rest.with_day.day = htonl(d->getDay());
	 }
	 else
	    i->rest.month = htonl(d->getMonth()); 

#ifdef _QORE_HAS_TIME_ZONES
	 if (conn->has_integer_datetimes())
	    i->time.i = i8MSB(((d->getYear() * 365 * 24 * 3600) + d->getHour() * 24 * 3600 + d->getMinute() * 3600 + d->getSecond()) * 1000000 + d->getMicrosecond());
	 else
	    i->time.f = f8MSB((double)((d->getYear() * 365 * 24 * 3600) + d->getHour() * 3600 + d->getMinute() * 60 + d->getSecond()) + (double)d->getMicrosecond() / 1000000.0);
#else
	 if (conn->has_integer_datetimes())
	    i->time.i = i8MSB(((d->getYear() * 365 * 24 * 3600) + d->getHour() * 24 * 3600 + d->getMinute() * 3600 + d->getSecond()) * 1000000 + d->getMillisecond() * 1000);
	 else
	    i->time.f = f8MSB((double)((d->getYear() * 365 * 24 * 3600) + d->getHour() * 3600 + d->getMinute() * 60 + d->getSecond()) + (double)d->getMillisecond() / 1000.0);
#endif

	 ptr += d_size;
      }
      else {
	 check_size(8);

#ifdef _QORE_HAS_TIME_ZONES
	 if (conn->has_integer_datetimes()) {
	    int64 *i = (int64 *)ptr;
	    // get number of seconds offset from jan 1 2000 then make it microseconds and add ms
	    *i = i8MSB((d->getEpochSecondsUTC() - PGSQL_EPOCH_OFFSET) * 1000000 + d->getMicrosecond());
	 }
	 else {
	    double *f = (double *)ptr;
	    *f = f8MSB((double)((double)d->getEpochSecondsUTC() - PGSQL_EPOCH_OFFSET) + (double)(d->getMicrosecond() / 1000000.0));
	 }
#else
	 if (conn->has_integer_datetimes()) {
	    int64 *i = (int64 *)ptr;
	    // get number of seconds offset from jan 1 2000 then make it microseconds and add ms
	    *i = i8MSB((d->getEpochSeconds() - PGSQL_EPOCH_OFFSET) * 1000000 + d->getMillisecond() * 1000);
	 }
	 else {
	    double *f = (double *)ptr;
	    *f = f8MSB((double)((double)d->getEpochSeconds() - PGSQL_EPOCH_OFFSET) + (double)(d->getMillisecond() / 1000.0));
	 }
#endif
	 ptr += 8;
      }
      return 0;
   }

   if (type == NT_BINARY) {
      const BinaryNode *b = reinterpret_cast<const BinaryNode *>(n);
      int len = b->size();
      check_size(len);
      memcpy(ptr, b->getPtr(), len);
      ptr += len;
      return 0;
   }

   if (type == NT_HASH) {
      const QoreHashNode *h = reinterpret_cast<const QoreHashNode *>(n);
      const AbstractQoreNode *t = h->getKeyValue("^value^");
      if (is_nothing(t) || is_null(t))
	 check_size(-1);
      else {
	 QoreStringValueHelper tmp(t, enc, xsink);
	 if (*xsink)
	    return -1;
	 int len = tmp->strlen();
	 check_size(len);
	 memcpy(ptr, tmp->getBuffer(), len);
	 ptr += len;
      }
      return 0;
   }
   return 0;
}

int QorePGBindArray::process_list(const QoreListNode *l, int current, const QoreEncoding *enc, ExceptionSink *xsink) {
   ConstListIterator li(l);
   while (li.next()) {
      qore_type_t ntype;
      const AbstractQoreNode *n = li.getValue();
      ntype = n ? n->getType() : NT_NOTHING;
      if (type == NT_LIST) {
	 const QoreListNode *l = reinterpret_cast<const QoreListNode *>(n);
	 if (li.first())
	    if (new_dimension(l, current + 1, xsink))
	       return -1;
	 if (process_list(l, current + 1, enc, xsink))
	    return -1;
      }
      else {
	 if (check_type(n, xsink))
	    return -1;
	 if (ntype == NT_HASH && check_oid(reinterpret_cast<const QoreHashNode *>(n), xsink))
	    return -1;

	 if (bind(n, enc, xsink))
	    return -1;
      }
   }
   if (!oid) {
      xsink->raiseException("DBI:PGSQL:ARRAY-BIND-ERROR", "no type can be determined from the list");
      return -1;
   }
   return 0;
}

int QorePgsqlStatement::parse(QoreString* str, const QoreListNode* args, ExceptionSink* xsink) {
   char quote = 0;
   const char *p = str->getBuffer();
   QoreString tmp;
   int index = 0;
   while (*p) {
      if (!quote && (*p) == '%' && (p == str->getBuffer() || !isalnum(*(p-1)))) { // found value marker
         int offset = p - str->getBuffer();

         p++;
         const AbstractQoreNode *v = args ? args->retrieve_entry(index++) : NULL;
	 if ((*p) == 'd') {
	    DBI_concat_numeric(&tmp, v);
	    str->replace(offset, 2, &tmp);
	    p = str->getBuffer() + offset + tmp.strlen();
	    tmp.clear();
	    continue;
	 }
	 if ((*p) == 's') {
	    if (DBI_concat_string(&tmp, v, xsink))
	       return -1;
	    str->replace(offset, 2, &tmp);
	    p = str->getBuffer() + offset + tmp.strlen();
	    tmp.clear();
	    continue;
	 }
         if ((*p) != 'v') {
            xsink->raiseException("DBI-EXEC-PARSE-EXCEPTION", "invalid value specification (expecting '%v' or '%%d', got %%%c)", *p);
            return -1;
         }
         p++;
         if (isalpha(*p)) {
            xsink->raiseException("DBI-EXEC-PARSE-EXCEPTION", "invalid value specification (expecting '%v' or '%%d', got %%v%c*)", *p);
            return -1;
         }

         // replace value marker with "$<num>"
         // find byte offset in case string buffer is reallocated with replace()
	 tmp.sprintf("$%d", nParams + 1); 
         str->replace(offset, 2, &tmp);
         p = str->getBuffer() + offset + tmp.strlen();
	 tmp.clear();
	 if (add(v, xsink))
	    return -1;
      }
      else if (((*p) == '\'') || ((*p) == '\"')) {
         if (!quote)
            quote = *p;
         else if (quote == (*p))
            quote = 0;
         p++;
      }
      // allow escaling of '%' characters
      else if (!quote && (*p) == '\\' && (*(p+1) == ':' || *(p+1) == '%')) {
	 str->splice(p - str->getBuffer(), 1, xsink);
	 p += 2;
      }
      else
         p++;
   }
   return 0;
}

// hackish way to determine if a pre release 8 server is using int8 or float8 types for datetime values
bool QorePgsqlStatement::checkIntegerDateTimes(PGconn *pc, ExceptionSink *xsink) {
   PGresult* tres = PQexecParams(pc, "select '00:00'::time as \"a\"", 0, NULL, NULL, NULL, NULL, 1);
   if (!tres) {
      xsink->raiseException("DBI:PGSQL:ERROR", "Error determining binary date/time format: PQexecParams() returned NULL");
      return false;
   }
   // make sure and delete the result when we exit
   ON_BLOCK_EXIT(PQclear, tres);

   ExecStatusType rc = PQresultStatus(tres);
   if (rc != PGRES_COMMAND_OK && rc != PGRES_TUPLES_OK) {
      const char *err = PQerrorMessage(pc);
      const char *e;
      if (!strncmp(err, "ERROR:  ", 8) || !strncmp(err, "FATAL:  ", 8))
	 e = err + 8;
      else
	 e = err;
      QoreString desc(e);
      desc.chomp();
      xsink->raiseException("DBI:PGSQL:ERROR", "Error determining binary date/time format: %s", desc.getBuffer());
      return false;
   }
   
   // ensure that the result format is what we expect
   if (PQnfields(tres) != 1) {
      xsink->raiseException("DBI:PGSQL:ERROR", "Error determining binary date/time format; expecting 1 column in test query, got %d", PQnfields(tres));
      return false;
   }
   if (PQntuples(tres) != 1) {
      xsink->raiseException("DBI:PGSQL:ERROR", "Error determining binary date/time format; expecting 1 row in test query, got %d", PQntuples(tres));
      return false;
   }

   void *data = PQgetvalue(tres, 0, 0);
   int64 val = MSBi8(*((uint64_t *)data));

   return val == 0;
}

int QorePgsqlStatement::exec(PGconn *pc, const QoreString *str, const QoreListNode *args, ExceptionSink *xsink) {
   // convert string to required character encoding or copy
   std::auto_ptr<QoreString> qstr(str->convertEncoding(enc, xsink));
   if (!qstr.get())
      return -1;

   if (parse(qstr.get(), args, xsink))
      return -1;

   printd(5, "QorePgsqlStatement::exec() nParams=%d args=%p (len=%d) sql=%s\n", nParams, args, args ? args->size() : 0, qstr->getBuffer());
  
   assert(!res);
   res = PQexecParams(pc, qstr->getBuffer(), nParams, paramTypes, paramValues, paramLengths, paramFormats, 1);
   return conn->checkClearResult(res, xsink);
}

int QorePgsqlStatement::exec(PGconn *pc, const char *cmd, ExceptionSink *xsink) {
   assert(!res);
   res = PQexecParams(pc, cmd, 0, NULL, NULL, NULL, NULL, 1);
   return conn->checkClearResult(res, xsink);
}

QorePGConnection::QorePGConnection(const char *str, ExceptionSink *xsink) {
   pc = PQconnectdb(str);
   if (PQstatus(pc) != CONNECTION_OK) {
      doError(xsink);
      return;
   }

   const char *pstr;
   // get server version to encode/decode binary values properly
#if POSTGRES_VERSION_MAJOR >= 8
   int server_version = PQserverVersion(pc);
   //printd(5, "version=%d\n", server_version);
   interval_has_day = server_version >= 80100 ? true : false;
#else
   pstr = PQparameterStatus(pc, "server_version");
   interval_has_day = strcmp(pstr, "8.1") >= 0 ? true : false;
#endif
   pstr = PQparameterStatus(pc, "integer_datetimes");
      
   if (!pstr || !pstr[0]) {
      // encoding does not matter here; we are only getting an integer
      QorePgsqlStatement res(this, QCS_DEFAULT);
      integer_datetimes = res.checkIntegerDateTimes(pc, xsink);
   }
   else
      integer_datetimes = strcmp(pstr, "off");
}

QorePGConnection::~QorePGConnection() {
   if (pc)
      PQfinish(pc);
}

int QorePGConnection::setPGEncoding(const char *enc, ExceptionSink *xsink) {
   if (PQsetClientEncoding(pc, enc)) {
      xsink->raiseException("DBI:PGSQL:ENCODING-ERROR", "invalid PostgreSQL encoding '%s'", enc);
      return -1;
   }
   return 0;
}

int QorePGConnection::commit(Datasource *ds, ExceptionSink *xsink) {
   QorePgsqlStatement res(this, ds->getQoreEncoding());
   return res.exec(pc, "commit", xsink);
}

int QorePGConnection::rollback(Datasource *ds, ExceptionSink *xsink) {
   QorePgsqlStatement res(this, ds->getQoreEncoding());
   return res.exec(pc, "rollback", xsink);
}

int QorePGConnection::begin_transaction(Datasource *ds, ExceptionSink *xsink) {
   QorePgsqlStatement res(this, ds->getQoreEncoding());
   return res.exec(pc, "begin", xsink);
}

QoreListNode* QorePGConnection::selectRows(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink) {
   QorePgsqlStatement res(this, ds->getQoreEncoding());
   if (res.exec(pc, qstr, args, xsink))
      return NULL;

   return res.getOutputList(xsink);
}

#ifdef _QORE_HAS_DBI_SELECT_ROW
QoreHashNode* QorePGConnection::selectRow(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink) {
   QorePgsqlStatement res(this, ds->getQoreEncoding());
   if (res.exec(pc, qstr, args, xsink))
      return NULL;

   return res.getSingleRow(xsink);
}
#endif

AbstractQoreNode *QorePGConnection::exec(Datasource *ds, const QoreString *qstr, const QoreListNode *args, ExceptionSink *xsink) {
   QorePgsqlStatement res(this, ds->getQoreEncoding());
   if (res.exec(pc, qstr, args, xsink))
      return NULL;

   if (res.hasResultData())
      return res.getOutputHash(xsink);

   return new QoreBigIntNode(res.rowsAffected());
}

#ifdef _QORE_HAS_DBI_EXECRAW
AbstractQoreNode *QorePGConnection::execRaw(Datasource *ds, const QoreString *qstr, ExceptionSink *xsink) {
   QorePgsqlStatement res(this, ds->getQoreEncoding());
   // convert string to required character encoding or copy
   std::auto_ptr<QoreString> ccstr(qstr->convertEncoding(ds->getQoreEncoding(), xsink));

   if (res.exec(pc, ccstr->getBuffer(), xsink))
      return NULL;

   if (res.hasResultData())
      return res.getOutputHash(xsink);

   return new QoreBigIntNode(res.rowsAffected());
}
#endif

int QorePGConnection::get_server_version() const {
#if POSTGRES_VERSION_MAJOR >= 8
   return PQserverVersion(pc);
#else
   // create version number from string
   int ver;
   const char *pstr = PQparameterStatus(pc, "server_version");
   if (!pstr)
      return 0;
   ver = 10000 * atoi(pstr);
   char *i = strchr(pstr, '.');
   if (i) {
      ++i;
      ver += 100 * atoi(i);
      i = strchr(i, '.');
      if (i)
	 ver += atoi(i + 1);
   }
   return ver;
#endif
}

#ifdef _QORE_HAS_PREPARED_STATMENT_API
int QorePgsqlPreparedStatement::prepare(const QoreString& n_sql, const QoreListNode* args, bool n_parse, ExceptionSink* xsink) {
   assert(!sql);
   // create copy of string and convert encoding if necessary
   sql = n_sql.convertEncoding(enc, xsink);
   if (*xsink)
      return -1;

   if (args)
      targs = args->listRefSelf();

   do_parse = n_parse;

   return 0;
}

int QorePgsqlPreparedStatement::bind(const QoreListNode &l, ExceptionSink *xsink) {
   if (targs) {
      targs->deref(xsink);
      targs = 0;
      if (*xsink)
	 return -1;
   }

   targs = l.listRefSelf();
   return 0;
}

int QorePgsqlPreparedStatement::exec(ExceptionSink* xsink) {
   if (res)
      QorePgsqlStatement::reset();

   if (do_parse && parse(sql, targs, xsink))
      return -1;

   printd(5, "QorePgsqlPreparedStatement::exec() nParams: %d args: %p (len: %d) sql: %s\n", nParams, targs, targs ? targs->size() : 0, sql->getBuffer());
  
   assert(!res);
   res = PQexecParams(conn->get(), sql->getBuffer(), nParams, paramTypes, paramValues, paramLengths, paramFormats, 1);
   return conn->checkClearResult(res, xsink);
}

QoreHashNode* QorePgsqlPreparedStatement::fetchRow(ExceptionSink* xsink) {
   if (crow == -1) {
      xsink->raiseException("DBI:PGSQL-FETCH-ROW-ERROR", "call SQLStatement::next() before calling SQLStatement::fetchRow()");
      return 0;
   }
   return getSingleRowIntern(xsink, crow);
}

QoreListNode* QorePgsqlPreparedStatement::fetchRows(int rows, ExceptionSink *xsink) {
   if (crow == -1)
      crow = 0;
   return getOutputList(xsink, &crow, rows);
}

QoreHashNode* QorePgsqlPreparedStatement::fetchColumns(int rows, ExceptionSink *xsink) {
   if (crow == -1)
      crow = 0;
   return getOutputHash(xsink, &crow, rows);
}

bool QorePgsqlPreparedStatement::next() {
   assert(res);
   ++crow;
   if (crow >= PQntuples(res)) {
      crow = -1;
      return false;
   }
   return true;
}

void QorePgsqlPreparedStatement::reset(ExceptionSink* xsink) {
   if (sql) {
      delete sql;
      sql = 0;
   }

   if (do_parse)
      do_parse = false;

   if (targs) {
      targs->deref(xsink);
      targs = 0;
   }

   if (crow != -1) 
      crow = -1;

   // call parent reset function
   QorePgsqlStatement::reset();
}

#endif