The goal of Crummium is to create the most lightweight version of Chromium possible.  This version has no tabs, DRM Plugins, etc.  It is based off of CEF making it compatible with Chromium but with an FLTK based interface
I used Gemini to help me get it to this point.  Adding features is is a possibility, and this may be the Lite version of the app in the future.  I cheated and used Gemini to generate the code and instructions for compilation:
Compiling Crummium for a complete newbie on Ubuntu: 

sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    libfltk1.3-dev \
    libx11-dev \
    libxcursor-dev \
    libxfixes-dev \
    libxrender-dev \
    libxext-dev \
    libxinerama-dev \
    libxi-dev \
    libxrandr-dev \
    libgl1-mesa-dev \
    libegl1-mesa-dev \
    libgles2-mesa-dev \
    curl \
    tar \
    bzip2

Verification: Run cmake --version and fltk-config --version. Both commands should execute and output their installed version numbers.

Creating the directory structure for the app in your home directory:

# Set up directories in the active user's home
mkdir -p "$HOME/cef_browser"
cd "$HOME/cef_browser"

# Download and extract CEF
curl -L -o cef.tar.bz2 "https://cef-builds.spotifycdn.com/cef_binary_156.0.15%2Bgcf671c0%2Bchromium-156.0.8078.80_macosarm64_minimal.tar.bz2"
tar -xjf cef.tar.bz2
mv cef_binary_*_linuxarm64 cef

# Compile the C++ wrapper
mkdir -p "$HOME/cef_browser/cef/build"
cd "$HOME/cef_browser/cef/build"
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc) libcef_dll_wrapper

Creating the CMakeLists.txt 
Use CMAKE_CURRENT_SOURCE_DIR so CMake automatically locates cef relative to wherever the project lives, regardless of username or directory depth:

cat << 'EOF' > "$HOME/cef_browser/CMakeLists.txt"
cmake_minimum_required(VERSION 3.16)
project(crummium C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef" CACHE PATH "Path to CEF root")

find_package(FLTK REQUIRED)
find_package(X11 REQUIRED)

include_directories(
    ${CEF_ROOT}
    ${FLTK_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)

find_library(CEF_WRAPPER_LIB
    NAMES libcef_dll_wrapper.a cef_dll_wrapper
    PATHS
        "${CEF_ROOT}/build/libcef_dll_wrapper"
        "${CEF_ROOT}/libcef_dll_wrapper"
    NO_DEFAULT_PATH
    REQUIRED
)

set(CEF_SO_LIB "${CEF_ROOT}/Release/libcef.so")

add_executable(crummium main.cpp)

target_link_libraries(crummium
    PRIVATE
        ${CEF_WRAPPER_LIB}
        ${CEF_SO_LIB}
        ${FLTK_LIBRARIES}
        ${X11_LIBRARIES}
)

set_target_properties(crummium PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

# Relative asset symlinks
add_custom_command(TARGET crummium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${CEF_ROOT}/Release/libcef.so"
        "$<TARGET_FILE_DIR:crummium>/libcef.so"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CEF_ROOT}/Resources"
        "$<TARGET_FILE_DIR:crummium>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CEF_ROOT}/Release/*.bin"
        "$<TARGET_FILE_DIR:crummium>" || true
    COMMENT "Linking CEF 150 runtime assets..."
)
EOF





Creating the main.cpp file:


cat << 'EOF' > "$HOME/cef_browser/main.cpp"
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Group.H>
#include <FL/x.H>

#include <include/cef_app.h>
#include <include/cef_client.h>
#include <include/cef_browser.h>
#include <include/cef_command_line.h>
#include <X11/Xlib.h>

#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <cstdlib>
#include <string>

const char* APP_TITLE = "Crummium: The Crummiest Browser Ever Made";

// Helper function to resolve the current user's home directory
static std::string GetUserHomeDirectory() {
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return std::string(home);
    }
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return std::string(pw->pw_dir);
    }
    return "/tmp";
}

class LockedUrlInput;
static LockedUrlInput* g_active_url_bar = nullptr;

