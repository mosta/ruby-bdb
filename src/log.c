#include "bdb.h"

struct dblsnst {
    VALUE env;
    DB_LSN *lsn;
};

static void
mark_lsn(lsnst)
    struct dblsnst *lsnst;
{
    rb_gc_mark(lsnst->env);
}

static void
free_lsn(lsnst)
    struct dblsnst *lsnst;
{
    if (lsnst->lsn) free(lsnst->lsn);
    free(lsnst);
}

static VALUE
MakeLsn(env)
    VALUE env;
{
    bdb_ENV *dbenvst;
    struct dblsnst *lsnst;
    VALUE res;

    GetEnvDB(env, dbenvst);
    res = Data_Make_Struct(bdb_cLsn, struct dblsnst, mark_lsn, free_lsn, lsnst);
    lsnst->env = env;
    lsnst->lsn = ALLOC(DB_LSN);
    return res;
}

static VALUE
bdb_s_log_put_internal(obj, a, flag)
    VALUE obj, a;
    int flag;
{
    bdb_ENV *dbenvst;
    VALUE ret;
    DBT data;
    struct dblsnst *lsnst;

    GetEnvDB(obj, dbenvst);
    if (TYPE(a) != T_STRING) a = rb_str_to_str(a);
    ret = MakeLsn(obj);
    Data_Get_Struct(ret, struct dblsnst, lsnst);
    data.data = RSTRING(a)->ptr;
    data.size = RSTRING(a)->len;
#if DB_VERSION_MAJOR < 3
    if (!dbenvst->dbenvp->lg_info) {
	rb_raise(bdb_eFatal, "log region not open");
    }
    bdb_test_error(log_put(dbenvst->dbenvp->lg_info, lsnst->lsn, &data, flag));
#else
    bdb_test_error(log_put(dbenvst->dbenvp, lsnst->lsn, &data, flag));
#endif
    return ret;
}

static VALUE
bdb_s_log_put(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE a, b;
    int flag;
    
    if (argc == 0 || argc > 2) {
	rb_raise(bdb_eFatal, "Invalid number of arguments");
    }
    flag = DB_CHECKPOINT;
    if (rb_scan_args(argc, argv, "11", &a, &b) == 2) {
	flag = NUM2INT(b);
    }
    return bdb_s_log_put_internal(obj, a, flag);
}

static VALUE
bdb_s_log_checkpoint(obj, a)
    VALUE obj, a;
{
    return bdb_s_log_put_internal(obj, a, DB_CHECKPOINT);
}

static VALUE
bdb_s_log_flush(argc, argv, obj)
    int argc;
    VALUE obj, *argv;
{
    bdb_ENV *dbenvst;

    if (argc == 0) {
	GetEnvDB(obj, dbenvst);
#if DB_VERSION_MAJOR < 3
	if (!dbenvst->dbenvp->lg_info) {
	    rb_raise(bdb_eFatal, "log region not open");
	}
	bdb_test_error(log_flush(dbenvst->dbenvp->lg_info, NULL));
#else
	bdb_test_error(log_flush(dbenvst->dbenvp, NULL));
#endif
	return obj;
    }
    else if (argc == 1) {
	return bdb_s_log_put_internal(obj, argv[0], DB_FLUSH);
    }
    else {
	rb_raise(bdb_eFatal, "Invalid number of arguments");
    }
}

static VALUE
bdb_s_log_curlsn(obj, a)
    VALUE obj, a;
{
    return bdb_s_log_put_internal(obj, a, DB_CURLSN);
}
  

