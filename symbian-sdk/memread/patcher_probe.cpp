/* patcher_probe.cpp v5 — Based on test.cpp (PROVEN WORKING template)
 * Just adds LDD loading and channel open to the working test.
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
            file.Write(_L8("v5\r\n"));

            // Load LDDs
            TInt r;
            r = User::LoadLogicalDevice(_L("patcherShadow.ldd"));
            b.Format(_L8("lS=%d\r\n"), r); file.Write(b);

            r = User::LoadLogicalDevice(_L("patcherS3.ldd"));
            b.Format(_L8("l3=%d\r\n"), r); file.Write(b);

            r = User::LoadLogicalDevice(_L("patcher.ldd"));
            b.Format(_L8("lB=%d\r\n"), r); file.Write(b);

            // Try channel names
            RPatcher ch;
            r = ch.Open(_L("PatcherShadow"));
            b.Format(_L8("o0=%d\r\n"), r); file.Write(b);
            if (r == KErrNone) { ch.Close(); }

            r = ch.Open(_L("patcherShadow"));
            b.Format(_L8("o1=%d\r\n"), r); file.Write(b);
            if (r == KErrNone) { ch.Close(); }

            r = ch.Open(_L("Patcher"));
            b.Format(_L8("o2=%d\r\n"), r); file.Write(b);
            if (r == KErrNone) { ch.Close(); }

            r = ch.Open(_L("patcher"));
            b.Format(_L8("o3=%d\r\n"), r); file.Write(b);
            if (r == KErrNone) { ch.Close(); }

            r = ch.Open(_L("PatcherS3"));
            b.Format(_L8("o4=%d\r\n"), r); file.Write(b);
            if (r == KErrNone) { ch.Close(); }

            r = ch.Open(_L("patcherS3"));
            b.Format(_L8("o5=%d\r\n"), r); file.Write(b);
            if (r == KErrNone) { ch.Close(); }

            file.Write(_L8("done\r\n"));
            file.Close();
            }
        fs.Close();
        }

    delete cleanup;
    return 0;
    }