class LowMemCefApp : public CefApp {
public:
    LowMemCefApp() = default;

    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override {
        
        command_line->AppendSwitch("disable-extensions");
        command_line->AppendSwitch("disable-background-networking");
        command_line->AppendSwitch("disable-sync");
        command_line->AppendSwitch("disable-speech-api");
        command_line->AppendSwitch("disable-pdf-extension");
        
        command_line->AppendSwitchWithValue("renderer-process-limit", "1");
        command_line->AppendSwitch("low-end-device-mode");
        command_line->AppendSwitch("enable-low-res-tiling");
        command_line->AppendSwitch("no-sandbox");

        // Software rasterization
        command_line->AppendSwitch("disable-gpu");
        command_line->AppendSwitch("disable-gpu-compositing");
        command_line->AppendSwitch("disable-gpu-rasterization");
        command_line->AppendSwitch("disable-software-rasterizer");

        command_line->AppendSwitchWithValue(
            "user-agent",
            "Mozilla/5.0 (X11; Linux aarch64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36"
        );
    }

private:
    IMPLEMENT_REFCOUNTING(LowMemCefApp);
};

class LockedUrlInput : public Fl_Input {
private:
    bool has_active_focus_;

public:
    LockedUrlInput(int X, int Y, int W, int H, const char* L = nullptr)
        : Fl_Input(X, Y, W, H, L), has_active_focus_(true) {
        g_active_url_bar = this;
    }

    bool HasActiveFocus() const { return has_active_focus_; }

    void SetActiveFocus(bool val) {
        has_active_focus_ = val;
        if (val) take_focus();
    }

    int handle(int event) override {
        switch (event) {
            case FL_PUSH:
                SetActiveFocus(true);
                return 1;
            case FL_KEYBOARD:
                if (has_active_focus_) return Fl_Input::handle(event);
                break;
        }
        return Fl_Input::handle(event);
    }
};

class BrowserClient : public CefClient,
                      public CefLifeSpanHandler,
                      public CefDisplayHandler,
                      public CefFocusHandler {
private:
    CefRefPtr<CefBrowser> browser_;
    LockedUrlInput* url_input_;

public:
    BrowserClient(LockedUrlInput* url_box) : browser_(nullptr), url_input_(url_box) {}

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefFocusHandler> GetFocusHandler() override { return this; }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override { browser_ = browser; }
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override { browser_ = nullptr; }

    bool OnSetFocus(CefRefPtr<CefBrowser> browser, FocusSource source) override {
        if (url_input_ && url_input_->HasActiveFocus()) return true;
        return false;
    }

    void OnAddressChange(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         const CefString& url) override {
        if (frame->IsMain() && url_input_) url_input_->value(url.ToString().c_str());
    }

    void OnTitleChange(CefRefPtr<CefBrowser> browser,
                       const CefString& title) override {
        if (url_input_ && url_input_->window()) {
            std::string full_title = title.ToString() + " - " + APP_TITLE;
            url_input_->window()->copy_label(full_title.c_str());
        }
    }

    CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }

    IMPLEMENT_REFCOUNTING(BrowserClient);
};

class CefViewWidget : public Fl_Group {
private:
    CefRefPtr<BrowserClient> client_;
    LockedUrlInput* url_ref_;

public:
    CefViewWidget(int X, int Y, int W, int H, LockedUrlInput* url_box)
        : Fl_Group(X, Y, W, H), client_(nullptr), url_ref_(url_box) {
        end();
    }

    void SetClient(CefRefPtr<BrowserClient> client) { client_ = client; }

    void resize(int X, int Y, int W, int H) override {
        Fl_Group::resize(X, Y, W, H);
        UpdateCefWindowBounds();
    }

    int handle(int event) override {
        if (event == FL_PUSH) {
            if (url_ref_) url_ref_->SetActiveFocus(false);
            if (client_ && client_->GetBrowser()) {
                client_->GetBrowser()->GetHost()->SetFocus(true);
            }
            return 1;
        }
        return Fl_Group::handle(event);
    }

    void InitCefBrowser(const std::string& start_url) {
        CefWindowInfo window_info;
        CefBrowserSettings browser_settings;

        Window parent_xid = fl_xid(this->window());
        CefRect bounds(x(), y(), w(), h());
        window_info.SetAsChild(parent_xid, bounds);

        CefBrowserHost::CreateBrowser(
            window_info, client_, start_url, browser_settings, nullptr, nullptr
        );
    }

    void UpdateCefWindowBounds() {
        if (!client_ || !client_->GetBrowser()) return;
        CefWindowHandle handle = client_->GetBrowser()->GetHost()->GetWindowHandle();
        if (handle && fl_display) {
            XMoveResizeWindow(fl_display, handle, x(), y(), w(), h());
            XFlush(fl_display);
        }
    }
};

