#include <lv2/ui/ui.h>
#include <X11/Xlib.h>
#include <cairo/cairo-xlib.h>
#include <dlfcn.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <thread>

namespace {
void require(bool value, const char* message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
void capture(LV2UI_Controller controller, uint32_t index, uint32_t size,
             uint32_t protocol, const void* value) {
    if (size == sizeof(float) && protocol == 0)
        (*static_cast<std::map<unsigned, float>*>(controller))[index] = *static_cast<const float*>(value);
}
void click(Display* d, Window w, int x, int y) {
    XEvent event{};
    event.xbutton.type = ButtonPress;
    event.xbutton.display = d;
    event.xbutton.window = w;
    event.xbutton.root = DefaultRootWindow(d);
    event.xbutton.button = Button1;
    event.xbutton.x = x;
    event.xbutton.y = y;
    event.xbutton.same_screen = True;
    XSendEvent(d, w, False, ButtonPressMask, &event);
    XSync(d, False);
}
void key(Display* d, Window w, KeySym symbol) {
    XEvent event{};
    event.xkey.type = KeyPress;
    event.xkey.display = d;
    event.xkey.window = w;
    event.xkey.keycode = XKeysymToKeycode(d,symbol);
    XSendEvent(d,w,False,KeyPressMask,&event);
    XSync(d,False);
}
void screenshot(Display* d, Window w, const char* filename) {
    XWindowAttributes attrs{};
    require(XGetWindowAttributes(d,w,&attrs) != 0, "read UI geometry");
    auto* src = cairo_xlib_surface_create(d,w,attrs.visual,attrs.width,attrs.height);
    auto* dst = cairo_image_surface_create(CAIRO_FORMAT_RGB24,attrs.width,attrs.height);
    auto* context = cairo_create(dst);
    cairo_set_source_surface(context,src,0,0);
    cairo_paint(context);
    cairo_destroy(context);
    require(cairo_surface_write_to_png(dst,filename)==CAIRO_STATUS_SUCCESS, "write real UI screenshot");
    cairo_surface_destroy(src);
    cairo_surface_destroy(dst);
}
} // namespace

int main(int argc, char** argv) {
    require(argc == 3, "expected LV2 UI library and screenshot path");
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::cerr << dlerror() << '\n'; return 1; }
    using Descriptor = const LV2UI_Descriptor* (*)(uint32_t);
    auto get = reinterpret_cast<Descriptor>(dlsym(library,"lv2ui_descriptor"));
    require(get != nullptr, "load LV2 UI descriptor");
    const auto* ui = get(0);
    Display* display = XOpenDisplay(nullptr);
    require(ui && display, "instantiate under a real X server");
    const auto parent = XCreateSimpleWindow(display,DefaultRootWindow(display),0,0,940,620,0,0,0);
    XMapWindow(display,parent);
    XSync(display,False);
    LV2_Feature parentFeature{LV2_UI__parent,reinterpret_cast<void*>(parent)};
    const LV2_Feature* features[]{&parentFeature,nullptr};
    LV2UI_Widget widget{};
    std::map<unsigned,float> values;
    auto instance = ui->instantiate(ui,"https://weikiboy-tech.github.io/voiceforge-lv2/mono","",
        capture,&values,&widget,features);
    require(instance && widget, "embed VoiceForge UI in X11 host window");
    const auto window = reinterpret_cast<Window>(widget);
    const auto* idle=static_cast<const LV2UI_Idle_Interface*>(ui->extension_data(LV2_UI__idleInterface));
    const auto* resize=static_cast<const LV2UI_Resize*>(ui->extension_data(LV2_UI__resize));
    require(idle && resize && resize->ui_resize(instance,940,620)==0, "idle and host resize features");
    const auto update=[&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(45));
        require(idle->idle(instance)==0,"paint UI");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };
    update();
    screenshot(display,window,argv[2]);
    click(display,window,725,31); update();
    click(display,window,274,307); update();
    require(values.contains(6) && values[6] == 65.0f, "factory preset applied via host high pass automation port");

    click(display,window,634,31); update();
    click(display,window,556,31); update();
    click(display,window,439,31); update();
    require(values[4] == 0.0f,"bypass received by host");
    click(display,window,511,31); update();
    require(values[4] == 1.0f,"A/B restores active comparison values");
    click(display,window,741,81); update();
    click(display,window,74,595); update();
    bool command=false;
    for (const auto& [port,value] : values)
        if (port>=75 && value==1.0f) command=true;
    require(command,"analysis command writes an input control");
    key(display,window,0xff1b); update();
    ui->cleanup(instance);
    XDestroyWindow(display,parent);
    XCloseDisplay(display);
    dlclose(library);
}
