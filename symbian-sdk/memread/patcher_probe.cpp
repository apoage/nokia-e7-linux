/* patcher_probe.cpp v6 — direct LDD channel opens only.
 *
 * v5 called User::LoadLogicalDevice() from a zero-cap process and the
 * phone rebooted (empty E:\pp.txt = unflushed writes lost). Loading kernel
 * drivers is panic territory; opening a channel on an ALREADY-LOADED
 * driver is not. RomPatcher+ loads its LDDs at boot, so we just try the
 * channel names. Every write is flushed so results survive crashes.
 */
#include <e32base.h>
#include <e32std.h>
#include <f32file.h>

class RPatcher : public RBusLogicalChannel
    {
public:
    TInt Open(const TDesC& aName)
        { return DoCreate(aName, TVersion(0,0,0), KNullUnit, NULL, NULL); }
    TInt Ctrl(TInt aOp, TAny* a1=NULL, TAny* a2=NULL)
        { return DoControl(aOp, a1, a2); }
    };

GLDEF_C TInt E32Main()
    {
    CTrapCleanup* cleanup = CTrapCleanup::New();

    RFs fs;
    if (fs.Connect() == KErrNone)
        {
        RFile file;
        if (file.Replace(fs, _L("E:\\pp.txt"), EFileWrite) == KErrNone)
            {
            TBuf8<128> b;
            file.Write(_L8("v6\r\n"));
            file.Flush();

            // Channel names only — no LoadLogicalDevice (v5 rebooted phone)
            RPatcher ch;
            TInt r;
            r = ch.Open(_L("PatcherShadow"));
            b.Format(_L8("o0=%d\r\n"), r); file.Write(b); file.Flush();
            if (r == KErrNone) { ch.Close(); }

            r = ch.Open(_L("patcherShadow"));
            b.Format(_L8("o1=%d\r\n"), r); file.Write(b); file.Flush();
            if (r == KErrNone) { ch.Close(); }

            r = ch.Open(_L("Patcher"));
            b.Format(_L8("o2=%d\r\n"), r); file.Write(b); file.Flush();
            if (r == KErrNone) { ch.Close(); }

            r = ch.Open(_L("patcher"));
            b.Format(_L8("o3=%d\r\n"), r); file.Write(b); file.Flush();
            if (r == KErrNone) { ch.Close(); }

            r = ch.Open(_L("PatcherS3"));
            b.Format(_L8("o4=%d\r\n"), r); file.Write(b); file.Flush();
            if (r == KErrNone) { ch.Close(); }

            r = ch.Open(_L("patcherS3"));
            b.Format(_L8("o5=%d\r\n"), r); file.Write(b); file.Flush();
            if (r == KErrNone) { ch.Close(); }

            file.Write(_L8("done\r\n"));
            file.Flush();
            file.Close();
            }
        fs.Close();
        }

    delete cleanup;
    return 0;
    }