static int global_keyboard_filter(int event) {
    if (event == FL_KEYDOWN || event == FL_KEYBOARD) {
        if (g_active_url_bar && g_active_url_bar->HasActiveFocus()) {
            return g_active_url_bar->handle(event);
        }
    }
    return 0;
}

static void cef_message_pump(void*) {
    CefDoMessageLoopWork();
    Fl::repeat_timeout(0.025, cef_message_pump);
}

static void back_cb(Fl_Widget*, void* data) {
    auto* client = static_cast<BrowserClient*>(data);
    if (client && client->GetBrowser() && client->GetBrowser()->CanGoBack()) client->GetBrowser()->GoBack();
}

static void forward_cb(Fl_Widget*, void* data) {
    auto* client = static_cast<BrowserClient*>(data);
    if (client && client->GetBrowser() && client->GetBrowser()->CanGoForward()) client->GetBrowser()->GoForward();
}

static void reload_cb(Fl_Widget*, void* data) {
    auto* client = static_cast<BrowserClient*>(data);
    if (client && client->GetBrowser()) client->GetBrowser()->Reload();
}

static void navigate_cb(Fl_Widget* w, void* data) {
    auto* client = static_cast<BrowserClient*>(data);
    auto* input = static_cast<LockedUrlInput*>(w);
    if (client && client->GetBrowser() && input->value()) {
        std::string target = input->value();
        if (target.find("://") == std::string::npos) target = "https://" + target;
        client->GetBrowser()->GetMainFrame()->LoadURL(target);
        input->SetActiveFocus(false);
        client->GetBrowser()->GetHost()->SetFocus(true);
    }
}

int main(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);
    CefRefPtr<LowMemCefApp> app(new LowMemCefApp());

    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) return exit_code;

    CefSettings settings;
    settings.multi_threaded_message_loop = false;
    settings.command_line_args_disabled = false;
    settings.no_sandbox = true;

    // Dynamically store cache under ~/.cache/crummium for the current user
    std::string cache_path = GetUserHomeDirectory() + "/.cache/crummium";
    CefString(&settings.root_cache_path).FromString(cache_path);

    CefInitialize(main_args, settings, app, nullptr);

    Fl_Double_Window* main_win = new Fl_Double_Window(800, 480, APP_TITLE);

    int btn_w = 36;
    int pad = 4;
    int cur_x = pad;

    Fl_Button* btn_back = new Fl_Button(cur_x, pad, btn_w, 26, "@<");
    cur_x += btn_w + pad;
    Fl_Button* btn_fwd = new Fl_Button(cur_x, pad, btn_w, 26, "@>");
    cur_x += btn_w + pad;
    Fl_Button* btn_reload = new Fl_Button(cur_x, pad, btn_w, 26, "@circle");
    cur_x += btn_w + pad;

    int input_w = 800 - cur_x - pad;
    LockedUrlInput* url_bar = new LockedUrlInput(cur_x, pad, input_w, 26);
    url_bar->when(FL_WHEN_ENTER_KEY);

    CefViewWidget* web_view = new CefViewWidget(0, 34, 800, 480 - 34, url_bar);
    main_win->resizable(web_view);
    main_win->end();

    CefRefPtr<BrowserClient> client = new BrowserClient(url_bar);
    web_view->SetClient(client);

    btn_back->callback(back_cb, client.get());
    btn_fwd->callback(forward_cb, client.get());
    btn_reload->callback(reload_cb, client.get());
    url_bar->callback(navigate_cb, client.get());

    Fl::add_handler(global_keyboard_filter);

    main_win->show();
    Fl::check();

    web_view->InitCefBrowser("https://html.duckduckgo.com");
    url_bar->SetActiveFocus(true);

    Fl::add_timeout(0.025, cef_message_pump);
    int result = Fl::run();

    CefShutdown();
    return result;
}
EOF

Compiling Crummium and running it:

mkdir -p "$HOME/cef_browser/build"
cd "$HOME/cef_browser/build"

# Compile Crummium
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Link runtime libraries relative to the build directory
ln -sf "$HOME/cef_browser/cef/Release/lib"*.so* .
ln -sf "$HOME/cef_browser/cef/Resources/"* .
ln -sf "$HOME/cef_browser/cef/Release/"*.bin . 2>/dev/null || true
ln -sf "$HOME/cef_browser/cef/Release/locales" . 2>/dev/null || true

# Launch
./crummium



