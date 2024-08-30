#include <stdio.h>
#include <stdlib.h>
#include "build.h"
#include "error.h"
#include "print.h"
#include "lex.h"
#include "parse.h"
#include "arena.h"

static Project *test_project = 0;

static int jatest_expect_error_x(char *msg, int64_t line_num, char *arg0)
{
	fprintf(stderr, "     expecting error \"%s\" at line %li", msg, line_num);

	if(arg0)
		fprintf(stderr, " with arg0 = %s", (char*)arg0);

	fprintf(stderr, " ...\r");

	for(ErrorRecord *record = error_first; record; record = record->next) {
		if(
			record->msg == msg &&
			record->tok.line->index + 1 == line_num &&
			(arg0 == 0 || record->arglog.argv[0] == arg0)
		) {
			fprintf(stderr, COL_OK("OK") "\n");
			return 1;
		}
	}

	fprintf(stderr, COL_FAIL("FAIL") "\n");
	return 0;
}

static int jatest_expect_error(char *msg, int64_t line_num)
{
	jatest_expect_error_x(msg, line_num, 0);
}

static void jatest_print_membytes()
{
	fprintf(stderr, COL_YELLOW "alloced bytes" COL_RESET ": %zu\n", get_total_alloc());
}

static void jatest_file(char *filename)
{
	test_project = build(filename);
}

static void jatest_reset()
{
	jatest_print_membytes();
	error_reset_records();
	free_arena();
	lex_reset();
}

#ifdef JA_TEST
	int main(int argc, char **argv)
	{
		jatest_print_membytes();

		jatest_file("./tests/test_lex_001_multi_line_comment.ja");
		jatest_expect_error(ERROR_unterminated_ml_comment, 2);
		jatest_reset();

		jatest_file("./tests/test_lex_002_unrecognized_token.ja");
		jatest_expect_error(ERROR_unrecognized_token, 1);
		jatest_reset();

		jatest_file("./tests/test_parse_003_vardecl_no_type_no_init.ja");
		jatest_expect_error(ERROR_vardecl_no_type_no_init, 1);
		jatest_reset();

		jatest_file("./tests/test_parse_004_already_declared.ja");
		jatest_expect_error(ERROR_already_declared, 2);
		jatest_reset();

		jatest_file("./tests/test_parse_005_no_valid_stmt.ja");
		jatest_expect_error(ERROR_no_valid_stmt, 1);
		jatest_reset();

		jatest_file("./tests/test_parse_006_vardecl_expected_ident.ja");
		jatest_expect_error_x(ERROR_expected, 1, token_names[TK_IDENT]);
		jatest_reset();

		jatest_file("./tests/test_parse_007_vardecl_expected_type.ja");
		jatest_expect_error_x(ERROR_expected, 1, EXPECT_type);
		jatest_reset();

		jatest_file("./tests/test_parse_008_vardecl_expected_expr.ja");
		jatest_expect_error_x(ERROR_expected, 1, EXPECT_expr);
		jatest_reset();

		jatest_file("./tests/test_parse_009_vardecl_expected_semicolon.ja");
		jatest_expect_error_x(ERROR_expected, 1, token_names[PT_SEMICOLON]);
		jatest_reset();
	}
#endif