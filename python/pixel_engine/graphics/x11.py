
import ctypes
import ctypes.util
import sys

# Load X11 Library
x11_path = ctypes.util.find_library('X11')
if not x11_path:
    raise ImportError("libX11 not found. This engine requires an X11-based Linux system.")
x11 = ctypes.cdll.LoadLibrary(x11_path)

# Constants
KeyPress = 2
KeyRelease = 3
ButtonPress = 4
ButtonRelease = 5
MotionNotify = 6
Expose = 12
ClientMessage = 33

KeyPressMask = (1 << 0)
KeyReleaseMask = (1 << 1)
ButtonPressMask = (1 << 2)
ButtonReleaseMask = (1 << 3)
PointerMotionMask = (1 << 6)
ExposureMask = (1 << 15)
StructureNotifyMask = (1 << 17)

ZPixmap = 2

# Types (Opaque handles for safety)
Display = ctypes.c_void_p
Window = ctypes.c_ulong
GC = ctypes.c_void_p
Visual = ctypes.c_void_p
Drawable = ctypes.c_ulong
Screen = ctypes.c_void_p
XImagePtr = ctypes.c_void_p # Treat XImage* as opaque

# Defines
ZPixmap = 2

class XKeyEvent(ctypes.Structure):
    _fields_ = [
        ('type', ctypes.c_int),
        ('serial', ctypes.c_ulong),
        ('send_event', ctypes.c_int),
        ('display', Display),
        ('window', Window),
        ('root', Window),
        ('subwindow', Window),
        ('time', ctypes.c_ulong),
        ('x', ctypes.c_int),
        ('y', ctypes.c_int),
        ('x_root', ctypes.c_int),
        ('y_root', ctypes.c_int),
        ('state', ctypes.c_uint),
        ('keycode', ctypes.c_uint),
        ('same_screen', ctypes.c_int),
    ]

class XButtonEvent(ctypes.Structure):
    _fields_ = [
        ('type', ctypes.c_int),
        ('serial', ctypes.c_ulong),
        ('send_event', ctypes.c_int),
        ('display', Display),
        ('window', Window),
        ('root', Window),
        ('subwindow', Window),
        ('time', ctypes.c_ulong),
        ('x', ctypes.c_int),
        ('y', ctypes.c_int),
        ('x_root', ctypes.c_int),
        ('y_root', ctypes.c_int),
        ('state', ctypes.c_uint),
        ('button', ctypes.c_uint),
        ('same_screen', ctypes.c_int),
    ]

class XMotionEvent(ctypes.Structure):
    _fields_ = [
        ('type', ctypes.c_int),
        ('serial', ctypes.c_ulong),
        ('send_event', ctypes.c_int),
        ('display', Display),
        ('window', Window),
        ('root', Window),
        ('subwindow', Window),
        ('time', ctypes.c_ulong),
        ('x', ctypes.c_int),
        ('y', ctypes.c_int),
        ('x_root', ctypes.c_int),
        ('y_root', ctypes.c_int),
        ('state', ctypes.c_uint),
        ('is_hint', ctypes.c_byte),
        ('same_screen', ctypes.c_int),
    ]

# Pad to make Union big enough for any event
class XEvent(ctypes.Union):
    _fields_ = [
        ('type', ctypes.c_int),
        ('xkey', XKeyEvent),
        ('xbutton', XButtonEvent),
        ('xmotion', XMotionEvent),
        ('pad', ctypes.c_long * 24)
    ]

# Setup Argtypes
x11.XOpenDisplay.restype = Display
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]

x11.XCreateSimpleWindow.restype = Window
x11.XCreateSimpleWindow.argtypes = [Display, Window, ctypes.c_int, ctypes.c_int, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_ulong]

x11.XMapWindow.argtypes = [Display, Window]
x11.XSelectInput.argtypes = [Display, Window, ctypes.c_long]
x11.XCreateGC.restype = GC
x11.XCreateGC.argtypes = [Display, Drawable, ctypes.c_ulong, ctypes.c_void_p]

