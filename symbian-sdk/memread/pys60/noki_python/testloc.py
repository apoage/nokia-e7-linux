import time
import sys
import e32
import appuifw
import thread
import locationrequestor

stopping = False;

def notifyUpdate(args):
    print args;
    if (stopping):
        lock.signal();

def threadFunc():
    global lock
    lock = e32.Ao_lock();

    # create LocationRequestor instance
    lr = locationrequestor.LocationRequestor();
    # show default module
    print 'Default', lr.GetDefaultModuleId();
    # show number of modules
    count = lr.GetNumModules();
    print 'Count', count;
    # find internal GPS (if any)
    i = 0;
    id = -1;
    while i < count:
        info = lr.GetModuleInfoByIndex(i);
        if ((info[3] == locationrequestor.EDeviceInternal) and ((info[2] & locationrequestor.ETechnologyNetwork) == 0)):
            id = info[0]
            print 'Using', info;

        i = i + 1;

    # find external GPS (if any), if internal not found
    if id < 0:
        i = 0;
        while i < count:
            info = lr.GetModuleInfoByIndex(i);
            if ((info[3] == locationrequestor.EDeviceExternal) and ((info[2] & locationrequestor.ETechnologyNetwork) == 0)):
                id = info[0]
                print 'Using', info;

            i = i + 1;

    # set update options
    lr.SetUpdateOptions(3, 25, 2, 1);
    # connect to position module
    lr.Open(id);
    # install callback
    try:
        lr.InstallPositionCallback(notifyUpdate);
        print 'Done'
    except Exception, reason:
        print reason

    lock.wait();
    lr.Close();

thread.start_new_thread(threadFunc,())
time.sleep(10);
stopping = True;

