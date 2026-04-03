.class public bb/bb_abort$AbortException
.super java/lang/RuntimeException
.inner class public static final abort_exception inner bb/bb_abort$AbortException outer bb/bb_abort

.method public <init>()V
    .limit stack 3
    .limit locals 1
    aload_0
    aconst_null     ; message = null
    aconst_null     ; cause = null
    iconst_1        ; enableSuppression = true
    iconst_0        ; writableStackTrace = false
    invokespecial java/lang/RuntimeException/<init>(Ljava/lang/String;Ljava/lang/Throwable;ZZ)V
    return
.end method