x11.XCreateImage.restype = XImagePtr
x11.XCreateImage.argtypes = [Display, Visual, ctypes.c_uint, ctypes.c_int, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_int, ctypes.c_int]

x11.XPutImage.argtypes = [Display, Drawable, GC, XImagePtr, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_uint, ctypes.c_uint]
x11.XFlush.argtypes = [Display]
x11.XSync.argtypes = [Display, ctypes.c_int]

x11.XNextEvent.argtypes = [Display, ctypes.POINTER(XEvent)]
x11.XPending.restype = ctypes.c_int
x11.XPending.argtypes = [Display]
x11.XDefaultScreen.restype = ctypes.c_int
x11.XDefaultScreen.argtypes = [Display]
x11.XDefaultVisual.restype = Visual
x11.XDefaultVisual.argtypes = [Display, ctypes.c_int]
x11.XDefaultDepth.restype = ctypes.c_int
x11.XDefaultDepth.argtypes = [Display, ctypes.c_int]
x11.XRootWindow.restype = Window
x11.XRootWindow.argtypes = [Display, ctypes.c_int]
x11.XBlackPixel.restype = ctypes.c_ulong
x11.XWhitePixel.restype = ctypes.c_ulong

class X11Window:
    def __init__(self, width, height, title="Pixel Engine"):
        self.width = width
        self.height = height
        
        self.display = x11.XOpenDisplay(None)
        if not self.display:
            raise RuntimeError("Cannot open X11 display")
            
        self.screen_num = x11.XDefaultScreen(self.display)
        self.root_win = x11.XRootWindow(self.display, self.screen_num)
        self.visual = x11.XDefaultVisual(self.display, self.screen_num)
        self.depth = x11.XDefaultDepth(self.display, self.screen_num)
        self.black = x11.XBlackPixel(self.display, self.screen_num)
        self.white = x11.XWhitePixel(self.display, self.screen_num)
        
        # Create Window
        self.window = x11.XCreateSimpleWindow(
            self.display,
            self.root_win,
            0, 0,
            width, height,
            1,
            self.black,
            self.black
        )
        
        # Select Inputs
        x11.XSelectInput(self.display, self.window, KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask | ExposureMask)
        
        # Map (Show)
        x11.XMapWindow(self.display, self.window)
        
        # GC
        self.gc = x11.XCreateGC(self.display, self.window, 0, None)
        
        # Create XImage container
        # We need a buffer that python can write to. ctypes string buffer.
        # 32-bit color (BGRA usually on X11 little endian) -> 4 bytes per pixel
        self.buffer_size = width * height * 4
        self.image_buffer = ctypes.create_string_buffer(self.buffer_size)
        
        # Explicit Cast to char* to ensure XCreateImage receives strictly address
        c_data = ctypes.cast(self.image_buffer, ctypes.c_char_p)

        self.ximage = x11.XCreateImage(
            self.display,
            self.visual,
            self.depth,
            ZPixmap,
            0,
            c_data, # data
            width, height,
            32, # bitmap_pad
            0 # bytes_per_line (0 = auto)
        )
        
        if not self.ximage:
             raise RuntimeError("XCreateImage failed")

        # Wait for map
        self.flush()

    def put_buffer(self, bytes_data):
        """
        Copies pure bytes into the C buffer and flushes to window.
        Assumes bytes_data is BGRA/RGBA compatible with X11 visual.
        """
        # copy to buffer
        ctypes.memmove(self.image_buffer, bytes_data, len(bytes_data))
        
        x11.XPutImage(
            self.display,
            self.window,
            self.gc,
            self.ximage,
            0, 0, # src_x, src_y
            0, 0, # dst_x, dst_y
            self.width, self.height
        )
        # x11.XFlush(self.display) # Flush done implicitly by next event check logic usually, or explicitly

    def flush(self):
        x11.XFlush(self.display)

    def pending_events(self):
        return x11.XPending(self.display)

    def next_event(self):
        ev = XEvent()
        x11.XNextEvent(self.display, ctypes.byref(ev))
        return ev
