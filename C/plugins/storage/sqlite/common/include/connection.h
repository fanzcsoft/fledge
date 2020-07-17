#ifndef _CONNECTION_H
#define _CONNECTION_H
/*
 * Fledge storage service.
 *
 * Copyright (c) 2018 OSisoft, LLC
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */

#include <sql_buffer.h>
#include <string>
#include <rapidjson/document.h>
#include <sqlite3.h>
#include <mutex>
#include <reading_stream.h>
#include <map>
#include <vector>
#include <atomic>

#define LEN_BUFFER_DATE 100
#define F_TIMEH24_S             "%H:%M:%S"
#define F_DATEH24_S             "%Y-%m-%d %H:%M:%S"
#define F_DATEH24_M             "%Y-%m-%d %H:%M"
#define F_DATEH24_H             "%Y-%m-%d %H"
// This is the default datetime format in Fledge: 2018-05-03 18:15:00.622
#define F_DATEH24_MS            "%Y-%m-%d %H:%M:%f"
// Format up to seconds
#define F_DATEH24_SEC           "%Y-%m-%d %H:%M:%S"
#define SQLITE3_NOW             "strftime('%Y-%m-%d %H:%M:%f', 'now', 'localtime')"
// The default precision is milliseconds, it adds microseconds and timezone
#define SQLITE3_NOW_READING     "strftime('%Y-%m-%d %H:%M:%f000+00:00', 'now')"
#define SQLITE3_FLEDGE_DATETIME_TYPE "DATETIME"

// Set plugin name for log messages
#ifndef PLUGIN_LOG_NAME
#define PLUGIN_LOG_NAME "SQLite3"
#endif

int dateCallback(void *data, int nCols, char **colValues, char **colNames);
bool applyColumnDateFormat(const std::string& inFormat,
			   const std::string& colName,
			   std::string& outFormat,
			   bool roundMs = false);

bool applyColumnDateFormatLocaltime(const std::string& inFormat,
				    const std::string& colName,
				    std::string& outFormat,
				    bool roundMs = false);

int rowidCallback(void *data,
		  int nCols,
		  char **colValues,
		  char **colNames);

int selectCallback(void *data,
		   int nCols,
		   char **colValues,
		   char **colNames);

int countCallback(void *data,
		  int nCols,
		  char **colValues,
		  char **colNames);

bool applyDateFormat(const std::string& inFormat, std::string& outFormat);

class Connection {
	public:
		Connection();
		~Connection();
#ifndef SQLITE_SPLIT_READINGS
		bool		retrieve(const std::string& table,
					 const std::string& condition,
					 std::string& resultSet);
		int		insert(const std::string& table, const std::string& data);
		int		update(const std::string& table, const std::string& data);
		int		deleteRows(const std::string& table, const std::string& condition);
		int		create_table_snapshot(const std::string& table, const std::string& id);
		int		load_table_snapshot(const std::string& table, const std::string& id);
		int		delete_table_snapshot(const std::string& table, const std::string& id);
		bool		get_table_snapshots(const std::string& table, std::string& resultSet);
#endif
		int		appendReadings(const char *readings);
		int 	readingStream(ReadingStream **readings, bool commit);
		bool		fetchReadings(unsigned long id, unsigned int blksize,
						std::string& resultSet);
		bool		retrieveReadings(const std::string& condition,
						 std::string& resultSet);
		unsigned int	purgeReadings(unsigned long age, unsigned int flags,
						unsigned long sent, std::string& results);
		unsigned int	purgeReadingsByRows(unsigned long rowcount, unsigned int flags,
						unsigned long sent, std::string& results);
		long		tableSize(const std::string& table);
		void		setTrace(bool);
		bool		formatDate(char *formatted_date, size_t formatted_date_size, const char *date);
		bool		aggregateQuery(const rapidjson::Value& payload, std::string& resultSet);
		bool        getNow(std::string& Now);

		//# FIXME_I:
		sqlite3		*getDbHandle() {return dbHandle;};

	private:
		bool 		m_streamOpenTransaction;
		int		m_queuing;
		std::mutex	m_qMutex;
		int 		SQLexec(sqlite3 *db, const char *sql,
					int (*callback)(void*,int,char**,char**),
					void *cbArg, char **errmsg);
		int		SQLstep(sqlite3_stmt *statement);
		bool		m_logSQL;
		void		raiseError(const char *operation, const char *reason,...);
		sqlite3		*dbHandle;
		int		mapResultSet(void *res, std::string& resultSet);
		bool		jsonWhereClause(const rapidjson::Value& whereClause, SQLBuffer&, bool convertLocaltime = false);
		bool		jsonModifiers(const rapidjson::Value&, SQLBuffer&, bool isTableReading = false);
		bool		jsonAggregates(const rapidjson::Value&,
					       const rapidjson::Value&,
					       SQLBuffer&,
					       SQLBuffer&,
					       bool isTableReading = false);
		bool		returnJson(const rapidjson::Value&, SQLBuffer&, SQLBuffer&);
		char		*trim(char *str);
		const std::string
				escape(const std::string&);
		bool		applyColumnDateTimeFormat(sqlite3_stmt *pStmt,
						int i,
						std::string& newDate);
		void		logSQL(const char *, const char *);


};

class ReadingsCatalogue {
	private:
		const int nReadingsAllocate = 20;

		ReadingsCatalogue(){};

		int           getMaxReadingsId();
		int           getnReadingsAllocate() const {return nReadingsAllocate;}
		bool          createReadingsTables(int idStartFrom, int nTables);
		void		  raiseError(const char *operation, const char *reason,...);
		bool          isReadingAvailable() const;
		void          allocateReadingAvailable();
		int           evaluateLastReadingAvailable(Connection *connection);
		int			  SQLStep(sqlite3_stmt *statement);
		int           SQLexec(sqlite3 *dbHandle, const char *sqlCmd);
		int           calculateGlobalId (sqlite3 *dbHandle);

		std::atomic<int> m_globalId;
		int              m_nReadingsTotal = 0;
		int              m_nReadingsAvailable = 0;
		std::mutex       m_mutexAssetReadingCatalogue;
		std::map <std::string, int>   m_AssetReadingCatalogue={

			// asset_code  - reading Table id
			// {"",         1         }
		};

	public:
		static ReadingsCatalogue *getInstance()
		{
			static ReadingsCatalogue *instance = 0;

			if (!instance)
			{
				instance = new ReadingsCatalogue;
			}
			return instance;
		}

		// Returns the global Id and increment it
		int           getGlobalId() {return m_globalId++;};
		bool          evaluateGlobalId();
		bool          storeGlobalId ();

		void          preallocateReadingsTables();
		bool          loadAssetReadingCatalogue();

		int           getNReadingsTotal() const      {return m_nReadingsTotal;}
		int           getNReadingsAvailable() const      {return m_nReadingsAvailable;}
		int           getReadingReference(Connection *connection, const char *asset_code);
};

#endif
