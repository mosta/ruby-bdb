#include <ruby.h>
#include <db.h>
#include <errno.h>

#include "bdb.h"

VALUE bdb_cEnv;
VALUE bdb_eFatal;
VALUE bdb_eLock, bdb_eLockDead, bdb_eLockHeld, bdb_eLockGranted;
VALUE bdb_mDb;
VALUE bdb_cCommon, bdb_cBtree, bdb_cRecnum, bdb_cHash, bdb_cRecno, bdb_cUnknown;
VALUE bdb_cDelegate;

#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 1
VALUE bdb_sKeyrange;
#endif

#if DB_VERSION_MAJOR == 3
VALUE bdb_cQueue;
#endif

VALUE bdb_cTxn, bdb_cTxnCatch;
VALUE bdb_cCursor;
VALUE bdb_cLock, bdb_cLockid;
VALUE bdb_cLsn;

VALUE bdb_mMarshal;

ID id_dump, id_load;
ID id_current_db;

VALUE bdb_errstr;
int bdb_errcall = 0;

#if DB_VERSION_MAJOR < 3

char *
db_strerror(int err)
{
    if (err == 0)
        return "" ;

    if (err > 0)
        return strerror(err) ;

    switch (err) {
    case DB_INCOMPLETE:
        return ("DB_INCOMPLETE: Sync was unable to complete");
    case DB_KEYEMPTY:
        return ("DB_KEYEMPTY: Non-existent key/data pair");
    case DB_KEYEXIST:
        return ("DB_KEYEXIST: Key/data pair already exists");
    case DB_LOCK_DEADLOCK:
        return ("DB_LOCK_DEADLOCK: Locker killed to resolve a deadlock");
    case DB_LOCK_NOTGRANTED:
        return ("DB_LOCK_NOTGRANTED: Lock not granted");
    case DB_LOCK_NOTHELD:
        return ("DB_LOCK_NOTHELD: Lock not held by locker");
    case DB_NOTFOUND:
        return ("DB_NOTFOUND: No matching key/data pair found");
#if DB_VERSION_MINOR >= 6
    case DB_RUNRECOVERY:
        return ("DB_RUNRECOVERY: Fatal error, run database recovery");
#endif
    default:
        return "Unknown Error" ;
    }
}

#endif

int
bdb_test_error(comm)
    int comm;
{
    VALUE error;

    switch (comm) {
    case 0:
    case DB_NOTFOUND:
    case DB_KEYEMPTY:
    case DB_KEYEXIST:
	return comm;
        break;
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 2
    case DB_INCOMPLETE:
	comm = 0;
	return comm;
        break;
#endif
    case DB_LOCK_DEADLOCK:
    case EAGAIN:
	error = bdb_eLockDead;
	break;
    case DB_LOCK_NOTGRANTED:
	error = bdb_eLockGranted;
	break;
#if DB_VERSION_MAJOR < 3
    case DB_LOCK_NOTHELD:
	error = bdb_eLockHeld;
	break;
#endif
    default:
	error = bdb_eFatal;
	break;
    }
    if (bdb_errcall) {
	bdb_errcall = 0;
	rb_raise(error, "%s -- %s", RSTRING(bdb_errstr)->ptr, db_strerror(comm));
    }
    else
	rb_raise(error, "%s", db_strerror(comm));
}