static VALUE
bdb_env_log_stat(obj)
    VALUE obj;
{
    DB_LOG_STAT *stat;
    bdb_ENV *dbenvst;
    VALUE res;

    GetEnvDB(obj, dbenvst);
#if DB_VERSION_MAJOR < 3
    if (!dbenvst->dbenvp->lg_info) {
	rb_raise(bdb_eFatal, "log region not open");
    }
    bdb_test_error(log_stat(dbenvst->dbenvp->lg_info, &stat, 0));
#else
    bdb_test_error(log_stat(dbenvst->dbenvp, &stat, 0));
#endif
    res = rb_hash_new();
    rb_hash_aset(res, rb_tainted_str_new2("st_magic"), INT2NUM(stat->st_magic));
    rb_hash_aset(res, rb_tainted_str_new2("st_version"), INT2NUM(stat->st_version));
    rb_hash_aset(res, rb_tainted_str_new2("st_regsize"), INT2NUM(stat->st_regsize));
    rb_hash_aset(res, rb_tainted_str_new2("st_mode"), INT2NUM(stat->st_mode));
#if DB_VERSION_MAJOR < 3
    rb_hash_aset(res, rb_tainted_str_new2("st_refcnt"), INT2NUM(stat->st_refcnt));
#else
    rb_hash_aset(res, rb_tainted_str_new2("st_lg_bsize"), INT2NUM(stat->st_lg_bsize));
#endif
    rb_hash_aset(res, rb_tainted_str_new2("st_lg_max"), INT2NUM(stat->st_lg_max));
    rb_hash_aset(res, rb_tainted_str_new2("st_w_mbytes"), INT2NUM(stat->st_w_mbytes));
    rb_hash_aset(res, rb_tainted_str_new2("st_w_bytes"), INT2NUM(stat->st_w_bytes));
    rb_hash_aset(res, rb_tainted_str_new2("st_wc_mbytes"), INT2NUM(stat->st_wc_mbytes));
    rb_hash_aset(res, rb_tainted_str_new2("st_wc_bytes"), INT2NUM(stat->st_wc_bytes));
    rb_hash_aset(res, rb_tainted_str_new2("st_wcount"), INT2NUM(stat->st_wcount));
#if DB_VERSION_MAJOR == 3
    rb_hash_aset(res, rb_tainted_str_new2("st_wcount_fill"), INT2NUM(stat->st_wcount_fill));
#endif
    rb_hash_aset(res, rb_tainted_str_new2("st_scount"), INT2NUM(stat->st_scount));
    rb_hash_aset(res, rb_tainted_str_new2("st_cur_file"), INT2NUM(stat->st_cur_file));
    rb_hash_aset(res, rb_tainted_str_new2("st_cur_offset"), INT2NUM(stat->st_cur_offset));
    rb_hash_aset(res, rb_tainted_str_new2("st_region_wait"), INT2NUM(stat->st_region_wait));
    rb_hash_aset(res, rb_tainted_str_new2("st_region_nowait"), INT2NUM(stat->st_region_nowait));
    free(stat);
    return res;
}

static VALUE
bdb_env_log_get(obj, a)
    VALUE obj, a;
{
    bdb_ENV *dbenvst;
    DBT data;
    struct dblsnst *lsnst;
    VALUE res, lsn;
    int ret, flag;

    GetEnvDB(obj, dbenvst);
    flag = NUM2INT(a);
    memset(&data, 0, sizeof(data));
    data.flags |= DB_DBT_MALLOC;
    lsn = MakeLsn(obj);
    Data_Get_Struct(lsn, struct dblsnst, lsnst);
#if DB_VERSION_MAJOR < 3
    if (!dbenvst->dbenvp->lg_info) {
	rb_raise(bdb_eFatal, "log region not open");
    }
    ret = bdb_test_error(log_get(dbenvst->dbenvp->lg_info, lsnst->lsn, &data, flag));
#else
    ret = bdb_test_error(log_get(dbenvst->dbenvp, lsnst->lsn, &data, flag));
#endif
    if (ret == DB_NOTFOUND) {
	return Qnil;
    }
    res = rb_tainted_str_new(data.data, data.size);
    free(data.data);
    return rb_assoc_new(res, lsn);
}

static VALUE
bdb_i_each_log_get(obj, flag)
    VALUE obj;
    int flag;
{
    bdb_ENV *dbenvst;
    struct dblsnst *lsnst;
    DBT data;
    VALUE lsn, res;
    int ret, init, flags;

    GetEnvDB(obj, dbenvst);
    init = 0; /* strange ??? */
    do {
	lsn = MakeLsn(obj);
	Data_Get_Struct(lsn, struct dblsnst, lsnst);
	memset(&data, 0, sizeof(data));
	data.flags |= DB_DBT_MALLOC;
	if (!init) {
	    flags = (flag == DB_NEXT)?DB_FIRST:DB_LAST;
	    init = 1;
	}
	else
	    flags = flag;
#if DB_VERSION_MAJOR < 3
	if (!dbenvst->dbenvp->lg_info) {
	    rb_raise(bdb_eFatal, "log region not open");
	}
	ret = bdb_test_error(log_get(dbenvst->dbenvp->lg_info, lsnst->lsn, &data, flags));
#else
	ret = bdb_test_error(log_get(dbenvst->dbenvp, lsnst->lsn, &data, flags));
#endif
	if (ret == DB_NOTFOUND) {
	    return Qnil;
	}
	res = rb_tainted_str_new(data.data, data.size);
	free(data.data);
	rb_yield(rb_assoc_new(res, lsn));
    } while (1);
    return Qnil;
}

