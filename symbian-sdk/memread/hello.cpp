/*
 * hello.cpp — Minimal Symbian test app.
 * Writes error codes to multiple locations + shows InfoPrint.
 */

#include <e32base.h>
#include <e32std.h>
#include <f32file.h>

GLDEF_C TInt E32Main()
    {
    CTrapCleanup* cleanup = CTrapCleanup::New();

    /* Show on-screen notification so we know the exe runs */
    User::InfoPrint(_L("hello starting"));

    RFs fs;
    TInt r = fs.Connect();

    if (r != KErrNone)
        {
        /* Can't even connect to file server - show error */
        TBuf<64> msg;
        msg.Format(_L("RFs err %d"), r);
        User::InfoPrint(msg);
        delete cleanup;
        return r;
        }

    /* Try writing to every plausible location, log each result */
    TBuf8<512> errlog;
    TInt written = 0;

    RFile file;
    TInt wr;

    wr = file.Replace(fs, _L("E:\\hello.txt"), EFileWrite);
    errlog.AppendFormat(_L8("E:hello=%d\n"), wr);
    if (wr == KErrNone) { file.Write(_L8("hello from docker\n")); file.Close(); written++; }

    wr = file.Replace(fs, _L("C:\\Data\\hello.txt"), EFileWrite);
    errlog.AppendFormat(_L8("C:Data=%d\n"), wr);
    if (wr == KErrNone) { file.Write(_L8("hello from docker\n")); file.Close(); written++; }

    wr = file.Replace(fs, _L("D:\\hello.txt"), EFileWrite);
    errlog.AppendFormat(_L8("D:=%d\n"), wr);
    if (wr == KErrNone) { file.Write(_L8("hello from docker\n")); file.Close(); written++; }

    wr = file.Replace(fs, _L("C:\\hello.txt"), EFileWrite);
    errlog.AppendFormat(_L8("C:=%d\n"), wr);
    if (wr == KErrNone) { file.Write(_L8("hello from docker\n")); file.Close(); written++; }

    /* Write error log */
    RFile logf;
    TInt lr = logf.Replace(fs, _L("E:\\hello_err.txt"), EFileWrite);
    if (lr != KErrNone)
        lr = logf.Replace(fs, _L("C:\\Data\\hello_err.txt"), EFileWrite);
    if (lr == KErrNone)
        {
        errlog.AppendFormat(_L8("written=%d fs=%d\n"), written, r);
        logf.Write(errlog);
        logf.Close();
        }

    fs.Close();

    TBuf<64> donemsg;
    donemsg.Format(_L("hello done w=%d"), written);
    User::InfoPrint(donemsg);

    delete cleanup;
    return KErrNone;
    }
