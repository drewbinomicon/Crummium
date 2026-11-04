/* CrummiumL  The Crummiest Web Browser Ever Made.  The goal is to have a very simple version of Chrome that supports the latest web features using
    FLTK to constrain how much memory the browser uses.  This version
    does not have tabs, profiles, widevine, nor any other DRM plugins.

    Copyright (C) 2026  Andrew D. Harris. <Andrew2325601@gmail.com>
    I am not an engineer or anything.  I am just some dude who is a
    hobbyist.  I couldn't have done it without Jesus Christ.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


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
#include <string>

const char* APP_TITLE = "Crummium: The Crummiest Browser Ever Made";

// Forward declaration
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
        if (val) {
            take_focus();
        }
    }

    int handle(int event) override {
        switch (event) {
            case FL_PUSH:
                SetActiveFocus(true);
                return 1;
            case FL_KEYBOARD:
                if (has_active_focus_) {
                    return Fl_Input::handle(event);
                }
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

    bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             int popup_id,
                             const CefString& target_url,
                             const CefString& target_frame_name,
                             WindowOpenDisposition target_disposition,
                             bool user_gesture,
                             const CefPopupFeatures& popupFeatures,
                             CefWindowInfo& windowInfo,
                             CefRefPtr<CefClient>& client,
                             CefBrowserSettings& settings,
                             CefRefPtr<CefDictionaryValue>& extra_info,
                             bool* no_javascript_access) override {

        // Force popups and new tabs to load in the current Crummium window
        if (browser && browser->GetMainFrame()) {
            browser->GetMainFrame()->LoadURL(target_url);
        }

        return true; // Return true to block CEF from creating a new unmanaged window
    }

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefFocusHandler> GetFocusHandler() override { return this; }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        browser_ = browser;
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        browser_ = nullptr;
    }

    // Completely prevent CEF from grabbing focus automatically on hover
    bool OnSetFocus(CefRefPtr<CefBrowser> browser, FocusSource source) override {
        if (url_input_ && url_input_->HasActiveFocus()) {
            return true; // Cancel CEF taking focus
        }
        return false;
    }

    void OnAddressChange(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         const CefString& url) override {
        if (frame->IsMain() && url_input_) {
            url_input_->value(url.ToString().c_str());
        }
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

    void SetClient(CefRefPtr<BrowserClient> client) {
        client_ = client;
    }

    void resize(int X, int Y, int W, int H) override {
        Fl_Group::resize(X, Y, W, H);
        UpdateCefWindowBounds();
    }

    int handle(int event) override {
        // Only give CEF focus when the user explicitly clicks the webpage
        if (event == FL_PUSH) {
            if (url_ref_) {
                url_ref_->SetActiveFocus(false);
            }
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
            window_info,
            client_,
            start_url,
            browser_settings,
            nullptr,
            nullptr
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

// Global FLTK filter: intercepts keyboard events before hover routing
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
    if (client && client->GetBrowser() && client->GetBrowser()->CanGoBack()) {
        client->GetBrowser()->GoBack();
    }
}

static void forward_cb(Fl_Widget*, void* data) {
    auto* client = static_cast<BrowserClient*>(data);
    if (client && client->GetBrowser() && client->GetBrowser()->CanGoForward()) {
        client->GetBrowser()->GoForward();
    }
}

static void reload_cb(Fl_Widget*, void* data) {
    auto* client = static_cast<BrowserClient*>(data);
    if (client && client->GetBrowser()) {
        client->GetBrowser()->Reload();
    }
}

static void navigate_cb(Fl_Widget* w, void* data) {
    auto* client = static_cast<BrowserClient*>(data);
    auto* input = static_cast<LockedUrlInput*>(w);
    if (client && client->GetBrowser() && input->value()) {
        std::string target = input->value();
        if (target.find("://") == std::string::npos) {
            target = "https://" + target;
        }
        client->GetBrowser()->GetMainFrame()->LoadURL(target);

        // After pressing Enter, shift focus down to the page
        input->SetActiveFocus(false);
        client->GetBrowser()->GetHost()->SetFocus(true);
    }
}

int main(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);
    CefRefPtr<LowMemCefApp> app(new LowMemCefApp());

    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    CefSettings settings;
    settings.multi_threaded_message_loop = false;
    settings.command_line_args_disabled = false;
    settings.no_sandbox = true;

    CefString(&settings.root_cache_path).FromASCII("/tmp/crummium_cache");

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

    // Register global event filter before showing
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
