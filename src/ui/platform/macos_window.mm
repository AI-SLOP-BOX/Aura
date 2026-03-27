#include <iostream>
#include <istream>
#include <ostream>
#include <random>
#include <vector>
#include <string>
#undef timeout
#undef check
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>
#include "../../AuraUltimate.hpp"
#include "../main/app_view.hpp"
#include "../../core/driver/mac_audio_driver.hpp"

@interface AuraAppDelegate : NSObject <NSApplicationDelegate, MTKViewDelegate>
@property (strong) NSWindow *window;
@property (strong) MTKView *metalView;
@property (assign) ::Aura::Core::Driver::MacAudioDriver* audioDriver;
@end

@implementation AuraAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    NSRect frame = NSMakeRect(0, 0, 1280, 800);
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
    
    _window = [[NSWindow alloc] initWithContentRect:frame
                                             styleMask:style
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];
    [_window setTitle:@"Aura Studio Pro Ultimate"];
    [_window setBackgroundColor:[NSColor blackColor]];
    
    // Setup Metal View
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    _metalView = [[MTKView alloc] initWithFrame:frame device:device];
    [_metalView setDelegate:self];
    [_metalView setPaused:NO];
    [_metalView setEnableSetNeedsDisplay:NO];
    
    [_window setContentView:_metalView];
    [_window makeKeyAndOrderFront:nil];
    [_window center];
    [NSApp activateIgnoringOtherApps:YES];

    // BOOT ENGINE
    auto& engine = ::Aura::AuraEngine::getInstance();
    const double sampleRate = 44100.0;
    const uint32_t blockSize = 512;
    engine.initialize(sampleRate, blockSize);

    // BOOT AUDIO
    _audioDriver = new ::Aura::Core::Driver::MacAudioDriver([&engine](float* l, float* r, uint32_t len) {
        engine.process(l, r, len);
    });
    _audioDriver->start(sampleRate, blockSize);

    auto& appView = ::Aura::UI::Main::AuraAppView::getInstance();
    appView.bootstrap((__bridge void*)_metalView, _metalView.bounds.size.width, _metalView.bounds.size.height);
    appView.setScale(_metalView.layer.contentsScale);

    // MOUSE HANDLING INJECTION
    NSEventMask mask = NSEventMaskLeftMouseDown | NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp | NSEventMaskMouseMoved | NSEventMaskKeyDown;
    [NSEvent addLocalMonitorForEventsMatchingMask:mask handler:^NSEvent * _Nullable(NSEvent * _Nonnull event) {
        auto& view = ::Aura::UI::Main::AuraAppView::getInstance();
        
        if (event.type == NSEventTypeKeyDown) {
            bool cmd = (event.modifierFlags & NSEventModifierFlagCommand) != 0;
            bool shift = (event.modifierFlags & NSEventModifierFlagShift) != 0;
            view.handleKeyDown(event.keyCode, cmd, shift);
            return nil; // Consume key events for the DAW
        }

        NSPoint p = [_metalView convertPoint:[event locationInWindow] fromView:nil];
        // Flip Y for DAW coordinates (top-left 0,0)
        float fx = p.x;
        float fy = _metalView.bounds.size.height - p.y;
        
        if (event.type == NSEventTypeLeftMouseDown) view.handleMouseDown(fx, fy);
        else if (event.type == NSEventTypeLeftMouseDragged) view.handleMouseDrag(fx, fy);
        else if (event.type == NSEventTypeLeftMouseUp) view.handleMouseUp(fx, fy);
        
        return event;
    }];

    std::cout << "[macOS] GUI Host launched with HiDPI Scale: " << _metalView.layer.contentsScale << std::endl;
}

- (void)drawInMTKView:(MTKView *)view {
    ::Aura::UI::Main::AuraAppView::getInstance().updateUI();
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    float bw = view.bounds.size.width;
    float bh = view.bounds.size.height;
    auto& appView = ::Aura::UI::Main::AuraAppView::getInstance();
    appView.onResize(bw, bh);
    appView.setScale(view.layer.contentsScale);
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    if (_audioDriver) {
        _audioDriver->stop();
        delete _audioDriver;
    }
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AuraAppDelegate *delegate = [[AuraAppDelegate alloc] init];
        [app setDelegate:delegate];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app run];
    }
    return 0;
}
