/* memaccess_probe.cpp v1 — kernel memory read via fshell's memoryAccess
 * driver (loaded at boot, no capability check in DoCreate — fshell FCL
 * source, libraries/memoryaccess/).
 *
 * Steps (each flushed to E:\ma.txt so any crash preserves evidence):
 *   open  = DoCreate("memoryAccess-fshell", TVersion(1,0,0))
 *   self  = GetThreadMem own-thread @ &magic (expect 0xDEADBEEF)
 *   gpio1 = GetThreadMem @ 0xC8004000 (GPFSEL0-6 = first 28 bytes)
 *   gpio2 = GetThreadMem @ 0xC8004150 (GPIO KVA from ROM constant pool)
 */
#include <e32base.h>
#include <e32std.h>
#include <f32file.h>

_LIT(KMemAccessName, "memoryAccess-fshell");

class TThreadMemoryAccessParams
    {
public:
    TUint iId;
    const TUint8* iAddr;
    TInt iSize;
    };
typedef TPckgBuf<TThreadMemoryAccessParams> TThreadMemoryAccessParamsBuf;

class RMemAccess : public RBusLogicalChannel
    {
public:
    TInt Open()
        { return DoCreate(KMemAccessName, TVersion(1,0,0), KNullUnit, NULL, NULL); }
    TInt GetThreadMem(TThreadMemoryAccessParamsBuf& aP, TDes8& aBuf)
        { return DoControl(0 /* EControlGetThreadMem */, (TAny*)&aP, (TAny*)&aBuf); }
    };

static RFile gFile;

static void Log(const TDesC8& aText)
    {
    gFile.Write(aText);
    gFile.Flush();
    }

static void LogHex(const TDesC8& aPrefix, const TDesC8& aData)
    {
    TBuf8<32> b;
    Log(aPrefix);
    for (TInt i = 0; i < aData.Length(); i++)
        {
        b.Format(_L8("%02x "), aData[i]);
        Log(b);
        }
    Log(_L8("\r\n"));
    }

GLDEF_C TInt E32Main()
    {
    CTrapCleanup* cleanup = CTrapCleanup::New();
    RFs fs;
    if (fs.Connect() != KErrNone)
        {
        delete cleanup;
        return -1;
        }
    if (gFile.Replace(fs, _L("E:\\ma.txt"), EFileWrite) != KErrNone)
        {
        fs.Close();
        delete cleanup;
        return -2;
        }

    TBuf8<64> line;
    RMemAccess ma;
    TInt r = ma.Open();
    line.Format(_L8("open=%d\r\n"), r);
    Log(line);

    if (r == KErrNone)
        {
        TThreadMemoryAccessParamsBuf p;
        TBuf8<256> buf;
        TUint tid = (TUint)(TInt)RThread().Id().Id();
        p().iId = tid;

        // self test: read our own stack variable
        volatile TUint magic = 0xDEADBEEF;
        p().iAddr = (const TUint8*)&magic;
        p().iSize = 16;
        buf.Zero();
        r = ma.GetThreadMem(p, buf);
        line.Format(_L8("self rc=%d len=%d\r\n"), r, buf.Length());
        Log(line);
        if (r == KErrNone && buf.Length() >= 4)
            LogHex(_L8("self data: "), buf.Left(Min(buf.Length(), 16)));

        // GPIO kernel VA window 1: GPFSEL0-6 at 0xC8004000
        if (r == KErrNone)
            {
            p().iAddr = (const TUint8*)0xC8004000;
            p().iSize = 64;
            buf.Zero();
            r = ma.GetThreadMem(p, buf);
            line.Format(_L8("gpio1 rc=%d len=%d\r\n"), r, buf.Length());
            Log(line);
            if (r == KErrNone)
                LogHex(_L8("GPFSEL: "), buf.Left(Min(buf.Length(), 64)));
            }

        // GPIO kernel VA window 2: 0xC8004150 (ROM constant pool value)
        if (r == KErrNone)
            {
            p().iAddr = (const TUint8*)0xC8004150;
            p().iSize = 64;
            buf.Zero();
            r = ma.GetThreadMem(p, buf);
            line.Format(_L8("gpio2 rc=%d len=%d\r\n"), r, buf.Length());
            Log(line);
            if (r == KErrNone)
                LogHex(_L8("gpio2: "), buf.Left(Min(buf.Length(), 64)));
            }

        ma.Close();
        }

    Log(_L8("done\r\n"));
    gFile.Close();
    fs.Close();
    delete cleanup;
    return 0;
    }
