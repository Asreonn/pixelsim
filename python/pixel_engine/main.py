
import sys
import os
import time

# Fix Import Path to allow running as script
current_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(current_dir)
if project_root not in sys.path:
    sys.path.insert(0, project_root)

from pixel_engine.config import FPS, pixelSize, screenSize
from pixel_engine.core.grid import Grid
from pixel_engine.physics.engine import PhysicsEngine
from pixel_engine.graphics.x11 import X11Window, KeyPress, ButtonPress, ButtonRelease, MotionNotify
from pixel_engine.graphics.renderer import Renderer
from pixel_engine.materials.registry import ID_SAND, ID_WATER, ID_FIRE, ID_STEAM, ID_MUD, ID_AIR

def main():
    # Init X11 Window
    win = X11Window(screenSize[0], screenSize[1], "Pixel Engine (X11 Pure)")
    
    # Init Core
    grid = Grid()
    physics = PhysicsEngine(grid)
    renderer = Renderer(win)
    
    # State
    running = True
    current_material = ID_SAND
    brush_size = 2
    mouse_down = False
    mx, my = 0, 0
    
    # Time
    target_frame_time = 1.0 / FPS
    
    print("Engine Started. Press 'Q' to quit.")
    
    while running:
        t_start = time.time()
        
        # 1. Event Loop (Non-blocking check)
        while win.pending_events() > 0:
            ev = win.next_event()
            
            if ev.type == KeyPress:
                # Basic key mapping (keycode depends on layout, but Q is usually 24 on Linux/standard)
                # We can print ev.xkey.keycode to find out easily, or just map digits.
                # Standard digits 1-9 are often 10-19.
                # '1' = 10 (on my machine), 'Q' = 24.
                # 'Esc' = 9.
                kc = ev.xkey.keycode
                # print(f"Key: {kc}") 
                if kc == 24: # Q
                    running = False
                elif kc == 10: current_material = ID_SAND # 1
                elif kc == 11: current_material = ID_WATER # 2
                elif kc == 12: current_material = ID_MUD # 3
                elif kc == 13: current_material = ID_FIRE # 4
                elif kc == 14: current_material = ID_STEAM # 5
                elif kc == 19: current_material = ID_AIR # 0 (Usually key 19 is '0')
                elif kc == 27: grid.reset() # R
                
            elif ev.type == ButtonPress:
                if ev.xbutton.button == 1: # Left Click
                    mouse_down = True
                    mx, my = ev.xbutton.x, ev.xbutton.y
            elif ev.type == ButtonRelease:
                if ev.xbutton.button == 1:
                    mouse_down = False
            elif ev.type == MotionNotify:
                mx, my = ev.xmotion.x, ev.xmotion.y
        
        # 2. Input Logic
        if mouse_down:
            gx = mx // pixelSize
            gy = my // pixelSize
            
            for y in range(gy - brush_size, gy + brush_size):
                for x in range(gx - brush_size, gx + brush_size):
                    if grid.is_within_bounds(x, y):
                        grid.set_material(x, y, current_material)
                        if current_material == ID_FIRE:
                            grid.set_temperature(x, y, 850.0)

        # 3. Physics
        physics.update()
        
        # 4. Render
        renderer.render(grid)
        
        # 5. Cap FPS
        t_end = time.time()
        elapsed = t_end - t_start
        if elapsed < target_frame_time:
            time.sleep(target_frame_time - elapsed)

if __name__ == "__main__":
    main()
