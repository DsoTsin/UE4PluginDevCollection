// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

struct sqlite3_stmt;

class SQLITEDB_API FDBCursor
{
public:

    virtual ~FDBCursor();

    virtual FDBCursor* MoveNext();

    virtual FString GetString(const TCHAR* Column) const;
    virtual int32 GetInt(const TCHAR* Column) const;
    virtual float GetFloat(const TCHAR* Column) const;
    virtual int64 GetBigInteger(const TCHAR* Column) const;
    
    virtual void BindData(void* Data, uint64 Size);

    friend class FDataBase;

private:
	FDBCursor(sqlite3_stmt*& InStatement);

    sqlite3_stmt*   PreparedQuery;
    int32           StepStatus = 0;
};