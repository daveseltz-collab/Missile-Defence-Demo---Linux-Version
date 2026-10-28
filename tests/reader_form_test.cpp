// Compile with the command_control target's include/link flags, without NDEBUG.
#include "../apps/command_control/threat_qos.cpp"
#include <cassert>
#include <cstring>

namespace {
GtkWidget* control(GtkWidget* widget, const char* name)
{
    const char* accessible = atk_object_get_name(gtk_widget_get_accessible(widget));
    if ((GTK_IS_ENTRY(widget) || GTK_IS_COMBO_BOX(widget)) &&
        accessible && std::strcmp(accessible, name) == 0) return widget;
    if (!GTK_IS_CONTAINER(widget)) return nullptr;
    GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
    GtkWidget* result = nullptr;
    for (GList* item = children; item && !result; item = item->next)
        result = control(GTK_WIDGET(item->data), name);
    g_list_free(children);
    return result;
}

struct Driver { int mode; int step = 0; };
gboolean drive(gpointer data)
{
    auto& driver = *static_cast<Driver*>(data);
    GList* windows = gtk_window_list_toplevels();
    GtkWidget* dialog = nullptr;
    for (GList* item = windows; item; item = item->next) {
        auto* candidate = GTK_WIDGET(item->data);
        const char* title = gtk_window_get_title(GTK_WINDOW(candidate));
        if (GTK_IS_DIALOG(candidate) && title && std::strstr(title, "Action Reader - Reader QoS"))
            dialog = candidate;
    }
    g_list_free(windows);
    assert(dialog);
    auto entry = [&](const char* name) {
        auto* result = control(dialog, name);
        assert(result && GTK_IS_ENTRY(result));
        return GTK_ENTRY(result);
    };
    assert(!control(dialog, "Lifespan (ms)"));
    assert(!gtk_widget_is_visible(GTK_WIDGET(entry("Strength"))));
    if (driver.mode == 0) {
        gtk_entry_set_text(entry("History depth (or ALL)"), "9");
        gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
    } else if (driver.mode == 1) {
        if (driver.step++ == 0) {
            gtk_entry_set_text(entry("History depth (or ALL)"), "0");
            gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
            return G_SOURCE_CONTINUE;
        }
        if (driver.step == 2) {
            gtk_entry_set_text(entry("History depth (or ALL)"), "3");
            gtk_entry_set_text(entry("Deadline (ms)"), "10");
            gtk_entry_set_text(entry("Minimum separation (ms)"), "20");
            gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
            return G_SOURCE_CONTINUE;
        }
        gtk_entry_set_text(entry("Minimum separation (ms)"), "5.125");
        gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    } else if (driver.mode == 2) {
        assert(std::string(gtk_entry_get_text(entry("History depth (or ALL)"))) == "3");
        gtk_entry_set_text(entry("History depth (or ALL)"), "8");
        gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
    } else {
        if (driver.step++ == 0) {
            auto* profiles = control(dialog, "Apply QoS from profile");
            assert(profiles);
            gtk_combo_box_set_active(GTK_COMBO_BOX(profiles), 1);
            gtk_combo_box_set_active(GTK_COMBO_BOX(profiles), 0);
            gtk_dialog_response(GTK_DIALOG(dialog), 1);
            return G_SOURCE_CONTINUE;
        }
        assert(std::string(gtk_entry_get_text(entry("History depth (or ALL)"))) == "7");
        assert(std::string(gtk_entry_get_text(entry("Partitions (semicolon-separated; blank = default)"))) == "test");
        gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    }
    return G_SOURCE_REMOVE;
}
}

int main()
{
    assert(gtk_init_check(nullptr, nullptr));
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    auto* window = SDL_CreateWindow("Reader form test", 0, 0, 800, 600, SDL_WINDOW_HIDDEN);
    assert(window);
    auto* factory = DDSDomainParticipantFactory::get_instance();
    auto* participant = factory->create_participant(132, DDS_PARTICIPANT_QOS_DEFAULT, nullptr, DDS_STATUS_MASK_NONE);
    assert(participant);
    assert(ship::EffectorActionTypeSupport::register_type(participant,
        ship::EffectorActionTypeSupport::get_type_name()) == DDS_RETCODE_OK);
    auto* topic = participant->create_topic("ReaderFormTest",
        ship::EffectorActionTypeSupport::get_type_name(), DDS_TOPIC_QOS_DEFAULT, nullptr, DDS_STATUS_MASK_NONE);
    assert(topic);
    {
        effector_qos::ReaderSettings original;
        participant->get_default_subscriber_qos(original.subscriber);
        participant->get_default_datareader_qos(original.reader);
        auto endpoint = effector_qos::create_reader(participant, topic, nullptr, original);
        assert(endpoint);
        int sets = 0;
        std::vector<ReaderQosTarget> targets{{"Action Reader", "Test reader", topic->get_name(),
            [&](effector_qos::ReaderSettings& value, std::string&) { value = endpoint->settings; return true; },
            [&](const effector_qos::ReaderSettings& value, std::string& error) {
                ++sets;
                return effector_qos::replace_reader(endpoint, value, error);
            }}};
        effector_qos::Settings writer;
        participant->get_default_publisher_qos(writer.publisher);
        participant->get_default_datawriter_qos(writer.writer);
        writer.writer.history.depth = 7;
        writer.publisher.partition.name.ensure_length(1, 1);
        writer.publisher.partition.name[0] = DDS_String_dup("test");
        std::vector<ReaderChange> changes;
        std::string error;
        for (int mode : {0, 1, 2, 3}) {
            Driver driver{mode};
            g_timeout_add(100, drive, &driver);
            assert(configure_readers(window, participant, targets, writer, changes, error));
            assert(sets == 0);
            if (mode == 0) assert(changes.empty());
            if (mode == 1 || mode == 2) {
                assert(changes.size() == 1 && changes[0].updated.reader.history.depth == 3);
                assert(changes[0].updated.reader.time_based_filter.minimum_separation.nanosec == 5125000);
            }
        }
        assert(changes[0].updated.reader.history.depth == 7);
        assert(apply_readers(changes, false, window, error));
        DDS_DataReaderQos actual;
        assert(endpoint->reader->get_qos(actual) == DDS_RETCODE_OK);
        assert(actual.history.depth == 7);
        assert(actual.time_based_filter.minimum_separation.nanosec == 5125000);
        assert(apply_readers(changes, true, window, error));
        assert(endpoint->reader->get_qos(actual) == DDS_RETCODE_OK);
        assert(actual.history.depth == original.reader.history.depth);
    }
    participant->delete_contained_entities();
    factory->delete_participant(participant);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "PASS: native reader form, validation, cancel, staged reopen, match, DDS apply/readback/rollback\n";
}
