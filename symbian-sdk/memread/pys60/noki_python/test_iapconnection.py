import e32
import urllib

import iapconnect

# return value 0 means that user canceled
default_iap = iapconnect.iap_selection_dialog()
print default_iap

conn = iapconnect.Connection()
conn.connect(u"www.google.com", 80, default_iap) # MUST BE UNICODE!!!!
print "connected.."
conn.send("GET / HTTP/1.0\n\n")
print "sent request"

buf = []
while 1:
    data = conn.recv(1024)
    print data
    if not data:
        break
    buf.append(data)

buf = ''.join(buf)

appuifw.note(unicode(buf[:100]), 'info')

#conn.close() # this will disconnect, but leave gprs active
conn.stop() # this will shutdown the gprs connection
