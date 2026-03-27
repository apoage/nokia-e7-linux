/* test.cpp — Absolute minimum: write a file */
#include <e32base.h>
#include <e32std.h>
#include <f32file.h>

GLDEF_C TInt E32Main()
    {
    CTrapCleanup* cleanup = CTrapCleanup::New();

    RFs fs;
    if (fs.Connect() == KErrNone)
        {
        RFile file;
        if (file.Replace(fs, _L("E:\\test_ok.txt"), EFileWrite) == KErrNone)
            {
            file.Write(_L8("it works\n"));
            file.Close();
            }
        fs.Close();
        }

    delete cleanup;
    return 0;
    }
