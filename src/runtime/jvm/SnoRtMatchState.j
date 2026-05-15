; SnoRtMatchState.j — MatchState class for SCRIP JVM pattern matching
; AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6
.class public rt/SnoRt$MatchState
.super java/lang/Object
.field public sigma Ljava/lang/String;
.field public delta I
.field public omega I
.method public <init>(Ljava/lang/String;)V
    .limit stack 3
    .limit locals 2
    aload_0
    invokespecial java/lang/Object/<init>()V
    aload_0
    aload_1
    putfield rt/SnoRt$MatchState/sigma Ljava/lang/String;
    aload_0
    iconst_0
    putfield rt/SnoRt$MatchState/delta I
    aload_0
    aload_1
    invokevirtual java/lang/String/length()I
    putfield rt/SnoRt$MatchState/omega I
    return
.end method