static VALUE
bdb_env_log_each(obj) 
    VALUE obj;
{ 
    return bdb_i_each_log_get(obj, DB_NEXT);
}

static VALUE
bdb_env_log_hcae(obj)
    VALUE obj;
{ 
    return bdb_i_each_log_get(obj, DB_PREV);
}
 
static VALUE
bdb_env_log_archive(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    char **list, **file;
    bdb_ENV *dbenvst;
    int flag;
    VALUE res;

    GetEnvDB(obj, dbenvst);
    flag = 0;
    list = NULL;
    if (rb_scan_args(argc, argv, "01", &res)) {
	flag = NUM2INT(res);
    }
#if DB_VERSION_MAJOR < 3
    if (!dbenvst->dbenvp->lg_info) {
	rb_raise(bdb_eFatal, "log region not open");
    }
    bdb_test_error(log_archive(dbenvst->dbenvp->lg_info, &list, flag, NULL));
#else
    bdb_test_error(log_archive(dbenvst->dbenvp, &list, flag, NULL));
#endif
    res = rb_ary_new();
    for (file = list; file != NULL && *file != NULL; file++) {
	rb_ary_push(res, rb_tainted_str_new2(*file));
    }
    if (list != NULL) free(list);
    return res;
}

#define GetLsn(obj, lsnst, dbenvst)			\
{							\
    Data_Get_Struct(obj, struct dblsnst, lsnst);	\
    GetEnvDB(lsnst->env, dbenvst);			\
}

static VALUE
bdb_lsn_log_file(obj)
    VALUE obj;
{
    struct dblsnst *lsnst;
    bdb_ENV *dbenvst;
    char name[2048];

    GetLsn(obj, lsnst, dbenvst);
#if DB_VERSION_MAJOR < 3
    if (!dbenvst->dbenvp->lg_info) {
	rb_raise(bdb_eFatal, "log region not open");
    }
    bdb_test_error(log_file(dbenvst->dbenvp->lg_info, lsnst->lsn, name, 2048));
#else
    bdb_test_error(log_file(dbenvst->dbenvp, lsnst->lsn, name, 2048));
#endif
    return rb_tainted_str_new2(name);
}

static VALUE
bdb_lsn_log_flush(obj)
    VALUE obj;
{
    struct dblsnst *lsnst;
    bdb_ENV *dbenvst;

    GetLsn(obj, lsnst, dbenvst);
#if DB_VERSION_MAJOR < 3
    if (!dbenvst->dbenvp->lg_info) {
	rb_raise(bdb_eFatal, "log region not open");
    }
    bdb_test_error(log_flush(dbenvst->dbenvp->lg_info, lsnst->lsn));
#else
    bdb_test_error(log_flush(dbenvst->dbenvp, lsnst->lsn));
#endif
    return obj;
}

static VALUE
bdb_lsn_log_compare(obj, a)
    VALUE obj, a;
{
    struct dblsnst *lsnst1, *lsnst2;
    bdb_ENV *dbenvst1, *dbenvst2;

    if (!rb_obj_is_kind_of(a, bdb_cLsn)) {
	rb_raise(bdb_eFatal, "invalid argument for <=>");
    }
    GetLsn(obj, lsnst1, dbenvst1);
    GetLsn(a, lsnst2, dbenvst2);
    return INT2NUM(log_compare(lsnst1->lsn, lsnst2->lsn));
}

static VALUE
bdb_lsn_log_get(obj)
    VALUE obj;
{
    struct dblsnst *lsnst;
    bdb_ENV *dbenvst;
    DBT data;
    VALUE res;
    int ret;

    GetLsn(obj, lsnst, dbenvst);
    memset(&data, 0, sizeof(data));
    data.flags |= DB_DBT_MALLOC;
#if DB_VERSION_MAJOR < 3
    if (!dbenvst->dbenvp->lg_info) {
	rb_raise(bdb_eFatal, "log region not open");
    }
    ret = bdb_test_error(log_get(dbenvst->dbenvp->lg_info, lsnst->lsn, &data, DB_SET));
#else
    ret = bdb_test_error(log_get(dbenvst->dbenvp, lsnst->lsn, &data, DB_SET));
#endif
    if (ret == DB_NOTFOUND) {
	return Qnil;
    }
    res = rb_tainted_str_new(data.data, data.size);
    free(data.data);
    return res;
}

