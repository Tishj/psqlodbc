/*
 * This test case tests for a bug in result set caching, with
 * UseDeclareFetch=1, that was fixed. The bug occurred when a cursor was
 * closed, due to transaction commit, before any rows were fetched from
 * it. That set the "base" of the internal cached rowset incorrectly,
 * off by one.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

TEST_CASE("cursor-commit-test", "[odbc]") {
	test_printf_reset();

	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLCHAR		charval[100];
	SQLLEN		len;
	int			row;

	test_connect();

	/* Start a transaction */
	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		REQUIRE(1==0);
	}

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	/*
	 * Begin executing a query
	 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT g FROM generate_series(1,3) g(g)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLBindCol(hstmt, 1, SQL_C_CHAR, &charval, sizeof(charval), &len);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	/* Commit. This implicitly closes the cursor in the server. */
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to commit", SQL_HANDLE_DBC, conn);
		REQUIRE(1==0);
	}

	rc = SQLFetchScroll(hstmt, SQL_FETCH_FIRST, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll(FIRST) failed", hstmt);

	if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO)
		test_printf("first row: %s\n", charval);

	row = 1;
	while (1)
	{
		rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
		if (rc == SQL_NO_DATA)
			break;

		row++;

		if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO)
			test_printf("row %d: %s\n", row, charval);
		else
		{
			print_diag("SQLFetchScroll failed", SQL_HANDLE_STMT, hstmt);
			REQUIRE(1==0);
		}
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	// clean up statement
	release_statement(hstmt);

	/* Clean up */
	test_disconnect();

	test_check_result("cursor-commit");

	return;
}
