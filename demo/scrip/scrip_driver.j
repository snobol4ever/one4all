; scrip_driver.j — entry point for the family demo.
; Runs SNOBOL4 CSV parser first (populates Prolog DB), then Icon query+report.
.class public scrip_driver
.super java/lang/Object

.method public static main([Ljava/lang/String;)V
    .limit stack 2
    .limit locals 1
    aconst_null
    invokestatic family_snobol4/main([Ljava/lang/String;)V
    aconst_null
    invokestatic family_icon/main([Ljava/lang/String;)V
    return
.end method