void
Init_bdb()
{
    int major, minor, patch;
    VALUE version;
    version = rb_tainted_str_new2(db_version(&major, &minor, &patch));
    if (major != DB_VERSION_MAJOR || minor != DB_VERSION_MINOR
	|| patch != DB_VERSION_PATCH) {
        rb_raise(rb_eNotImpError, "\nBDB needs compatible versions of libdb & db.h\n\tyou have db.h version %d.%d.%d and libdb version %d.%d.%d\n",
		 DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
		 major, minor, patch);
    }
    bdb_mMarshal = rb_const_get(rb_cObject, rb_intern("Marshal"));
    id_current_db = rb_intern("bdb_current_db");
    id_dump = rb_intern("dump");
    id_load = rb_intern("load");
    bdb_mDb = rb_define_module("BDB");
    bdb_eFatal = rb_define_class_under(bdb_mDb, "Fatal", rb_eStandardError);
    bdb_eLock = rb_define_class_under(bdb_mDb, "LockError", bdb_eFatal);
    bdb_eLockDead = rb_define_class_under(bdb_mDb, "LockDead", bdb_eLock);
    bdb_eLockHeld = rb_define_class_under(bdb_mDb, "LockHeld", bdb_eLock);
    bdb_eLockGranted = rb_define_class_under(bdb_mDb, "LockGranted",  bdb_eLock);
/* CONSTANT */
    rb_define_const(bdb_mDb, "VERSION", version);
    rb_define_const(bdb_mDb, "VERSION_MAJOR", INT2FIX(major));
    rb_define_const(bdb_mDb, "VERSION_MINOR", INT2FIX(minor));
    rb_define_const(bdb_mDb, "VERSION_PATCH", INT2FIX(patch));
    rb_define_const(bdb_mDb, "BTREE", INT2FIX(DB_BTREE));
    rb_define_const(bdb_mDb, "HASH", INT2FIX(DB_HASH));
    rb_define_const(bdb_mDb, "RECNO", INT2FIX(DB_RECNO));
#if DB_VERSION_MAJOR < 3
    rb_define_const(bdb_mDb, "QUEUE", INT2FIX(0));
#else
    rb_define_const(bdb_mDb, "QUEUE", INT2FIX(DB_QUEUE));
#endif
    rb_define_const(bdb_mDb, "UNKNOWN", INT2FIX(DB_UNKNOWN));
    rb_define_const(bdb_mDb, "AFTER", INT2FIX(DB_AFTER));
    rb_define_const(bdb_mDb, "APPEND", INT2FIX(DB_APPEND));
    rb_define_const(bdb_mDb, "ARCH_ABS", INT2FIX(DB_ARCH_ABS));
    rb_define_const(bdb_mDb, "ARCH_DATA", INT2FIX(DB_ARCH_DATA));
    rb_define_const(bdb_mDb, "ARCH_LOG", INT2FIX(DB_ARCH_LOG));
    rb_define_const(bdb_mDb, "BEFORE", INT2FIX(DB_BEFORE));
    rb_define_const(bdb_mDb, "CHECKPOINT", INT2FIX(DB_CHECKPOINT));
#if DB_VERSION_MAJOR < 3
    rb_define_const(bdb_mDb, "CONSUME", INT2FIX(0));
#else
    rb_define_const(bdb_mDb, "CONSUME", INT2FIX(DB_CONSUME));
#ifdef DB_CDB_ALLDB
    rb_define_const(bdb_mDb, "CDB_ALLDB", INT2FIX(DB_CDB_ALLDB));
#endif
#endif
    rb_define_const(bdb_mDb, "CREATE", INT2FIX(DB_CREATE));
    rb_define_const(bdb_mDb, "CURLSN", INT2FIX(DB_CURLSN));
    rb_define_const(bdb_mDb, "CURRENT", INT2FIX(DB_CURRENT));
#if DB_VERSION_MAJOR < 3
    rb_define_const(bdb_mDb, "DB_VERB_CHKPOINT", INT2FIX(1));
    rb_define_const(bdb_mDb, "DB_VERB_DEADLOCK", INT2FIX(1));
    rb_define_const(bdb_mDb, "DB_VERB_RECOVERY", INT2FIX(1));
    rb_define_const(bdb_mDb, "DB_VERB_WAITSFOR", INT2FIX(1));
#else
    rb_define_const(bdb_mDb, "DB_VERB_CHKPOINT", INT2FIX(DB_VERB_CHKPOINT));
    rb_define_const(bdb_mDb, "DB_VERB_DEADLOCK", INT2FIX(DB_VERB_DEADLOCK));
    rb_define_const(bdb_mDb, "DB_VERB_RECOVERY", INT2FIX(DB_VERB_RECOVERY));
    rb_define_const(bdb_mDb, "DB_VERB_WAITSFOR", INT2FIX(DB_VERB_WAITSFOR));
#endif
    rb_define_const(bdb_mDb, "DBT_MALLOC", INT2FIX(DB_DBT_MALLOC));
    rb_define_const(bdb_mDb, "DBT_PARTIAL", INT2FIX(DB_DBT_PARTIAL));
#if DB_VERSION_MAJOR < 3
    rb_define_const(bdb_mDb, "DBT_REALLOC", INT2FIX(0));
#else
    rb_define_const(bdb_mDb, "DBT_REALLOC", INT2FIX(DB_DBT_REALLOC));
#endif
    rb_define_const(bdb_mDb, "DBT_USERMEM", INT2FIX(DB_DBT_USERMEM));
    rb_define_const(bdb_mDb, "DUP", INT2FIX(DB_DUP));
#ifdef DB_DUPSORT
    rb_define_const(bdb_mDb, "DUPSORT", INT2FIX(DB_DUPSORT));
#endif
    rb_define_const(bdb_mDb, "FIRST", INT2FIX(DB_FIRST));
    rb_define_const(bdb_mDb, "FLUSH", INT2FIX(DB_FLUSH));
#if DB_VERSION_MAJOR < 3
    rb_define_const(bdb_mDb, "FORCE", INT2FIX(1));
#else
    rb_define_const(bdb_mDb, "FORCE", INT2FIX(DB_FORCE));
#endif
#if DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 6
    rb_define_const(bdb_mDb, "GET_BOTH", INT2FIX(9));
#else
    rb_define_const(bdb_mDb, "GET_BOTH", INT2FIX(DB_GET_BOTH));
#endif
    rb_define_const(bdb_mDb, "GET_RECNO", INT2FIX(DB_GET_RECNO));
#ifdef DB_INIT_CDB
    rb_define_const(bdb_mDb, "INIT_CDB", INT2FIX(DB_INIT_CDB));
#endif
    rb_define_const(bdb_mDb, "INIT_LOCK", INT2FIX(DB_INIT_LOCK));
    rb_define_const(bdb_mDb, "INIT_LOG", INT2FIX(DB_INIT_LOG));
    rb_define_const(bdb_mDb, "INIT_MPOOL", INT2FIX(DB_INIT_MPOOL));
    rb_define_const(bdb_mDb, "INIT_TXN", INT2FIX(DB_INIT_TXN));
    rb_define_const(bdb_mDb, "INIT_TRANSACTION", INT2FIX(BDB_INIT_TRANSACTION));
#ifdef DB_JOIN_ITEM
    rb_define_const(bdb_mDb, "JOIN_ITEM", INT2FIX(DB_JOIN_ITEM));
#endif
    rb_define_const(bdb_mDb, "KEYFIRST", INT2FIX(DB_KEYFIRST));
    rb_define_const(bdb_mDb, "KEYLAST", INT2FIX(DB_KEYLAST));
    rb_define_const(bdb_mDb, "LAST", INT2FIX(DB_LAST));
    rb_define_const(bdb_mDb, "LOCK_CONFLICT", INT2FIX(DB_LOCK_CONFLICT));
    rb_define_const(bdb_mDb, "LOCK_DEADLOCK", INT2FIX(DB_LOCK_DEADLOCK));
    rb_define_const(bdb_mDb, "LOCK_DEFAULT", INT2FIX(DB_LOCK_DEFAULT));
    rb_define_const(bdb_mDb, "LOCK_GET", INT2FIX(DB_LOCK_GET));
    rb_define_const(bdb_mDb, "LOCK_NOTGRANTED", INT2FIX(DB_LOCK_NOTGRANTED));
    rb_define_const(bdb_mDb, "LOCK_NOWAIT", INT2FIX(DB_LOCK_NOWAIT));
    rb_define_const(bdb_mDb, "LOCK_OLDEST", INT2FIX(DB_LOCK_OLDEST));
    rb_define_const(bdb_mDb, "LOCK_PUT", INT2FIX(DB_LOCK_PUT));
    rb_define_const(bdb_mDb, "LOCK_PUT_ALL", INT2FIX(DB_LOCK_PUT_ALL));
    rb_define_const(bdb_mDb, "LOCK_PUT_OBJ", INT2FIX(DB_LOCK_PUT_OBJ));
    rb_define_const(bdb_mDb, "LOCK_RANDOM", INT2FIX(DB_LOCK_RANDOM));
    rb_define_const(bdb_mDb, "LOCK_YOUNGEST", INT2FIX(DB_LOCK_YOUNGEST));
    rb_define_const(bdb_mDb, "LOCK_NG", INT2FIX(DB_LOCK_NG));
    rb_define_const(bdb_mDb, "LOCK_READ", INT2FIX(DB_LOCK_READ));
    rb_define_const(bdb_mDb, "LOCK_WRITE", INT2FIX(DB_LOCK_WRITE));
    rb_define_const(bdb_mDb, "LOCK_IWRITE", INT2FIX(DB_LOCK_IWRITE));
    rb_define_const(bdb_mDb, "LOCK_IREAD", INT2FIX(DB_LOCK_IREAD));
    rb_define_const(bdb_mDb, "LOCK_IWR", INT2FIX(DB_LOCK_IWR));
    rb_define_const(bdb_mDb, "MPOOL_CREATE", INT2FIX(DB_MPOOL_CREATE));
    rb_define_const(bdb_mDb, "MPOOL_LAST", INT2FIX(DB_MPOOL_LAST));
    rb_define_const(bdb_mDb, "MPOOL_NEW", INT2FIX(DB_MPOOL_NEW));
    rb_define_const(bdb_mDb, "NEXT", INT2FIX(DB_NEXT));
#if DB_NEXT_DUP
    rb_define_const(bdb_mDb, "NEXT_DUP", INT2FIX(DB_NEXT_DUP));
#endif
    rb_define_const(bdb_mDb, "NOMMAP", INT2FIX(DB_NOMMAP));
    rb_define_const(bdb_mDb, "NOOVERWRITE", INT2FIX(DB_NOOVERWRITE));
    rb_define_const(bdb_mDb, "NOSYNC", INT2FIX(DB_NOSYNC));
#if DB_VERSION_MAJOR < 3
    rb_define_const(bdb_mDb, "POSITION", INT2FIX(0));
#else
    rb_define_const(bdb_mDb, "POSITION", INT2FIX(DB_POSITION));
#endif
    rb_define_const(bdb_mDb, "PREV", INT2FIX(DB_PREV));
#if DB_VERSION_MAJOR < 3
    rb_define_const(bdb_mDb, "PRIVATE", INT2FIX(0));
#else
    rb_define_const(bdb_mDb, "PRIVATE", INT2FIX(DB_PRIVATE));
#endif
    rb_define_const(bdb_mDb, "RDONLY", INT2FIX(DB_RDONLY));
    rb_define_const(bdb_mDb, "RECNUM", INT2FIX(DB_RECNUM));
    rb_define_const(bdb_mDb, "RECORDCOUNT", INT2FIX(DB_RECORDCOUNT));
    rb_define_const(bdb_mDb, "RECOVER", INT2FIX(DB_RECOVER));
    rb_define_const(bdb_mDb, "RECOVER_FATAL", INT2FIX(DB_RECOVER_FATAL));
    rb_define_const(bdb_mDb, "RENUMBER", INT2FIX(DB_RENUMBER));
    rb_define_const(bdb_mDb, "RMW", INT2FIX(DB_RMW));
    rb_define_const(bdb_mDb, "SET", INT2FIX(DB_SET));
    rb_define_const(bdb_mDb, "SET_RANGE", INT2FIX(DB_SET_RANGE));
    rb_define_const(bdb_mDb, "SET_RECNO", INT2FIX(DB_SET_RECNO));
    rb_define_const(bdb_mDb, "SNAPSHOT", INT2FIX(DB_SNAPSHOT));
#if DB_VERSION_MAJOR < 3
    rb_define_const(bdb_mDb, "SYSTEM_MEM", INT2FIX(0));
#else
    rb_define_const(bdb_mDb, "SYSTEM_MEM", INT2FIX(DB_SYSTEM_MEM));
#endif
    rb_define_const(bdb_mDb, "THREAD", INT2FIX(DB_THREAD));
    rb_define_const(bdb_mDb, "TRUNCATE", INT2FIX(DB_TRUNCATE));
    rb_define_const(bdb_mDb, "TXN_NOSYNC", INT2FIX(DB_TXN_NOSYNC));
    rb_define_const(bdb_mDb, "USE_ENVIRON", INT2FIX(DB_USE_ENVIRON));
    rb_define_const(bdb_mDb, "USE_ENVIRON_ROOT", INT2FIX(DB_USE_ENVIRON_ROOT));
#if DB_VERSION_MAJOR < 3
    rb_define_const(bdb_mDb, "TXN_NOWAIT", INT2FIX(0));
    rb_define_const(bdb_mDb, "TXN_SYNC", INT2FIX(0));
    rb_define_const(bdb_mDb, "WRITECURSOR", INT2FIX(0));
#else
    rb_define_const(bdb_mDb, "TXN_NOWAIT", INT2FIX(DB_TXN_NOWAIT));
    rb_define_const(bdb_mDb, "TXN_SYNC", INT2FIX(DB_TXN_SYNC));
    rb_define_const(bdb_mDb, "WRITECURSOR", INT2FIX(DB_WRITECURSOR));
#endif
    rb_define_const(bdb_mDb, "TXN_COMMIT", INT2FIX(BDB_TXN_COMMIT));

    Init_env();
    Init_common();
    Init_recnum();
    Init_transaction();
    Init_cursor();
    Init_lock();
    Init_log();
    Init_delegator();

    bdb_errstr = rb_tainted_str_new(0, 0);
    rb_global_variable(&bdb_errstr);
}
