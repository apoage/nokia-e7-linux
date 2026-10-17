/* peek.cpp — native child memory reader for the bridge agent.
 *
 * Usage (from PyS60): e32.start_exe('peek.exe', 'ADDR [N]', 1)
 *   ADDR = hex address (user VA), N = byte count (default 1, max 4096)
 * Writes "addr n: hh hh hh ..." to E:\peek_out.txt on success.
 * If ADDR is unmapped the data abort kills ONLY this child process;
 * the agent survives and sees a nonzero exit code.
 *
 * Build: build_variants.sh recipe (uid 0xA0000097, 4 platform deps).
 */
#include <e32base.h>
#include <e32std.h>
#include <f32file.h>

GLDEF_C TInt E32Main()
    {
    CTrapCleanup* cleanup = CTrapCleanup::New();

    TBuf<64> cmd;
    User::CommandLine(cmd);
    TBuf8<64> cmd8;
    cmd8.Copy(cmd);

    TLex8 lex(cmd8);
    TUint addr = 0;
    TInt n = 1;
    TInt ok = KErrNone;
    ok = lex.Val(addr, EHex);
    if (ok == KErrNone)
        {
        lex.SkipSpace();
        if (lex.Remainder().Length() > 0)
            lex.Val(n);
        }

    RFs fs;
    TInt result = ok;
    if (result == KErrNone && fs.Connect() == KErrNone)
        {
        RFile file;
        if (file.Replace(fs, _L("E:\\peek_out.txt"), EFileWrite) == KErrNone)
            {
            if (n < 1)
                n = 1;
            if (n > 4096)
                n = 4096;
            TBuf8<32> b;
            b.Format(_L8("%08x %d:"), addr, n);
            file.Write(b);
            for (TInt i = 0; i < n; i++)
                {
                volatile TUint8* p = (volatile TUint8*)(addr + i);
                TUint8 v = *p;  /* unmapped => abort => child dies only */
                b.Format(_L8(" %02x"), v);
                file.Write(b);
                }
            file.Write(_L8("\r\n"));
            file.Close();
            }
        else
            {
            result = KErrPathNotFound;
            }
        fs.Close();
        }

    delete cleanup;
    return result;
    }
