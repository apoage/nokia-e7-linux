import e32
import appuifw
from appuifw import *
from graphics import *
import urllib
import pyextaccel
from key_codes import *
import sysinfo

e32.ao_yield()

class Loep:
    map = u'E:\\tmp\\map.jpg'
    title = u'Loep'
    mainUIText = u'Welcome to Loep.'
    helpText = u'Loop is self explanatory.'
    aboutText = u'Loep was created by Berco Beute (cyberco<at>media2b.net)'
    mainColor = (143, 63, 150) 
    targetLocX = 0.
    targetLocY = 0.
    accelX = 0
    accelY = 0
    accelZ = 0
    pixelStep = 3
    threshold = 30
    queueLength = 10
    accelQueueX = [0 for x in range(queueLength)]
    accelQueueY = [0 for x in range(queueLength)]
    accelQueueZ = [0 for x in range(queueLength)]
    
    def __init__(self):
        self.lock = e32.Ao_lock()
        self.exit_flag = False
        appuifw.app.exit_key_handler = self.abort      
        appuifw.app.screen='large' # normal, large or full
        self.imgWidth, self.imgHeight = Image.inspect(self.map)['size']
        self.sourceLocX = self.imgWidth / 2
        self.sourceLocY = self.imgHeight / 2
        self.img = Image.open(self.map)        
        self.keyboard=Keyboard()
        self.canvas=appuifw.Canvas(event_callback=self.keyboard.handle_event, redraw_callback=self.onRedraw)                
        appuifw.app.body = self.canvas

        self.mainMenu = [    ( u"Map", ((u"Start", self.startMap), 
                                             (u"Stop", self.stopMap))), 
                             ( u"Accelerometer", ((u"Start", self.startAccelerometer), 
                                                  (u"Pixelstep", self.setPixelStep), 
                                                  (u"Threshold", self.setThreshold), 
                                                  (u"Stop", self.stopAccelerometer))),   
                             ( u"Help", self.showHelp)
                         ]             
        appuifw.app.menu = self.mainMenu   
        self.loop()        
    
    #___BOILERPLATE PYTHON _____________________________________________________________________________________
    
    def loop( self ):
        try:
            while not self.exit_flag:
                self.lock.wait()
        finally:
            pass

    def abort( self ):
        try:
            self.stopAccelerometer()
        except:
            print 'error stopping Accelerometer'
        self.mapRunning = 0
        appuifw.app.menu = []
        appuifw.app.body = None
        appuifw.app.exit_key_handler = None
        self.img = None
        self.exit_flag = True
        self.lock.signal()

    def set_exit_if_standalone():
        '''Only set_exit() if this wasn't invoked from the interpreter ui.'''
        appname = appuifw.app.full_name()
        if appname[-10:] != u"Python.app":
            appuifw.app.set_exit()
        
    #___CUSTOM ________________________________________________________________________________________________

    def startAccelerometer(self):
        pyextaccel.register(self.onAccelerometerEvent)        
        
    def stopAccelerometer(self):
        pyextaccel.unregister(self.onAccelerometerEvent)        
        
    def onAccelerometerEvent(self, x,y,z):
        #print "x:%i y:%i z:%i" %(x,y,z)
        if -self.threshold < x < self.threshold:
            x = 0
        if -self.threshold < y < self.threshold:
            y = 0
        if -self.threshold < z < self.threshold:
            z = 0
        self.accelQueueX.pop(0)        
        self.accelQueueX.append(x)
        self.accelQueueY.pop(0)
        self.accelQueueY.append(y)
        self.accelQueueZ.pop(0)
        self.accelQueueZ.append(z)

    def queueSum(self, queue):
        result = 0
        for x in queue:
            result += x
        return result

    def runMap(self):
        while self.mapRunning:
            self.onRedraw(())
            e32.ao_yield()
            if self.keyboard.is_down(EScancodeLeftArrow):
                self.sourceLocX = self.sourceLocX + 5
            if self.keyboard.is_down(EScancodeRightArrow):
                self.sourceLocX = self.sourceLocX - 5
            if self.keyboard.is_down(EScancodeDownArrow):
                self.sourceLocY = self.sourceLocY + 5
            if self.keyboard.is_down(EScancodeUpArrow):
                self.sourceLocY = self.sourceLocY - 5        

    def onRedraw(self, rect):
        #accelX means turning on the X axis, which results in movement along the Y axis
        accelX = self.queueSum(self.accelQueueX)
        accelY = self.queueSum(self.accelQueueY)
        if accelY > self.queueLength * self.threshold:
            if self.sourceLocX + self.canvas.size[0] < self.imgWidth:
                self.sourceLocX += self.pixelStep 
        elif accelY < -(self.queueLength * self.threshold):
            if self.sourceLocX > self.pixelStep:
                self.sourceLocX -= self.pixelStep
        if accelX > self.queueLength * self.threshold:
            if self.sourceLocY + self.canvas.size[1] < self.imgHeight:
                self.sourceLocY += self.pixelStep 
        elif accelX < -(self.queueLength * self.threshold):
            if self.sourceLocY > self.pixelStep:
                self.sourceLocY -= self.pixelStep
        self.canvas.blit(self.img, source=((self.sourceLocX, self.sourceLocY), 
                                           (self.sourceLocX + self.canvas.size[0], 
                                            self.sourceLocY + self.canvas.size[1])))

    def startMap(self):
        self.mapRunning = 1
        self.startAccelerometer()
        self.runMap()
        
    def stopMap(self):
        self.mapRunning = 0
        self.stopAccelerometer()        
        
    def showHelp(self):
        helpTextComponent = appuifw.Text()
        helpTextComponent.set(self.helpText)
        appuifw.app.body = helpTextComponent

    def setPixelStep(self):
        newStep = appuifw.query( u'Set pixelstep:' , 'number', self.pixelStep)
        if newStep:
            self.pixelStep = newStep    

    def setThreshold(self):
        newThreshold = appuifw.query( u'Set threshold:' , 'number', self.threshold)
        if newThreshold:
            self.threshold = newThreshold
        
class Keyboard(object):
    
    def __init__(self,onevent=lambda:None):
        self._keyboard_state={}
        self._downs={}
        self._onevent=onevent
        
    def handle_event(self,event):
        if event['type'] == appuifw.EEventKeyDown:
            codee = event['scancode']
            if not self.is_down(codee):
                self._downs[codee] = self._downs.get(codee,0)+1
            self._keyboard_state[codee] = 1
        elif event['type'] == appuifw.EEventKeyUp:
            self._keyboard_state[event['scancode']] = 0
        self._onevent()
        
    def is_down(self,scancode):
        return self._keyboard_state.get(scancode,0)
    
    def pressed(self,scancode):
        if self._downs.get(scancode,0):
            self._downs[scancode] -= 1
            return True
        return False

        
# MAIN _________________________________________________________________________________________________________

def main():
    app = Loep()

if __name__ == "__main__":
    main()
