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

#define _DB_NAME                  "/fledge.sqlite"
#define READINGS_DB_NAME_BASE     "readings"
#define READINGS_DB_FILE_NAME     "/" READINGS_DB_NAME_BASE "_1.db"
#define READINGS_DB               READINGS_DB_NAME_BASE "_1"
#define READINGS_TABLE            "readings"
#define READINGS_TABLE_MEM       READINGS_TABLE "_1"

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

#define  DB_CONFIGURATION "PRAGMA busy_timeout = 5000; PRAGMA cache_size = -4000; PRAGMA journal_mode = WAL; PRAGMA secure_delete = off; PRAGMA journal_size_limit = 4096000;"

// Set plugin name for log messages
#ifndef PLUGIN_LOG_NAME
#define PLUGIN_LOG_NAME "SQLite3"
#endif

/*
 * Control the way purge deletes readings. The block size sets a limit as to how many rows
 * get deleted in each call, whilst the sleep interval controls how long the thread sleeps
 * between deletes. The idea is to not keep the database locked too long and allow other threads
 * to have access to the database between blocks.
 */
#define PURGE_SLEEP_MS 500
#define PURGE_DELETE_BLOCK_SIZE	20
#define TARGET_PURGE_BLOCK_DEL_TIME	(70*1000) 	// 70 msec
#define PURGE_BLOCK_SZ_GRANULARITY	5 	// 5 rows
#define MIN_PURGE_DELETE_BLOCK_SIZE	20
#define MAX_PURGE_DELETE_BLOCK_SIZE	1500
#define RECALC_PURGE_BLOCK_SIZE_NUM_BLOCKS	30	// recalculate purge block size after every 30 blocks

#define PURGE_SLOWDOWN_AFTER_BLOCKS 5
#define PURGE_SLOWDOWN_SLEEP_MS 500

#define SECONDS_PER_DAY "86400.0"
// 2440587.5 is the julian day at 1/1/1970 0:00 UTC.
#define JULIAN_DAY_START_UNIXTIME "2440587.5"


#define START_TIME std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#define END_TIME std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now(); \
				 auto usecs = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();

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

		std::string   generateDbAlias(int dbId);
		std::string   generateDbName(int tableId);
		std::string   generateDbFileName(int dbId);
		std::string   generateDbNameFromTableId(int tableId);
		std::string   generateReadingsName(int tableId);
		void          getAllDbs(std::vector<int> &dbIdList);
		int           getMaxReadingsId();
		int           getNReadingsAvailable() const      {return m_nReadingsAvailable;}
		int           getGlobalId() {return m_globalId++;};
		bool          evaluateGlobalId();
		bool          storeGlobalId ();

		void          preallocateReadingsTables();
		bool          loadAssetReadingCatalogue();
		bool          createNewDB();
		int           getReadingReference(Connection *connection, const char *asset_code);
		bool          attachAllDbs();
		std::string   sqlConstructMultiDb(std::string &sqlCmdBase);
		int           purgeAllReadings(sqlite3 *dbHandle, const char *sqlCmdBase, char **errMsg = NULL, unsigned int *rowsAffected = NULL);

	private:
		const int nReadingsAllocate = 15;

		typedef struct ReadingAvailable {
			int lastReadings;
			int tableCount;

		} tyReadingsAvailable;

		ReadingsCatalogue(){};

		int           getUsedTablesDbId(int dbId);
		int           getNReadingsAllocate() const {return nReadingsAllocate;}
		bool          createReadingsTables(int dbId, int idStartFrom, int nTables);
		bool          isReadingAvailable() const;
		void          allocateReadingAvailable();
		tyReadingsAvailable   evaluateLastReadingAvailable(int dbId);
		int           calculateGlobalId (sqlite3 *dbHandle);
		std::string   generateDbFilePah(int dbId);

		void		  raiseError(const char *operation, const char *reason,...);
		int			  SQLStep(sqlite3_stmt *statement);
		int           SQLExec(sqlite3 *dbHandle, const char *sqlCmd,  char **errMsg = NULL);
		bool          enableWAL(std::string &dbPathReadings);

		int                                           m_dbId;
		std::atomic<int>                              m_globalId;
		int                                           m_nReadingsAvailable = 0;
		std::mutex                                    m_mutexAssetReadingCatalogue;
		std::map <std::string, std::pair<int, int>>   m_AssetReadingCatalogue={

			// asset_code  - reading Table Id, Db Id
			// {"",         ,{1               ,1 }}
		};

};

#endif
