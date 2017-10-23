// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "SQLitePCH.h"
#include "Classes/Cursor.h"
#include "sqlite3.h"

FDBCursor::~FDBCursor()
{
    if (PreparedQuery)
    {
        sqlite3_finalize(PreparedQuery);
    }
}

FDBCursor::FDBCursor(sqlite3_stmt*& InStatement)
: PreparedQuery(InStatement)
{
    StepStatus = sqlite3_step(PreparedQuery);
}


FDBCursor* FDBCursor::MoveNext()
{
    return nullptr;
}

FString FDBCursor::GetString(const TCHAR * Column) const
{
    return FString() /*FString(UTF8_TO_TCHAR(sqlite3_column_text(PreparedQuery, ColumnIndex)));*/;
}

int32 FDBCursor::GetInt(const TCHAR * Column) const
{
    return int32();
}

float FDBCursor::GetFloat(const TCHAR * Column) const
{
    return 0.0f;
}

int64 FDBCursor::GetBigInteger(const TCHAR * Column) const
{
    return int64();
}

void FDBCursor::BindData(void * Data, uint64 Size)
{
}