static VALUE
bdb_log_register(obj, a)
    VALUE obj, a;
{
    bdb_DB *dbst;
    bdb_ENV *dbenvst;

    if (TYPE(a) != T_STRING) {
	rb_raise(bdb_eFatal, "Need a filename");
    }
    if (bdb_has_env(obj) == Qfalse) {
	rb_raise(bdb_eFatal, "Database must be open in an Env");
    }
    Data_Get_Struct(obj, bdb_DB, dbst);
    Data_Get_Struct(dbst->env, bdb_ENV, dbenvst);
#if DB_VERSION_MAJOR < 3
    if (!dbenvst->dbenvp->lg_info) {
	rb_raise(bdb_eFatal, "log region not open");
    }
    bdb_test_error(log_register(dbenvst->dbenvp->lg_info, dbst->dbp, RSTRING(a)->ptr, dbst->type, &dbenvst->fidp));
#else
#if DB_VERSION_MINOR < 1 || (DB_VERSION_MINOR == 1 && DB_VERSION_PATCH <= 5)
    bdb_test_error(log_register(dbenvst->dbenvp, dbst->dbp, RSTRING(a)->ptr, &dbenvst->fidp));
#else
    bdb_test_error(log_register(dbenvst->dbenvp, dbst->dbp, RSTRING(a)->ptr));
#endif
#endif
    return obj;
}

static VALUE
bdb_log_unregister(obj)
    VALUE obj;
{
    bdb_DB *dbst;
    bdb_ENV *dbenvst;

    if (bdb_has_env(obj) == Qfalse) {
	rb_raise(bdb_eFatal, "Database must be open in an Env");
    }
    Data_Get_Struct(obj, bdb_DB, dbst);
    Data_Get_Struct(dbst->env, bdb_ENV, dbenvst);
#if DB_VERSION_MAJOR < 3
    if (!dbenvst->dbenvp->lg_info) {
	rb_raise(bdb_eFatal, "log region not open");
    }
    bdb_test_error(log_unregister(dbenvst->dbenvp->lg_info, dbenvst->fidp));
#else
#if DB_VERSION_MINOR < 1 || (DB_VERSION_MINOR == 1 && DB_VERSION_PATCH <= 5)
    bdb_test_error(log_unregister(dbenvst->dbenvp, dbenvst->fidp));
#else
    bdb_test_error(log_unregister(dbenvst->dbenvp, dbst->dbp));
#endif
#endif
    return obj;
}

void Init_log()
{
    rb_define_method(bdb_cEnv, "log_put", bdb_s_log_put, -1);
    rb_define_method(bdb_cEnv, "log_curlsn", bdb_s_log_curlsn, 0);
    rb_define_method(bdb_cEnv, "log_checkpoint", bdb_s_log_checkpoint, 1);
    rb_define_method(bdb_cEnv, "log_flush", bdb_s_log_flush, -1);
    rb_define_method(bdb_cEnv, "log_stat", bdb_env_log_stat, 0);
    rb_define_method(bdb_cEnv, "log_archive", bdb_env_log_archive, -1);
    rb_define_method(bdb_cEnv, "log_get", bdb_env_log_get, 1);
    rb_define_method(bdb_cEnv, "log_each", bdb_env_log_each, 0);
    rb_define_method(bdb_cEnv, "log_reverse_each", bdb_env_log_hcae, 0);
    rb_define_method(bdb_cCommon, "log_register", bdb_log_register, 1);
    rb_define_method(bdb_cCommon, "log_unregister", bdb_log_unregister, 0);
    bdb_cLsn = rb_define_class_under(bdb_mDb, "Lsn", rb_cObject);
    rb_include_module(bdb_cLsn, rb_mComparable);
    rb_undef_method(CLASS_OF(bdb_cLsn), "new");
    rb_define_method(bdb_cLsn, "log_get", bdb_lsn_log_get, 0);
    rb_define_method(bdb_cLsn, "get", bdb_lsn_log_get, 0);
    rb_define_method(bdb_cLsn, "log_compare", bdb_lsn_log_compare, 1);
    rb_define_method(bdb_cLsn, "compare", bdb_lsn_log_compare, 1);
    rb_define_method(bdb_cLsn, "<=>", bdb_lsn_log_compare, 1);
    rb_define_method(bdb_cLsn, "log_file", bdb_lsn_log_file, 0);
    rb_define_method(bdb_cLsn, "file", bdb_lsn_log_file, 0);
    rb_define_method(bdb_cLsn, "log_flush", bdb_lsn_log_flush, 0);
    rb_define_method(bdb_cLsn, "flush", bdb_lsn_log_flush, 0);
}
