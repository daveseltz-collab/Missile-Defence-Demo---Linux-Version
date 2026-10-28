// Build with command_control's include/link flags and assertions enabled.
// Build again with -DTEST_EFFECTOR_SERVICE for the isolated DDS service.
#include <ndds/ndds_cpp.h>
#define create_participant(domain, ...) create_participant(132, __VA_ARGS__)
#ifdef TEST_EFFECTOR_SERVICE
#include "../apps/effector/main.cpp"
#else
#include <SDL.h>
#include <gtk/gtk.h>
#include <cassert>
#include <cstring>

static int poll_test_event(SDL_Event*);
static void present_test_frame(SDL_Renderer*);
static int run_test_dialog(GtkDialog*);

#define main demo_main
#define SDL_PollEvent poll_test_event
#define SDL_RenderPresent present_test_frame
#include "../apps/command_control/main.cpp"
#undef SDL_RenderPresent
#undef SDL_PollEvent
#undef main

#define gtk_dialog_run run_test_dialog
#include "../apps/command_control/threat_qos.cpp"
#undef gtk_dialog_run
#undef create_participant

static int completed = 0;
static int opened = 0;
static bool idle_processed = false;
static bool requested = false;
static bool in_reader = false;
static Uint32 start;

static GtkWidget* find_control(GtkWidget* widget, const char* text)
{
    const char* name = atk_object_get_name(gtk_widget_get_accessible(widget));
    if (GTK_IS_TOGGLE_BUTTON(widget) && name && std::strcmp(name, text) == 0) return widget;
    if (!GTK_IS_CONTAINER(widget)) return nullptr;
    GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
    GtkWidget* found = nullptr;
    for (GList* item = children; item && !found; item = item->next)
        found = find_control(GTK_WIDGET(item->data), text);
    g_list_free(children);
    return found;
}

static int run_test_dialog(GtkDialog* dialog)
{
    const char* title = gtk_window_get_title(GTK_WINDOW(dialog));
    assert(title);
    const bool reader = std::strstr(title, " - Reader QoS") != nullptr;
    in_reader = reader;
    ++opened;
    g_timeout_add(100, +[](gpointer data) -> gboolean {
        auto* dialog = GTK_DIALOG(data);
        if (completed == 1 && !in_reader && opened == 2) {
            gtk_dialog_response(dialog, 1);
        } else {
            auto* reliability = find_control(GTK_WIDGET(dialog), "Reliability (reliable delivery)");
            assert(reliability);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reliability),
                !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(reliability)));
            gtk_dialog_response(dialog, completed == 2 ? GTK_RESPONSE_CANCEL : GTK_RESPONSE_OK);
        }
        return G_SOURCE_REMOVE;
    }, dialog);
    const int response = gtk_dialog_run(dialog);
    if (!reader && response != 1) {
        ++completed;
        idle_processed = false;
        g_idle_add(+[](gpointer) -> gboolean {
            idle_processed = true;
            return G_SOURCE_REMOVE;
        }, nullptr);
    }
    return response;
}

static int poll_test_event(SDL_Event* event)
{
    if (!requested && SDL_GetTicks() - start > 1000 && completed < 3) {
        requested = true;
        *event = {};
        event->type = SDL_MOUSEBUTTONDOWN;
        event->button.button = SDL_BUTTON_LEFT;
        event->button.x = 488;
        event->button.y = 555;
        return 1;
    }
    return SDL_PollEvent(event);
}

static void present_test_frame(SDL_Renderer* renderer)
{
    assert(SDL_GetTicks() - start < 20000);
    if (completed > 0 && idle_processed) {
        // GTK must finish deferred work even though no native dialog is running.
        GList* windows = gtk_window_list_toplevels();
        for (GList* item = windows; item; item = item->next) {
            auto* widget = GTK_WIDGET(item->data);
            if (GTK_IS_DIALOG(widget)) assert(!gtk_widget_get_visible(widget));
        }
        g_list_free(windows);
        if (requested) {
            std::cout << "Closed dialog cycle " << completed << "; GTK idle serviced by SDL loop.\n";
            requested = false;
        }
        if (completed == 3) {
            SDL_Event quit{};
            quit.type = SDL_QUIT;
            assert(SDL_PushEvent(&quit) == 1);
        }
    }
    SDL_RenderPresent(renderer);
}

int main(int argc, char** argv)
{
    start = SDL_GetTicks();
    const int result = demo_main(argc, argv);
    assert(result == 0 && completed == 3 && opened == 5 && idle_processed);
    std::cout << "PASS: publisher OK, nested reader OK, Cancel, reopen, and deferred GTK cleanup in the real SDL loop.\n";
    return result;
}
#endif
