/*
  QorePGMapper.cpp
  
  Qore Programming Language

  Copyright 2003 - 2025 Qore Technologies, s.r.o.

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

#include "QorePGMapper.h"

const_encoding_map_t QorePGMapper::map;
rev_encoding_map_t QorePGMapper::rmap;

#define DO_MAP(a, b) map[(a)] = (b); rmap[(b)] = (a);

void QorePGMapper::static_init() {
   // UTF-8
   DO_MAP("UTF8",       QCS_UTF8);

   // ISO 8859 Latin encodings
   DO_MAP("LATIN1",     QCS_ISO_8859_1);
   DO_MAP("LATIN2",     QCS_ISO_8859_2);
   DO_MAP("LATIN3",     QCS_ISO_8859_3);
   DO_MAP("LATIN4",     QCS_ISO_8859_4);
   DO_MAP("ISO_8859_5", QCS_ISO_8859_5);
   DO_MAP("ISO_8859_6", QCS_ISO_8859_6);
   DO_MAP("ISO_8859_7", QCS_ISO_8859_7);
   DO_MAP("ISO_8859_8", QCS_ISO_8859_8);
   DO_MAP("LATIN5",     QCS_ISO_8859_9);
   DO_MAP("LATIN6",     QCS_ISO_8859_10);
   DO_MAP("LATIN7",     QCS_ISO_8859_13);
   DO_MAP("LATIN8",     QCS_ISO_8859_14);
   DO_MAP("LATIN9",     QCS_ISO_8859_15);
   DO_MAP("LATIN10",    QCS_ISO_8859_16);

   // ASCII
   DO_MAP("SQL_ASCII",  QCS_USASCII);

   // Cyrillic encodings
   DO_MAP("KOI8",       QCS_KOI8_R);
   DO_MAP("KOI8R",      QCS_KOI8_R);
   DO_MAP("KOI8U",      QCS_KOI8_U);

   // Windows code pages
   DO_MAP("WIN1250",    QCS_WINDOWS_1250);
   DO_MAP("WIN1251",    QCS_WINDOWS_1251);
   DO_MAP("WIN1252",    QCS_WINDOWS_1252);
   DO_MAP("WIN1253",    QCS_WINDOWS_1253);
   DO_MAP("WIN1254",    QCS_WINDOWS_1254);
   DO_MAP("WIN1255",    QCS_WINDOWS_1255);
   DO_MAP("WIN1256",    QCS_WINDOWS_1256);
   DO_MAP("WIN1257",    QCS_WINDOWS_1257);
   DO_MAP("WIN1258",    QCS_WINDOWS_1258);
}

const QoreEncoding *QorePGMapper::getQoreEncoding(const char *cs) {
   const_encoding_map_t::const_iterator i = map.find(cs);
   
   if (i != map.end())
      return i->second;

   return QEM.findCreate(cs);
}

const char *QorePGMapper::getPGEncoding(const QoreEncoding *enc) {
   rev_encoding_map_t::const_iterator i = rmap.find(enc);

   if (i != rmap.end())
      return i->second;

   return 0;
}
