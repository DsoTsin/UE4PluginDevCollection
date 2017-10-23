#pragma once

struct sqlite3;

class FDBCursor;

class SQLITEDB_API FDataBase
{
public:
    explicit FDataBase(FString const& PathOfDB);
    virtual ~FDataBase();

    virtual bool        Execute(const TCHAR* Command);
    virtual FDBCursor*  RawQuery(const TCHAR* QueryString);

    virtual void        Close();

private:
    sqlite3*    DB;
    FString     PathDB;
};