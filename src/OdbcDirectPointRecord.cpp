//
//  OdbcDirectPointRecord.cpp
//  epanet-rtx
//
//  Created by Sam Hatchett on 12/31/13.
//
//

#include "OdbcDirectPointRecord.h"

#include <boost/algorithm/string.hpp>

#include <iostream>


using namespace RTX;
using namespace std;

OdbcDirectPointRecord::OdbcDirectPointRecord() {
  
}

OdbcDirectPointRecord::~OdbcDirectPointRecord() {
  
}


void OdbcDirectPointRecord::dbConnect() throw(RtxException) {
  OdbcPointRecord::dbConnect();
}


vector<string> OdbcDirectPointRecord::identifiers() {
  vector<string> ids;
  if (!isConnected()) {
    return ids;
  }
  
  string tagQuery("");
  tagQuery += "SELECT " + tableDescription.tagNameCol;
  if (!RTX_STRINGS_ARE_EQUAL(tableDescription.tagUnitsCol,"")) {
    tagQuery += ", " + tableDescription.tagUnitsCol;
  }
  tagQuery += " FROM " + tableDescription.tagTable + " ORDER BY " + tableDescription.tagNameCol;
  
  SQLCHAR tagName[512];
  SQLLEN tagLengthInd;
  SQLRETURN retcode;
  
  retcode = SQLAllocHandle(SQL_HANDLE_STMT, _handles.SCADAdbc, &_directStatment);
  retcode = SQLExecDirect(_directStatment, (SQLCHAR*)tagQuery.c_str(), SQL_NTS);
  
  while (SQL_SUCCEEDED(SQLFetch(_directStatment))) {
    SQLGetData(_directStatment, 1, SQL_C_CHAR, tagName, 512, &tagLengthInd);
    string newTag((char*)tagName);
    ids.push_back(newTag);
  }
  
  SQLFreeStmt(_directStatment, SQL_CLOSE);
  SQLFreeHandle(SQL_HANDLE_STMT, _directStatment);
  
  return ids;
}



std::vector<Point> OdbcDirectPointRecord::selectRange(const std::string& id, time_t startTime, time_t endTime) {
  
  scoped_lock<mutex> lock(_odbcMutex);
  
  this->checkConnected();
  
  vector<Point> points;
  
  // construct the static query text
  string q = this->stringQueryForRange(id, startTime, endTime);
  
  // execute the query and get a result set
  SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, _handles.SCADAdbc, &_directStatment);
  retcode = SQLExecDirect(_directStatment, (SQLCHAR*)q.c_str(), SQL_NTS);
  if (SQL_SUCCEEDED(retcode)) {
    points = this->pointsFromStatement(_directStatment);
  }
  else {
    cerr << extract_error("SQLExecDirect", _directStatment, SQL_HANDLE_STMT) << endl;
    cerr << "query did not succeed: " << q << endl;
  }
  SQLFreeHandle(SQL_HANDLE_STMT, _directStatment);
  
  return points;
}

Point OdbcDirectPointRecord::selectNext(const std::string& id, time_t time) {
  scoped_lock<mutex> lock(_odbcMutex);
  this->checkConnected();
  
  Point p;
  vector<Point> points;
  time_t margin = 60*60*12;
  time_t max_margin = this->searchDistance();
  time_t lookahead = time;

  
  string q = this->stringQueryForRange(id, time, time+margin);
  
  SQLAllocHandle(SQL_HANDLE_STMT, _handles.SCADAdbc, &_directStatment);
  
  while (points.size() == 0 && lookahead < time + max_margin) {
    if (SQL_SUCCEEDED(SQLExecDirect(_directStatment, (SQLCHAR*)q.c_str(), SQL_NTS))) {
      points = pointsFromStatement(_directStatment);
    }
    else {
      cerr << "query did not succeed: " << q << endl;
    }
    lookahead += margin;
  }
  
  SQLFreeHandle(SQL_HANDLE_STMT, _directStatment);

  
  if (points.size() > 0) {
    p = points.front();
    int i = 0;
    while (p.time <= time && i < points.size()) {
      p = points.at(i);
      ++i;
    }
  }
  else {
    //cerr << "no points found for " << id << " :: range " << time - 1 << " - " << lookahead + margin << endl;
  }
  
  return p;
}

Point OdbcDirectPointRecord::selectPrevious(const std::string& id, time_t time) {
  scoped_lock<mutex> lock(_odbcMutex);
  this->checkConnected();
  
  Point p;
  vector<Point> points;
  time_t margin = 60*60*12;
  time_t max_margin = this->searchDistance();
  time_t lookBehind = time;
  
  string q = this->stringQueryForRange(id, time-margin, time+1);
  
  SQLAllocHandle(SQL_HANDLE_STMT, _handles.SCADAdbc, &_directStatment);
  
  while (points.size() == 0 && lookBehind > time - max_margin) {
    if (SQL_SUCCEEDED(SQLExecDirect(_directStatment, (SQLCHAR*)q.c_str(), SQL_NTS))) {
      points = pointsFromStatement(_directStatment);
    }
    else {
      cerr << "query did not succeed: " << q << endl;
    }
    lookBehind -= margin;
  }
  
  SQLFreeHandle(SQL_HANDLE_STMT, _directStatment);
  
  
  if (points.size() > 0) {
    p = points.back();
    while ( p.time >= time && points.size() > 0 ) {
      points.pop_back();
      p = points.back();
    }
  }
  else {
    //cerr << "no points found for " << id << " :: range " << time - 1 << " - " << lookahead + margin << endl;
  }
  
  return p;
}


std::string OdbcDirectPointRecord::stringQueryForRange(const std::string& id, time_t start, time_t end) {
  
  string query = _querySyntax.rangeSelect;
  
  string startDateStr = "'" + PointRecordTime::utcDateStringFromUnix(start) + "'";
  string endDateStr = "'" + PointRecordTime::utcDateStringFromUnix(end) + "'";
  string idStr = "'" + id + "'";
  
  boost::replace_first(query, "?", idStr);
  boost::replace_first(query, "?", startDateStr);
  boost::replace_first(query, "?", endDateStr);
  
  return query;
  
}

