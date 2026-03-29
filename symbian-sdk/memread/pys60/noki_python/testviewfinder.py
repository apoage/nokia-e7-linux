# Copyright (c) 2007 Nokia Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# viewfinder.py - shows viewfinder on the screen
#

import appuifw 
import fastcamera
import e32

class ViewFinder:
    def __init__(self):
        self.script_lock = e32.Ao_lock()
        self.finder_on=0
        self.camtouse = fastcamera.Camera(0)

    def run(self):
        old_title = appuifw.app.title
        self.refresh()
        self.script_lock.wait()
        appuifw.app.title = old_title
        appuifw.app.body = None

    def refresh(self):
        appuifw.app.title = u"Viewfinder"
        appuifw.app.menu = [(u"Start", self.start_finder),
                            (u"Stop", self.stop_finder),
                            (u"Toggle mirror", self.toggle_mirror),
                            (u"Next camera", self.next_camera),
                            (u"Exit", self.do_exit)]
        appuifw.app.exit_key_handler = self.exit_key_handler
        appuifw.app.body=self.canvas=appuifw.Canvas()
        self.camtouse.start(self.camera_started)

    def stop_finder(self):
        fastcamera.stop_finder(self.camtouse)
        self.finder_on=0
        appuifw.note(u"Viewfinder stopped")

    def toggle_mirror(self):
        if (self.finder_on):
            if (self.camtouse.viewfinder_mirror_supported()):
                mirror = 1
                if (self.camtouse.viewfinder_mirror()):
                    mirror = 0
                self.camtouse.set_viewfinder_mirror(mirror)

    def next_camera(self):
        if (self.finder_on):
            fastcamera.stop_finder(self.camtouse)
        index = self.camtouse.camIndex() + 1
        if (index >= fastcamera.cameras_available()):
            index = 0
        self.camtouse.release()
        self.camtouse = fastcamera.Camera(index)
        self.camtouse.start(self.camera_started)
    
    def camera_started(self, error):
        if error == 0:
            fastcamera.start_finder(self.camtouse, self.show_img, size=(160, 120))
            self.finder_on=1
        else:
            appuifw.note(u"Start camera failed")

    def start_finder(self):
        if (not self.finder_on):
            fastcamera.start_finder(self.camtouse, self.show_img, size=(160, 120))
            self.finder_on=1
        else:
            appuifw.note(u"Viewfinder already on")

    def show_img(self, error, image_frame):
        if error == 0:
            self.canvas.blit(image_frame)

    def do_exit(self):
        self.exit_key_handler()

    def exit_key_handler(self):
        fastcamera.stop_finder(self.camtouse)
        self.canvas=None
        appuifw.app.exit_key_handler = None
        self.script_lock.signal()

if __name__ == '__main__':
    ViewFinder().run()
