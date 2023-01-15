#pragma once

typedef enum {
	SELECT_ERROR = -1,
	SELECT_TIMEOUT = 0,
	SELECT_READ,
	SELECT_WRITE,
	SELECT_READWRITE,
	SELECT_EXCEPTION
} SelectResults;

typedef enum {
	READ = 0,
	WRITE,
	READ_WRITE
} OperationCodes;
