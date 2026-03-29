try:
    import bt_teror
except:
    import sys
    import traceback
    import e32
    import appuifw

    lock = e32.Ao_lock()
    appuifw.app.title = u'User Error'
    appuifw.app.screen = 'normal'
    appuifw.app.focus = None
    body = appuifw.Text()
    appuifw.app.body = body
    body.set(unicode(''.join(traceback.format_exception(*sys.exc_info()))))
    lock.wait()