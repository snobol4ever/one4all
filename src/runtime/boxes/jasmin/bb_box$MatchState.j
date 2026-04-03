.class public final bb/bb_box$MatchState
.super java/lang/Object
.inner class public static final matchstate inner bb/bb_box$MatchState outer bb/bb_box

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
    putfield bb/bb_box$MatchState/sigma Ljava/lang/String;
    aload_0
    iconst_0
    putfield bb/bb_box$MatchState/delta I
    aload_0
    aload_1
    invokevirtual java/lang/String/length()I
    putfield bb/bb_box$MatchState/omega I
    return
.end method
