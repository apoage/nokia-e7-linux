/* memread_simple.cpp — v16: Try ONE DoCreate name per run.
 * Reads name index from E:\name_idx.txt (0-7).
 * Increments index after each attempt so next reboot tries next name.
 */
#include <e32base.h>
#include <e32std.h>
#include <f32file.h>

class RMemAccess : public RBusLogicalChannel
    {
public:
    TInt OpenByName(const TDesC& aName)
        { return DoCreate(aName, TVersion(0,0,0), KNullUnit, NULL, NULL); }
    TInt Try(TInt aOp, TAny* a1, TAny* a2)
        { return DoControl(aOp, a1, a2); }
    };

static void AppendLog(RFs& aFs, const TDesC8& aLine)
    {
    RFile f;
    TInt r = f.Open(aFs, _L("E:\\memread_log.txt"), EFileWrite);
    if (r == KErrNotFound)
        r = f.Create(aFs, _L("E:\\memread_log.txt"), EFileWrite);
    if (r == KErrNone)
        { TInt p=0; f.Seek(ESeekEnd,p); f.Write(aLine); f.Close(); }
    r = f.Open(aFs, _L("C:\\Data\\memread_log.txt"), EFileWrite);
    if (r == KErrNotFound)
        r = f.Create(aFs, _L("C:\\Data\\memread_log.txt"), EFileWrite);
    if (r == KErrNone)
        { TInt p=0; f.Seek(ESeekEnd,p); f.Write(aLine); f.Close(); }
    }

static TInt ReadIdx(RFs& aFs)
    {
    RFile f;
    if (f.Open(aFs, _L("E:\\name_idx.txt"), EFileRead) != KErrNone)
        return 0;
    TBuf8<8> b;
    f.Read(b);
    f.Close();
    TInt v = 0;
    for (TInt i = 0; i < b.Length(); i++)
        if (b[i] >= '0' && b[i] <= '9')
            v = v*10 + (b[i] - '0');
    return v;
    }

static void WriteIdx(RFs& aFs, TInt aIdx)
    {
    RFile f;
    if (f.Replace(aFs, _L("E:\\name_idx.txt"), EFileWrite) == KErrNone)
        { TBuf8<8> b; b.AppendNum(aIdx); f.Write(b); f.Close(); }
    }

GLDEF_C TInt E32Main()
    {
    CTrapCleanup* cleanup = CTrapCleanup::New();
    RFs fs;
    fs.Connect();

    TInt idx = ReadIdx(fs);

    /* Write NEXT index immediately (before potential crash) */
    WriteIdx(fs, idx + 1);

    TBuf8<256> line;
    line.Format(_L8("=== memread v16 idx=%d ===\n"), idx);
    AppendLog(fs, line);

    /* Load LDD */
    _LIT(KLoadName, "memoryAccess-fshell");
    TInt r = User::LoadLogicalDevice(KLoadName);
    line.Format(_L8("LoadLD: %d\n"), r);
    AppendLog(fs, line);

    /* Name table */
    _LIT(KN0, "memoryAccess-fshell");
    _LIT(KN1, "memoryaccess-fshell");
    _LIT(KN2, "memoryAccess");
    _LIT(KN3, "memoryaccess");
    _LIT(KN4, "MemoryAccess");
    _LIT(KN5, "MEMORYACCESS");
    _LIT(KN6, "memoryAccess-fshell.ldd");
    _LIT(KN7, "fshell-memoryaccess");
    const TDesC* names[] = {&KN0,&KN1,&KN2,&KN3,&KN4,&KN5,&KN6,&KN7};

    if (idx >= 8)
        {
        line.Format(_L8("All names exhausted. Reset name_idx.txt to 0.\n"));
        AppendLog(fs, line);
        fs.Close();
        delete cleanup;
        return 0;
        }

    TBuf8<64> n8;
    n8.Copy(names[idx]->Left(60));
    line.Format(_L8("Trying DoCreate(\"%S\")...\n"), &n8);
    AppendLog(fs, line);

    RMemAccess ma;
    r = ma.OpenByName(*names[idx]);
    line.Format(_L8("DoCreate result: %d\n"), r);
    AppendLog(fs, line);

    if (r == KErrNone)
        {
        line.Format(_L8("*** CHANNEL OPEN with \"%S\"! ***\n"), &n8);
        AppendLog(fs, line);

        /* Quick safe opcode scan */
        for (TInt op = 0; op <= 20; op++)
            {
            r = ma.Try(op, NULL, NULL);
            if (r != -1 && r != -5)
                {
                line.Format(_L8("  op[%d]=%d\n"), op, r);
                AppendLog(fs, line);
                }
            }
        ma.Close();
        }

    line.Format(_L8("=== v16 idx=%d complete ===\n"), idx);
    AppendLog(fs, line);

    fs.Close();
    delete cleanup;
    return 0;
    }
