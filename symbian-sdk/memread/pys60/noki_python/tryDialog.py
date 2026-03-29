import e32, appuifw
from dialog import Wait, Progress

appuifw.app.title= u'Hello dialog'

def cancel_cb():
    print "cancel :D"

# Wait(label=u'', [True/False, cancel_cb])
dlg = Wait(u"Hello Wait note :)", True, cancel_cb )
dlg.show()
print "Show"
e32.ao_sleep(2)
dlg.show()
dlg.set_label(u'Wait! Don\'t go :\'(')
print "Change label"
e32.ao_sleep(1)
dlg.close()
print "Wait, that's not finished!"
e32.ao_sleep(2)
dlg.show(u'Me again ;)')
e32.ao_sleep(2)
dlg.close()
e32.ao_sleep(2)
dlg.show()
print "Show again with the last label"
e32.ao_sleep(2)
dlg.close()

e32.ao_sleep(2)


# Progress(label=u'', bars=5, [time=5, global=[True/False]])
pgrs = Progress(u"Hello Progress :)\n It's LFD", 15, 5)
try:
    pgrs.show()
    print "Show"
    e32.ao_sleep(1)
    pgrs.show()
    print "Try to show again: if already active, doesn't do anything"

    print "Bar num: ", pgrs.bars()
    e32.ao_sleep(1)

    pgrs.step()
    print "1 step"

    e32.ao_sleep(1)

    pgrs.step(3)
    print "3 steps at once"

    pgrs.goto(2)
    e32.ao_sleep(1)
    print "goto bar 2"

    pgrs.set_label(u"Don't go Progress :(")
    print "Set different label"
    e32.ao_sleep(1)
    pgrs.close()
    print "Close and destroy"
except:
    pgrs.close()

print "finished"
