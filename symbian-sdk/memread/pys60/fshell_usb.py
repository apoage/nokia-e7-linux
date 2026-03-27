# fshell_usb.py - Launch FShell with USB VT100 console
# After running this, connect with: screen /dev/ttyACM0 115200
# or: minicom -D /dev/ttyACM0
import e32

# Try different console options for USB serial
consoles = [
    '--console vt100usbcons',
    '--console vt100usbcons.dll',
    '--console vt100busdevcons',
    '--console vt100cons --console-title port=ACM::0',
    '--console vt100cons --console-title port=ACM::1',
]

for con in consoles:
    print "trying: fshell %s" % con
    try:
        # Don't wait - fshell will block waiting for USB connection
        e32.start_exe('fshell.exe', con)
        print "launched (no wait)"
        break
    except Exception, ex:
        print "error: %s" % str(ex)

print "fshell launched - connect from PC with:"
print "  screen /dev/ttyACM0 115200"
print "  or: minicom -D /dev/ttyACM0"
