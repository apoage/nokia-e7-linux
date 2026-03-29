# Import the extension
import e32
import pyextaccel


# Define some service function - must take 3 parameters (16 bit int's)

def printout(x,y,z):
  print "x: %i y: %i z: %i"%(x,y,z)


# Data acquisition callbacks will start as soon as a valid Python function is registered

pyextaccel.register(printout)


#
#
#... Data acquisition callbacks will commence and ended after 15 secs
#
#

e32.ao_sleep(15)

# Unregister to stop data acquisition callbacks.

pyextaccel.unregister()
