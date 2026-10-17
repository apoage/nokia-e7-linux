/* test_variant.cpp — minimal exec test, marker via -DMARKER
 *
 * Writes E:\test_ok_<MARKER>.txt so a successful run is provable from
 * mass storage without any console. Build via build_variants.sh (docker).
 */
#include <e32base.h>
#include <e32std.h>
#include <f32file.h>

GLDEF_C TInt E32Main()
    {
    CTrapCleanup* cleanup = CTrapCleanup::New();

    RFs fs;
    if (fs.Connect() == KErrNone)
        {
        _LIT8(KMsg, "variant " MARKER " ok\n");
        _LIT(KFile, "E:\\test_ok_" MARKER ".txt");
        RFile file;
        if (file.Replace(fs, KFile, EFileWrite) == KErrNone)
            {
            file.Write(KMsg);
            file.Close();
            }
        fs.Close();
        }

    delete cleanup;
    return 0;
    }
