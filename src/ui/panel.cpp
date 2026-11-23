#include "voiceforge_ui_data.hpp"
#include "common/compare_state.hpp"
#include "dsp/analysis/spectrum.hpp"

#include <lv2/ui/ui.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo-xlib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace voiceforge::ui {
namespace {
constexpr char uri[] = "https://weikiboy-tech.github.io/voiceforge-lv2/ui";
constexpr int width = 940, height = 620;
constexpr std::size_t parameterCount = std::size(params);
static_assert(parameterCount <= compareCapacity);
using Values = std::array<float, parameterCount>;
using Clock = std::chrono::steady_clock;

struct Color { double r, g, b, a = 1; };
constexpr Color text{.88,.92,.93}, muted{.54,.62,.65}, cyan{.40,.89,.88};
constexpr Color blue{.25,.58,.92}, surface{.096,.12,.145}, line{.19,.24,.28}, dim{.055,.071,.086};
constexpr Color red{1,.42,.38};
void color(cairo_t* c, Color v) { cairo_set_source_rgba(c, v.r, v.g, v.b, v.a); }

struct Rect {
    double x,y,w,h;
    bool contains(double px, double py) const { return px >= x && py >= y && px <= x+w && py <= y+h; }
};
void box(cairo_t* c, Rect r, Color fill) {
    cairo_new_path(c);
    constexpr double s = 6;
    cairo_arc(c,r.x+r.w-s,r.y+s,s,-std::numbers::pi/2,0);
    cairo_arc(c,r.x+r.w-s,r.y+r.h-s,s,0,std::numbers::pi/2);
    cairo_arc(c,r.x+s,r.y+r.h-s,s,std::numbers::pi/2,std::numbers::pi);
    cairo_arc(c,r.x+s,r.y+s,s,std::numbers::pi,1.5*std::numbers::pi);
    cairo_close_path(c);
    color(c, fill);
    cairo_fill(c);
}
void label(cairo_t* c, double x, double y, const std::string& value, double size = 14,
           Color fill = text, bool strong = false) {
    color(c, fill);
    cairo_select_font_face(c,"Sans",CAIRO_FONT_SLANT_NORMAL,strong ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(c,size);
    cairo_move_to(c,x,y);
    cairo_show_text(c,value.c_str());
}
void stroke(cairo_t* c, double x, double y, double a, double b, Color v) {
    color(c, v);
    cairo_move_to(c,x,y); cairo_line_to(c,a,b); cairo_stroke(c);
}

int parameter(std::string_view symbol) {
    for (std::size_t i=0;i<parameterCount;++i)
        if (symbol==params[i].symbol) return static_cast<int>(i);
    return -1;
}
bool persistent(const Param& p) {
    const std::string_view s=p.symbol;
    return !p.output && s!="analyze" && s!="analysis_reset" && s!="meter_reset";
}
Values defaults() {
    Values values{};
    for (std::size_t i=0;i<values.size();++i) values[i]=params[i].initial;
    return values;
}

struct Group { const char* name; std::vector<std::string_view> prefix; };
const std::array<std::vector<Group>,3> groups{{
    {{"Input / EQ", {"input_gain","high_pass","low_shelf","eq_","high_shelf","phase_invert","dc_removal"}},
     {"Compressor", {"comp_"}}, {"De-Esser", {"de_esser_"}}, {"Expander / Rider", {"exp_","rider_","vad_"}}},
    {{"Noise Reduction", {"noise_"}}, {"De-Click", {"de_click_"}}, {"De-Plosive", {"de_plosive_"}},
     {"Dereverb", {"room_"}}},
    {{"Output / Limiter", {"output_gain","limiter_"}}}
}};
enum Action { SelectPage=1000, SelectGroup=1010, Mode=1020, Bypass, Presets, SideA, SideB,
              Copy, ResetMeter, StartAnalysis, ResetAnalysis, Apply, Undo, PrevPreset, NextPreset,
              LoadPreset, SavePreset, RenamePreset, DeletePreset, ClosePreset, Scale,
              Differences, CloseDifferences, PrevDifference, NextDifference };
struct Hit { Rect rect; int id; };
enum class Edit { none, create, rename };

class Panel {
public:
    Panel(Display* display, Window parent, LV2UI_Write_Function write, LV2UI_Controller ctl,
          unsigned audio, const LV2UI_Touch* touch, LV2UI_Resize* hostResize, const LV2_URID_Map* map)
        : display_(display), write_(write), controller_(ctl), audio_(audio), touch_(touch),
          hostResize_(hostResize), ids_(map), values_(defaults()) {
        const int screen = DefaultScreen(display_);
        const double dpi = 25.4 * DisplayWidth(display_,screen) / std::max(1,DisplayWidthMM(display_,screen));
        requestedScale_ = dpi >= 155 ? 1.5 : 1.0;
        pixelsW_=static_cast<int>(width*requestedScale_); pixelsH_=static_cast<int>(height*requestedScale_);
        window = XCreateSimpleWindow(display_,parent,0,0,pixelsW_,pixelsH_,0,0,0x0d1318);
        XStoreName(display_,window,"VoiceForge LV2 Panel");
        XWindowAttributes attrs{};
        XGetWindowAttributes(display_,window,&attrs);
        canvas_ = cairo_xlib_surface_create(display_,window,attrs.visual,pixelsW_,pixelsH_);
        XSelectInput(display_,window,ExposureMask|StructureNotifyMask|ButtonPressMask|
                     ButtonReleaseMask|PointerMotionMask|KeyPressMask);
        XMapWindow(display_,window);
        XSync(display_,False);
        if (hostResize_) hostResize_->ui_resize(hostResize_->handle,pixelsW_,pixelsH_);
        refreshPresets();
    }
    ~Panel() {
        cairo_surface_destroy(canvas_);
        XDestroyWindow(display_,window);
        XCloseDisplay(display_);
    }
    Window window{};
    void incoming(unsigned port, float data) {
        if (port<audio_ || !std::isfinite(data)) return;
        const unsigned offset=port-audio_;
        const auto found=std::lower_bound(std::begin(params),std::end(params),offset,
            [](const Param& entry,unsigned index){return entry.offset<index;});
        if (found==std::end(params) || found->offset!=offset) return;
        const auto i=static_cast<unsigned>(found-std::begin(params));
        if (drag_!=static_cast<int>(i) && values_[i]!=data) { values_[i]=data; dirty_=true; }
    }
    void incoming(unsigned port, unsigned size, unsigned protocol, const void* data) {
        if (!ids_.eventTransfer || protocol != ids_.eventTransfer ||
            port != audio_ + compareOutputOffset || size != sizeof(LV2_Atom) + sizeof(CompareState))
            return;
        LV2_Atom atom{};
        std::memcpy(&atom,data,sizeof(atom));
        if (atom.type != ids_.vector || atom.size != sizeof(CompareState)) return;
        CompareState state;
        std::memcpy(&state,static_cast<const char*>(data)+sizeof(atom),sizeof(state));
        if (state.vector.child_type != ids_.floating || state.vector.child_size != sizeof(float)) return;
        if (!received_) {
            side_=state.values[0]>.5f ? 1:0;
            const float oldCount=state.values.back();
            const unsigned stateCount=std::isfinite(oldCount) && oldCount>=1 && oldCount<=compareCapacity-1
                ? static_cast<unsigned>(oldCount) : compareInputOffset;
            for (unsigned s=0;s<2;++s) {
                haveSide_[s]=state.values[1+s]>.5f;
                for (unsigned i=0;i<parameterCount;++i) {
                    const auto value=state.values[3+s*compareCapacity+i];
                    compare_[s][i]=i<stateCount && std::isfinite(value)
                        ? std::clamp(value,params[i].min,params[i].max) : params[i].initial;
                }
            }
        }
        received_=true;
        dirty_=true;
    }
    int size(int w, int h) {
        if (w<1 || h<1) return 1;
        XResizeWindow(display_,window,w,h);
        return 0;
    }
    int idle() {
        while (XPending(display_)) {
            XEvent e; XNextEvent(display_,&e);
            if (e.type==ConfigureNotify) {
                pixelsW_=e.xconfigure.width; pixelsH_=e.xconfigure.height;
                cairo_xlib_surface_set_size(canvas_, pixelsW_, pixelsH_); dirty_=true;
            } else if (e.type==Expose) dirty_=true;
            else if (e.type==ButtonPress) press(e.xbutton);
            else if (e.type==MotionNotify && drag_>=0) {
                const double x=e.xmotion.x/scaleX();
                const double delta=(x-origin_)/dragWidth_;
                set(drag_, fromScale(drag_, dragValue_+delta*(e.xmotion.state&ShiftMask ? .12 : 1)));
            } else if (e.type==ButtonRelease && drag_>=0) {
                touching(drag_,false); drag_=-1;
            } else if (e.type==KeyPress) key(e.xkey);
        }
        const auto now=Clock::now();
        if (ids_.eventTransfer && !received_ && requests_<12 && now>=nextRequest_) {
            struct { LV2_Atom header; int32_t value; } request{{sizeof(int32_t),ids_.boolean},1};
            write_(controller_,audio_+compareInputOffset,sizeof(request),ids_.eventTransfer,&request);
            ++requests_;
            nextRequest_=now+std::chrono::milliseconds(350);
        }
        if (pulse_>=0 && now>=pulseEnd_) {
            set(pulse_,0.0f); pulse_=-1;
        }
        if (dirty_ && now-lastPaint_>=std::chrono::milliseconds(28)) {
            draw(); lastPaint_=now; dirty_=false;
        }
        return 0;
    }

private:
    double scaleX() const { return static_cast<double>(pixelsW_)/width; }
    double scaleY() const { return static_cast<double>(pixelsH_)/height; }
    float get(std::string_view s) const { const int i=parameter(s); return i<0 ? 0.0f : values_[i]; }
    void touching(int i,bool active) {
        if (touch_ && touch_->touch) touch_->touch(touch_->handle,params[i].offset+audio_,active);
    }
    void set(int i, float value) {
        if (i<0 || params[i].output) return;
        values_[i]=std::clamp(value,params[i].min,params[i].max);
        write_(controller_,params[i].offset+audio_,sizeof(float),0,&values_[i]);
        dirty_=true;
    }
    void set(std::string_view s,float v) { set(parameter(s),v); }
    void pulse(std::string_view s) {
        if (pulse_>=0) set(pulse_,0.0f);
        pulse_=parameter(s); set(pulse_,1.0f); pulseEnd_=Clock::now()+std::chrono::milliseconds(350);
    }
    bool logScale(int i) const {
        const std::string_view u=params[i].unit;
        return (u=="hz" || u=="ms") && params[i].min>0 && params[i].max>params[i].min*3;
    }
    double toScale(int i, float value) const {
        const auto& p=params[i];
        if (logScale(i)) return std::log(std::max<double>(value,p.min)/p.min)/std::log(p.max/p.min);
        return (value-p.min)/(p.max-p.min);
    }
    float fromScale(int i, double t) const {
        const auto& p=params[i]; t=std::clamp(t,0.0,1.0);
        if (logScale(i)) return static_cast<float>(p.min*std::pow(p.max/p.min,t));
        return static_cast<float>(std::lerp<double>(p.min,p.max,t));
    }
    void apply(const Values& target) {
        for (std::size_t i=0;i<parameterCount;++i)
            if (persistent(params[i]) && target[i]!=values_[i]) {
                touching(static_cast<int>(i),true); set(static_cast<int>(i),target[i]); touching(static_cast<int>(i),false);
            }
    }
    static std::string format(float v, std::string_view unit="",bool meter=false) {
        if (meter && v<=-119.9) return "—";
        char b[40];
        const int precision=unit=="hz" || (unit=="ms" && v>=10) ? 0 : 1;
        std::snprintf(b,sizeof(b),"%.*f",precision,v);
        if (unit=="db") return std::string(b)+" dB";
        if (unit=="hz") return std::string(b)+" Hz";
        if (unit=="ms") return std::string(b)+" ms";
        return b;
    }
    void button(cairo_t* c,Rect r,const std::string& str,int id,bool selected=false) {
        box(c,r,selected ? Color{.14,.30,.33} : surface);
        label(c,r.x+10,r.y+r.h/2+5,str,13,selected ? cyan : text,selected);
        hits_.push_back({r,id});
    }
    void bar(cairo_t* c,double x,double y,double w,double h,float v,Color colorValue) {
        box(c,{x,y,w,h},dim);
        const double amount=std::clamp(static_cast<double>(v),0.0,1.0);
        if (amount>0) {
            color(c,colorValue); cairo_rectangle(c,x+2,y+2,(w-4)*amount,h-4); cairo_fill(c);
        }
    }
    double response(double freq,double rate) const {
        const double warp=std::tan(std::numbers::pi*freq/rate);
        double amp=1.0;
        const double cut=get("high_pass");
        if (cut>0) {
            const double z=std::exp(-2.0*std::numbers::pi*std::clamp(cut,20.0,300.0)/rate);
            const auto e=std::polar(1.0,-2.0*std::numbers::pi*freq/rate);
            amp*=std::abs(1.0-(1.0-z)/(1.0-z*e));
        }
        for (unsigned i=0;i<6;++i) {
            const std::string key=i==0 ? "low_shelf" : (i==5 ? "high_shelf" : "eq_"+std::to_string(i));
            if (get(key+"_enabled")<.5f) continue;
            const auto a=std::pow(10.0,get(key+"_gain")/40.0);
            double g=std::tan(std::numbers::pi*std::min<double>(get(key+"_frequency"),.475*rate)/rate);
            double k=1.0/std::max(.25f,get(key+"_q")), direct=1.0, band=0.0, low=0.0;
            if (i==0) { g/=std::sqrt(a); band=k*(a-1); low=a*a-1; }
            else if (i==5) { g*=std::sqrt(a); direct=a*a; band=k*(1-a)*a; low=1-a*a; }
            else { k/=a; band=k*(a*a-1); }
            const double x=warp/g;
            const std::complex<double> jw(0,x);
            amp*=std::abs(direct+(band*jw+low)/(1.0-x*x+k*jw));
        }
        return 20*std::log10(std::max(amp,1e-9));
    }
    void graph(cairo_t* c) {
        box(c,{18,114,674,175},dim);
        label(c,31,136,"INPUT SPECTRUM",11,blue,true);
        label(c,183,136,"EQ RESPONSE",11,cyan,true);
        const double reportedRate=get("sample_rate");
        const double rate=reportedRate<8000.0 ? 48000.0 : reportedRate;
        const double maxHz=std::min(20000.0,rate*.475);
        const auto x=[&](double f){ return 33.0+640.0*std::log(f/40)/std::log(maxHz/40); };
        cairo_save(c); cairo_rectangle(c,31,146,643,116); cairo_clip(c);
        cairo_set_line_width(c,1);
        for (double f:{60,100,250,630,1000,2500,6300,10000})
            if (f<maxHz) stroke(c,x(f),146,x(f),261,line);
        for (unsigned i=0;i<Spectrum::frequencies.size();++i) {
            const double f=Spectrum::frequencies[i];
            if (f>=maxHz) break;
            double h=112*std::clamp((get("spectrum_"+std::to_string(i))+78)/68.0,0.0,1.0);
            color(c,{.20,.51,.79,.39});
            cairo_rectangle(c,x(f)-13,261-h,26,h); cairo_fill(c);
        }
        stroke(c,31,204,674,204,line);
        for (int i=0;i<320;++i) {
            const double f=40*std::pow(maxHz/40.0,i/319.0);
            const double yy=204-3.1*response(f,rate);
            if (i==0) cairo_move_to(c,33,yy); else cairo_line_to(c,x(f),yy);
        }
        cairo_set_line_width(c,2); color(c,cyan); cairo_stroke(c); cairo_restore(c);
        for (auto [f,s]:std::array<std::pair<int,const char*>,6>{{{60,"60"},{160,"160"},{400,"400"},
                {1000,"1k"},{2500,"2.5k"},{10000,"10k"}}})
            if(f<maxHz) label(c,x(f)-7,279,s,11,muted);
    }
    void meters(cairo_t* c) {
        box(c,{708,114,214,175},surface);
        label(c,720,136,"LEVELS",11,muted,true);
        auto meter=[&](double y,const char* name,const char* s,const char* clip) {
            label(c,720,y,name,12,get(clip)>.5f ? red : text);
            label(c,823,y,format(get(s),"db",true),12);
            bar(c,720,y+7,190,10,(get(s)+60)/63,get(clip)>.5f ? red : cyan);
        };
        meter(157,"Input","input_peak","input_clip"); meter(191,"Output","output_peak","output_clip");
        label(c,720,225,"True peak",12,muted); label(c,822,225,format(get("true_peak"),"",true)+" dBTP",12);
        label(c,720,246,"Integrated",12,muted); label(c,822,246,format(get("lufs_i"),"",true)+" LUFS",12);
        label(c,720,270,get("voice_active")>=.5f ? "● Voice detected":"○ Waiting for voice",12,
              get("voice_active")>=.5f ? blue : muted);
    }
    void drawControls(cairo_t* c) {
        const auto& region=groups[page_];
        int x=21;
        for (unsigned i=0;i<region.size();++i) {
            int w=18+static_cast<int>(std::strlen(region[i].name))*8;
            button(c,{double(x),307,double(w),28},region[i].name,SelectGroup+i,group_==i); x+=w+8;
        }
        std::vector<int> visible;
        for (std::size_t i=0;i<parameterCount;++i) {
            const auto& p=params[i];
            if (!persistent(p)) continue;
            const std::string_view symbol=p.symbol;
            if (std::any_of(region[group_].prefix.begin(),region[group_].prefix.end(),
                      [&](std::string_view prefix){return symbol.starts_with(prefix);})) visible.push_back(static_cast<int>(i));
        }
        const int rows=static_cast<int>((visible.size()+2)/3);
        maxScroll_=std::max(0,rows-4);
        scroll_=std::clamp(scroll_,0,maxScroll_);
        for (unsigned r=0;r<4;++r)
            for (unsigned col=0;col<3;++col) {
                const unsigned slot=3*(scroll_+r)+col;
                if (slot>=visible.size()) continue;
                const int i=visible[slot]; const auto& p=params[i];
                const double x=20+303*col, y=346+58*r;
                Rect card{x,y,292,50};
                box(c,card,surface);
                label(c,x+10,y+20,p.label,12,selected_==i ? cyan : text,true);
                if(p.toggle) {
                    const Rect switcher{x+231,y+11,49,28};
                    box(c,switcher,values_[i]>.5f ? Color{.12,.31,.33} : dim);
                    label(c,x+241,y+29,values_[i]>.5f ? "On":"Off",12,values_[i]>.5f ? cyan : muted);
                } else {
                    label(c,x+230,y+20,format(values_[i],p.unit),11,muted);
                    bar(c,x+8,y+30,274,12,static_cast<float>(toScale(i,values_[i])),blue);
                }
                hits_.push_back({card,i});
            }
        if (maxScroll_>0) label(c,755,597,"Scroll to see more controls",11,muted);
        if(page_==2) {
            label(c,25,597,"Momentary  "+format(get("lufs_m"),"",true)+"   Short-term  "+
                  format(get("lufs_s"),"",true)+" LUFS",13,muted);
            button(c,{750,574,169,30},"Reset measurements",ResetMeter);
        } else {
            label(c,23,601,"Gain reduction   De-Esser "+format(get("de_esser_reduction"),"db")+
                  "    Compressor "+format(get("comp_reduction"),"db")+"    Limiter "+
                  format(get("limiter_reduction"),"db"),12,muted);
        }
        if (page_==1 && get("mode")<.5f && group_!=2)
            label(c,600,327,"Select Studio to use this module",12,cyan);
    }
    void drawAnalysis(cairo_t* c) {
        box(c,{20,306,900,260},surface);
        const float progress=get("analysis_progress");
        label(c,37,335,"Analyze Voice",21,text,true);
        label(c,38,358,progress>=1 ? "Analysis complete" : "Play the recording to measure eight seconds of input.",14,muted);
        bar(c,37,372,862,12,progress,cyan);
        if(progress>=1) {
            label(c,40,410,"Peak  "+format(get("analysis_peak"),"db",true)+"    RMS  "+format(get("analysis_rms"),"db",true)+
                  "    Noise floor  "+format(get("analysis_floor"),"db",true)+"    Loudness  "+
                  format(get("analysis_loudness"),"",true)+" LUFS",14);
            label(c,40,439,"Dynamics  "+format(get("analysis_range"),"db")+"    Voice  "+
                  std::to_string(static_cast<int>(get("analysis_speech")*100))+"%    Clipping  "+
                  (get("analysis_clip")>.5f ? "Detected":"Clear"),13,muted);
            label(c,40,469,"Bass  "+std::to_string(static_cast<int>(get("analysis_low")*100))+"%    High band  "+
                  std::to_string(static_cast<int>(get("analysis_high")*100))+"%    Sibilance  "+
                  std::to_string(static_cast<int>(get("analysis_sibilance")*100))+"%",13,muted);
            if(get("recommend_hpf")>0) {
                label(c,40,502,"Suggested  •  High pass "+format(get("recommend_hpf"),"hz")+
                      "   Compressor "+format(get("recommend_comp"),"db")+
                      "   De-Esser "+format(get("recommend_ess"),"hz"),13,cyan);
                label(c,40,526,get("recommend_noise")>0 && get("mode")>.5f
                      ? "Studio noise reduction  "+format(get("recommend_noise"),"db")
                      : "Spectral noise settings apply in Studio mode.",12,muted);
            } else label(c,40,502,"Play a section with more audible speech to get recommendations.",13,muted);
        }
        button(c,{20,579,127,31},progress>0 && progress<1 ? "Stop analysis" : "Analyze Voice",StartAnalysis);
        button(c,{153,579,68,31},"Reset",ResetAnalysis);
        if(progress>=1 && get("recommend_hpf")>0)
            button(c,{227,579,180,31},"Apply recommendations",Apply,true);
        if(beforeAnalysis_) button(c,{414,579,98,31},"Compare",Undo);
    }
    void drawPresets(cairo_t* c) {
        color(c,{.025,.04,.05,.83}); cairo_rectangle(c,0,0,width,height); cairo_fill(c);
        box(c,{232,169,476,273},surface);
        label(c,254,203,"Presets",22,text,true);
        button(c,{614,181,76,30},"Close",ClosePreset);
        button(c,{253,221,37,31},"‹",PrevPreset);
        button(c,{649,221,40,31},"›",NextPreset);
        label(c,306,242,names_[preset_],16,cyan,true);
        label(c,253,277,preset_<std::size(factoryNames) ? "Factory preset":"Saved on this computer",13,muted);
        button(c,{253,291,71,30},"Load",LoadPreset,true);
        button(c,{332,291,87,30},"Save as",SavePreset);
        if (preset_>=std::size(factoryNames)) {
            button(c,{427,291,83,30},"Rename",RenamePreset);
            button(c,{518,291,79,30},"Delete",DeletePreset);
        }
        if (edit_!=Edit::none) {
            label(c,253,355,"Name:  "+editing_+"_",15,cyan);
            label(c,253,378,"Enter to save · Esc to cancel",12,muted);
        }
        if (!status_.empty()) label(c,253,414,status_,13,text);
    }
    void drawDifferences(cairo_t* c) {
        color(c,{.025,.04,.05,.83});
        cairo_rectangle(c,0,0,width,height); cairo_fill(c);
        box(c,{165,119,610,378},surface);
        label(c,187,153,"A/B differences",22,text,true);
        button(c,{676,132,76,30},"Close",CloseDifferences);
        const Values& a=side_==0 ? values_ : compare_[0];
        const Values& b=side_==1 ? values_ : compare_[1];
        std::vector<unsigned> changed;
        if(haveSide_[1-side_]) {
            for(unsigned i=0;i<parameterCount;++i)
                if(persistent(params[i]) && std::abs(a[i]-b[i])>1e-4f)
                    changed.push_back(i);
        }
        if(changed.empty()) {
            label(c,188,199,haveSide_[1-side_] ? "Both settings are the same." :
                  "Switch sides or copy the current settings to start a comparison.",14,muted);
            return;
        }
        constexpr unsigned pageSize=7;
        const unsigned pages=static_cast<unsigned>((changed.size()+pageSize-1)/pageSize);
        differencePage_=std::min(differencePage_,pages-1);
        label(c,187,189,"Parameter",12,muted,true);
        label(c,521,189,"A",12,cyan,true); label(c,639,189,"B",12,cyan,true);
        for(unsigned j=0;j<pageSize;++j) {
            const unsigned pos=differencePage_*pageSize+j;
            if(pos>=changed.size()) break;
            const unsigned i=changed[pos];
            const auto& p=params[i];
            const double y=216+31*j;
            stroke(c,187,y-12,752,y-12,line);
            label(c,187,y+7,p.label,13);
            const auto render=[&](float v) -> std::string {
                if(std::string_view(p.symbol)=="mode") return v>.5f ? "Studio":"Live";
                return p.toggle ? (v>.5f ? "On":"Off") : format(v,p.unit);
            };
            label(c,521,y+7,render(a[i]),13);
            label(c,639,y+7,render(b[i]),13);
        }
        if(pages>1) {
            button(c,{187,445,34,29},"‹",PrevDifference);
            button(c,{228,445,34,29},"›",NextDifference);
            label(c,275,464,std::to_string(differencePage_+1)+" / "+std::to_string(pages),12,muted);
        }
    }
    void draw() {
        hits_.clear();
        cairo_t* c=cairo_create(canvas_);
        cairo_scale(c,scaleX(),scaleY());
        color(c,{.055,.071,.086}); cairo_paint(c);
        label(c,21,40,"VoiceForge",29,text,true); label(c,217,38,"LV2",13,cyan,true);
        button(c,{291,16,94,34},get("mode")>.5f ? "Studio":"Live",Mode,get("mode")>.5f);
        button(c,{392,16,85,34},get("enabled")>.5f ? "Active":"Bypass",Bypass,get("enabled")<.5f);
        button(c,{496,16,39,34},"A",SideA,side_==0);
        button(c,{540,16,39,34},"B",SideB,side_==1);
        button(c,{586,16,89,34},side_==0 ? "Copy → B":"Copy → A",Copy);
        button(c,{696,16,100,34},"Presets",Presets);
        button(c,{803,16,117,34},requestedScale_>1.0 ? "Scale 150%":"Scale 100%",Scale);
        const std::array<const char*,4> tabs{"Voice processing","Restoration","Mastering","Analyze Voice"};
        for(unsigned i=0;i<tabs.size();++i)
            button(c,{20.0+227*i,63,218,36},tabs[i],SelectPage+i,page_==i);
        graph(c); meters(c);
        button(c,{536,117,137,26},"A/B differences",Differences);
        if (page_==3) drawAnalysis(c); else drawControls(c);
        if (presetOpen_) { hits_.clear(); drawPresets(c); }
        if (differencesOpen_) { hits_.clear(); drawDifferences(c); }
        cairo_destroy(c);
        cairo_surface_flush(canvas_); XFlush(display_);
    }
    void press(const XButtonEvent& e) {
        const double x=e.x/scaleX(), y=e.y/scaleY();
        const auto hit=std::find_if(hits_.rbegin(),hits_.rend(),[&](const Hit& h){return h.rect.contains(x,y);});
        if(e.button==4 || e.button==5) {
            if (e.state&ControlMask && hit!=hits_.rend() && hit->id<SelectPage && !params[hit->id].toggle) {
                touching(hit->id,true);
                set(hit->id,fromScale(hit->id,toScale(hit->id,values_[hit->id])+(e.button==4?.03:-.03)));
                touching(hit->id,false);
            } else if(!presetOpen_ && !differencesOpen_ && y>305) {
                scroll_=std::clamp(scroll_+(e.button==4?-1:1),0,maxScroll_); dirty_=true;
            }
            return;
        }
        if(hit==hits_.rend()) return;
        XSetInputFocus(display_,window,RevertToParent,CurrentTime);
        const int id=hit->id;
        if(id>=SelectPage) { if(e.button==1) action(id); return; }
        selected_=id;
        if(e.button==3 || params[id].toggle) {
            touching(id,true); set(id,e.button==3?params[id].initial:1.0f-values_[id]); touching(id,false);
        } else if(e.button==1) {
            drag_=id; origin_=x; dragWidth_=hit->rect.w-18; dragValue_=toScale(id,values_[id]); touching(id,true);
        }
        dirty_=true;
    }
    void switchSide(int next) {
        if(side_==next) return;
        compare_[side_]=values_; haveSide_[side_]=true;
        if(haveSide_[next]) apply(compare_[next]); else { compare_[next]=values_; haveSide_[next]=true; }
        side_=next;
        saveComparison();
    }
    void saveComparison() {
        if (!ids_.eventTransfer) return;
        received_=true;
        struct { LV2_Atom header; CompareState state; } message{};
        message.header={sizeof(message.state),ids_.vector};
        message.state.vector.child_type=ids_.floating;
        message.state.values[0]=static_cast<float>(side_);
        message.state.values.back()=static_cast<float>(parameterCount);
        for (unsigned s=0;s<2;++s) {
            message.state.values[1+s]=haveSide_[s] ? 1.0f : 0.0f;
            for (unsigned i=0;i<parameterCount;++i)
                if (persistent(params[i]))
                    message.state.values[3+s*compareCapacity+i]=compare_[s][i];
        }
        write_(controller_,audio_+compareInputOffset,sizeof(message),ids_.eventTransfer,&message);
    }
    void action(int id) {
        if(id>=SelectPage && id<SelectPage+4) { page_=id-SelectPage; group_=0; scroll_=0; }
        else if(id>=SelectGroup && id<SelectGroup+4) { group_=id-SelectGroup; scroll_=0; }
        else switch(id) {
            case Mode: set("mode",1.0f-get("mode")); break;
            case Bypass: set("enabled",1.0f-get("enabled")); break;
            case SideA: switchSide(0); break;
            case SideB: switchSide(1); break;
            case Copy: compare_[1-side_]=values_; haveSide_[1-side_]=true; saveComparison(); break;
            case Differences: differencesOpen_=true; differencePage_=0; break;
            case CloseDifferences: differencesOpen_=false; break;
            case PrevDifference: if(differencePage_>0) --differencePage_; break;
            case NextDifference: ++differencePage_; break;
            case Presets: presetOpen_=true; refreshPresets(); break;
            case ClosePreset: presetOpen_=false; edit_=Edit::none; status_.clear(); break;
            case PrevPreset: preset_=(preset_+names_.size()-1)%names_.size(); status_.clear(); break;
            case NextPreset: preset_=(preset_+1)%names_.size(); status_.clear(); break;
            case LoadPreset: loadPreset(); break;
            case SavePreset: edit_=Edit::create; editing_.clear(); status_.clear(); break;
            case RenamePreset: edit_=Edit::rename; editing_=names_[preset_]; status_.clear(); break;
            case DeletePreset:
                if (status_=="Click Delete again to confirm.") deletePreset();
                else status_="Click Delete again to confirm.";
                break;
            case ResetMeter: pulse("meter_reset"); break;
            case StartAnalysis: pulse("analyze"); break;
            case ResetAnalysis: pulse("analysis_reset"); beforeAnalysis_=false; break;
            case Apply: applyAnalysis(); break;
            case Undo: { auto current=values_; apply(prior_); prior_=current; break; }
            case Scale:
                requestedScale_=requestedScale_>1.0 ? 1.0 : 1.5;
                if(hostResize_) hostResize_->ui_resize(hostResize_->handle,width*requestedScale_,height*requestedScale_);
                else size(width*requestedScale_,height*requestedScale_);
                break;
        }
        dirty_=true;
    }
    void applyAnalysis() {
        prior_=values_; beforeAnalysis_=true;
        set("high_pass",get("recommend_hpf")); set("comp_enabled",1); set("comp_threshold",get("recommend_comp"));
        if(get("analysis_sibilance")>.06f) { set("de_esser_enabled",1); set("de_esser_frequency",get("recommend_ess")); }
        if(get("recommend_noise")>0 && get("mode")>.5f) { set("noise_enabled",1); set("noise_reduction",get("recommend_noise")); }
        if(get("analysis_range")>15) { set("rider_enabled",1); set("rider_target",-20); }
    }
    std::filesystem::path directory() const {
        if(const char* config=std::getenv("XDG_CONFIG_HOME"); config && *config)
            return std::filesystem::path(config)/"voiceforge"/"presets";
        if(const char* home=std::getenv("HOME"); home && *home)
            return std::filesystem::path(home)/".config"/"voiceforge"/"presets";
        return {};
    }
    void refreshPresets() {
        names_.assign(std::begin(factoryNames),std::end(factoryNames));
        const auto dir=directory();
        std::error_code error;
        for (std::filesystem::directory_iterator it(dir,error), end; it!=end && !error; it.increment(error)) {
            if (it->path().extension()==".vfpreset" && it->is_regular_file(error))
                names_.push_back(it->path().stem().string());
        }
        std::sort(names_.begin()+std::size(factoryNames),names_.end());
        preset_=std::min(preset_,names_.size()-1);
    }
    std::filesystem::path file(const std::string& name) const { return directory()/(name+".vfpreset"); }
    void loadPreset() {
        Values target=defaults();
        if (preset_<std::size(factoryNames)) std::copy_n(factory[preset_],parameterCount,target.begin());
        else {
            std::ifstream stream(file(names_[preset_]));
            stream.imbue(std::locale::classic());
            std::string magic; unsigned version;
            if (!(stream>>magic>>version) || magic!="VoiceForge" || version!=1) { status_="Cannot read this preset."; return; }
            std::string key; float data;
            while(stream>>key>>data) {
                const int i=parameter(key);
                if(i>=0 && persistent(params[i]) && std::isfinite(data)) target[i]=data;
            }
            if(!stream.eof()) { status_="Invalid preset value."; return; }
        }
        apply(target); presetOpen_=false; status_.clear();
    }
    void writePreset() {
        if(editing_.empty() || editing_.front()==' ' || editing_.back()==' ') { status_="Enter a valid name."; return; }
        const auto dir=directory();
        std::error_code ec;
        if(dir.empty()) { status_="User configuration directory is unavailable."; return; }
        std::filesystem::create_directories(dir,ec);
        if(ec) { status_="Cannot create the preset directory."; return; }
        const auto destination=file(editing_);
        if(std::filesystem::exists(destination,ec)) { status_="That preset name is already in use."; return; }
        if(edit_==Edit::rename) std::filesystem::rename(file(names_[preset_]),destination,ec);
        else {
            auto temporary=destination; temporary += ".tmp";
            {
                std::ofstream output(temporary);
                output.imbue(std::locale::classic());
                output<<"VoiceForge 1\n"<<std::setprecision(9);
                for(std::size_t i=0;i<parameterCount;++i)
                    if(persistent(params[i])) output<<params[i].symbol<<' '<<values_[i]<<'\n';
                output.close();
                if(!output) { status_="Cannot save the preset."; return; }
            }
            std::filesystem::rename(temporary,destination,ec);
        }
        if(ec) { status_="Cannot save the preset."; return; }
        refreshPresets();
        preset_=static_cast<std::size_t>(std::find(names_.begin(),names_.end(),editing_)-names_.begin());
        edit_=Edit::none; status_="Preset saved.";
    }
    void deletePreset() {
        std::error_code ec;
        if(std::filesystem::remove(file(names_[preset_]),ec)) { refreshPresets(); status_="Preset deleted."; }
        else status_="Cannot delete this preset.";
    }
    void key(XKeyEvent event) {
        KeySym code=0; char bytes[8]{};
        XLookupString(&event,bytes,sizeof(bytes),&code,nullptr);
        if(presetOpen_ && edit_!=Edit::none) {
            if(code==XK_Escape) edit_=Edit::none;
            else if(code==XK_Return) writePreset();
            else if(code==XK_BackSpace && !editing_.empty()) editing_.pop_back();
            else if(editing_.size()<28 && ((bytes[0]>='A' && bytes[0]<='Z') ||
                   (bytes[0]>='a' && bytes[0]<='z') || (bytes[0]>='0' && bytes[0]<='9') ||
                   bytes[0]==' ' || bytes[0]=='_' || bytes[0]=='-')) editing_+=bytes[0];
        } else if(differencesOpen_) {
            if(code==XK_Left) action(PrevDifference);
            if(code==XK_Right) action(NextDifference);
            if(code==XK_Escape) action(CloseDifferences);
        } else if(presetOpen_) {
            if(code==XK_Left) action(PrevPreset);
            if(code==XK_Right) action(NextPreset);
            if(code==XK_Return) action(LoadPreset);
            if(code==XK_Escape) action(ClosePreset);
        } else if(selected_>=0 && (code==XK_Up || code==XK_Down)) {
            touching(selected_,true);
            set(selected_,params[selected_].toggle ? 1.0f-values_[selected_]
                : fromScale(selected_,toScale(selected_,values_[selected_])+(code==XK_Up?.02:-.02)));
            touching(selected_,false);
        }
        dirty_=true;
    }

    Display* display_; cairo_surface_t* canvas_{};
    LV2UI_Write_Function write_; LV2UI_Controller controller_; unsigned audio_;
    const LV2UI_Touch* touch_; LV2UI_Resize* hostResize_;
    CompareUrids ids_;
    Values values_, prior_{};
    std::array<Values,2> compare_{}; std::array<bool,2> haveSide_{};
    std::vector<Hit> hits_;
    std::vector<std::string> names_;
    std::size_t preset_=0;
    std::string editing_,status_;
    Edit edit_=Edit::none;
    unsigned page_=0,group_=0,differencePage_=0;
    int pixelsW_=width,pixelsH_=height,side_=0,scroll_=0,maxScroll_=0,drag_=-1,selected_=-1,pulse_=-1;
    double requestedScale_=1.0,origin_=0,dragWidth_=1,dragValue_=0;
    bool presetOpen_=false,differencesOpen_=false,dirty_=true,beforeAnalysis_=false;
    bool received_=false;
    unsigned requests_=0;
    Clock::time_point nextRequest_{};
    Clock::time_point lastPaint_{},pulseEnd_{};
};

LV2UI_Handle instantiate(const LV2UI_Descriptor*,const char* plugin,const char*,
                         LV2UI_Write_Function write,LV2UI_Controller controller,LV2UI_Widget* widget,
                         const LV2_Feature* const* features) {
    Window parent=0; const LV2UI_Touch* touch=nullptr; LV2UI_Resize* resize=nullptr;
    const LV2_URID_Map* map=nullptr;
    for(auto fs=features;fs && *fs;++fs) {
        const auto& f=**fs;
        if(std::strcmp(f.URI,LV2_UI__parent)==0) parent=reinterpret_cast<Window>(f.data);
        else if(std::strcmp(f.URI,LV2_UI__touch)==0) touch=static_cast<const LV2UI_Touch*>(f.data);
        else if(std::strcmp(f.URI,LV2_UI__resize)==0) resize=static_cast<LV2UI_Resize*>(f.data);
        else if(std::strcmp(f.URI,LV2_URID__map)==0) map=static_cast<const LV2_URID_Map*>(f.data);
    }
    if(!parent || !widget || !write) return nullptr;
    Display* display=XOpenDisplay(nullptr);
    if(!display) return nullptr;
    try {
        const unsigned channels=std::string_view(plugin).ends_with("/stereo") ? 4u : 2u;
        auto panel=std::make_unique<Panel>(display,parent,write,controller,channels,touch,resize,map);
        *widget=reinterpret_cast<LV2UI_Widget>(panel->window);
        return panel.release();
    } catch(...) {
        XCloseDisplay(display); return nullptr;
    }
}
void cleanup(LV2UI_Handle ui) { delete static_cast<Panel*>(ui); }
void portEvent(LV2UI_Handle ui,uint32_t index,uint32_t size,uint32_t protocol,const void* data) {
    if (!data) return;
    if(protocol==0 && size==sizeof(float) && data) {
        float value; std::memcpy(&value,data,sizeof(value));
        static_cast<Panel*>(ui)->incoming(index,value);
    } else static_cast<Panel*>(ui)->incoming(index,size,protocol,data);
}
int idle(LV2UI_Handle ui) { return static_cast<Panel*>(ui)->idle(); }
int resize(LV2UI_Feature_Handle h,int w,int t) { return static_cast<Panel*>(h)->size(w,t); }
const void* extension(const char* id) {
    static const LV2UI_Idle_Interface i{idle};
    static const LV2UI_Resize r{nullptr,resize};
    if(std::strcmp(id,LV2_UI__idleInterface)==0) return &i;
    if(std::strcmp(id,LV2_UI__resize)==0) return &r;
    return nullptr;
}
const LV2UI_Descriptor descriptor{uri,instantiate,cleanup,portEvent,extension};
} // namespace
} // namespace voiceforge::ui

LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return index==0 ? &voiceforge::ui::descriptor : nullptr;
}
