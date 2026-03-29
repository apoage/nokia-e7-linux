import e32
import time
import imagedecoder
import thread
import graphics
import struct

def notifyUpdate(args):
    global lock
    print "notifyUpdate", args
    if isinstance(args, int):
        print "error", args
    else:
        img = graphics.Image.from_cfbsbitmap(args)
        img.save("c:\\test.jpg")
        del img
    lock.signal();

def notifyLoad(args):
    global lock
    global bitmap
    print "notifyLoad", args
    if isinstance(args, int):
        print "error", args
    else:
        bitmap = args
    lock.signal();

def notifySave(args):
    global lock
    if isinstance(args, int):
        print "error", args
    else:
        open('c:\\convertout.jpg','w').write(args);
    lock.signal();

lock = e32.Ao_lock();

#imagefile = 'c:\\imageExif.jpg'
#s = open(imagefile,'r').read()                    

# must assign to a variable (converter here) otherwise the converter is immediately garbage collected!
#converter = imagedecoder.ConvertImage(s, notifyUpdate);
#print "Decoder called"

#print "Starting wait"
#lock.wait()
#print "Decoder done"

print "Calling decoder"
converter = imagedecoder.ConvertFileImageMime(u'c:\\imageExif.jpg', "image/jpeg", notifyLoad, imagedecoder.EGray16, 16, 16);
print "Decoder called"

print "Starting wait"
lock.wait()
print "Decoder done"

print "Calling encoder"
print bitmap
converter = imagedecoder.ConvertBitmap(bitmap, "image/jpeg", notifySave, 75, 'default', 24);
print "Encoder called"

print "Starting wait"
lock.wait()
print "Encoder done"

del converter

#imagefile = 'c:\\imageExif.jpg'
#s = open(imagefile,'r').read()                    
#exif = imagedecoder.CreateExifModify(s);
#s1 = struct.pack("iiiiii", 42, 1, 40, 1, 171, 10)
#exif.SetTag(3,2,5,3, s1);
#open('c:\\exiftest.jpg','w').write(exif.WriteData(s));
#print "Exif done"
