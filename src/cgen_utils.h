
void write(char *msg, ...);
int is_in_header();
Unit *get_cur_unit();
void gen_type_postfix(Type *dtype);
void gen_type(Type *dtype);
void gen_init_expr(Expr *expr);
void gen_expr(Expr *expr);
