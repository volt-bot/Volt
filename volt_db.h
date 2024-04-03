#pragma once
#include <iostream>
#include <string.h>
#include <chrono>
#include "../../../../Users/user/source/sqlite3/sqlite3.h"

namespace Volt
{
	namespace DB{

		struct SQLiteResourceDeleter
		{
			void operator()(sqlite3 *sqlite3_) const
			{
				if (sqlite3_ != nullptr)
					sqlite3_close(sqlite3_);
				sqlite3_ = nullptr;
				std::cout<<"sqlite_database handle destroyed!"<<std::endl;
			}
		};

		std::shared_ptr<sqlite3>
		CreateSharedSQLite3(const char *_path)
		{
			sqlite3* tmp_db_ = nullptr;
			if (SQLITE_OK != sqlite3_open(_path, &tmp_db_))
			{
				std::cout<<"error loading database: "<<_path<<std::endl;
				sqlite3_close(tmp_db_);
				return nullptr;
			}
			else {
				return std::shared_ptr<sqlite3>(tmp_db_, SQLiteResourceDeleter());
			}
		}

		class SQLiteDatabase{
			public:
				void setDatabase(){}

				virtual bool loadDatabase(std::string _path)
				{
					bool result = false;
					shared_database_ = CreateSharedSQLite3(_path.data());
					if(nullptr != shared_database_)
						result = true;
					return result;
				}

				virtual bool loadDataBase(sqlite3* database) {
					return false;
				}

				std::shared_ptr<sqlite3> getDatabase(){ return this->shared_database_; }

				SQLiteDatabase& InsertInto(const std::string& table) {
    			    query_ = "INSERT INTO " + table;
    			    return *this;
    			}

    			SQLiteDatabase& Select(const std::string& fields) {
    			    query_ = "SELECT " + fields;
    			    return *this;
    			}

				SQLiteDatabase& From(const std::string& table) {
    			    query_ += " FROM " + table;
    			    return *this;
    			}

    			SQLiteDatabase& Update(const std::string& table) {
    			    query_ = "UPDATE " + table;
    			    return *this;
    			}

    			SQLiteDatabase& Where(const std::string& condition) {
    			    query_ += " WHERE " + condition;
    			    return *this;
    			}

    			SQLiteDatabase& And(const std::string& table) {
    			    query_ += " AND " + table;
    			    return *this;
    			}

				SQLiteDatabase& OrderBy(const std::string& table) {
    			    query_ += " ORDER_BY " + table;
    			    return *this;
    			}

				void setQuery(std::string _query) { query_ = _query; }

				std::string getQuery() { return query_; }

				void execute(){
					char* errMsg;
    			    int rc = sqlite3_exec(shared_database_.get(), query_.c_str(), nullptr, nullptr, &errMsg);
    			    if (rc != SQLITE_OK) {
    			        std::cerr << "SQL error: " << errMsg << "\n";
    			        sqlite3_free(errMsg);
    			    }
    			    query_.clear();
				}
			protected:
			std::string query_{};
			std::shared_ptr<sqlite3> shared_database_;
		};
	}
}
