#include "threat_qos.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#ifdef __linux__
#include <charconv>
#include <gtk/gtk.h>
#include <SDL_syswm.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#endif

namespace {

struct Profile {
    std::string library;
    std::string name;
};

struct ReaderChange {
    const ReaderQosTarget* target = nullptr;
    effector_qos::ReaderSettings original;
    effector_qos::ReaderSettings updated;
};

int show_choices(SDL_Window* parent, const char* title, const std::string& message,
                 const std::vector<std::string>& labels, int cancel_id = -1)
{
    std::vector<SDL_MessageBoxButtonData> buttons;
    buttons.reserve(labels.size());
    for (size_t i = 0; i < labels.size(); ++i) {
        Uint32 flags = 0;
        if (static_cast<int>(i) == cancel_id) flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
        buttons.push_back({flags, static_cast<int>(i), labels[i].c_str()});
    }

    SDL_MessageBoxData data{};
    data.flags = SDL_MESSAGEBOX_INFORMATION;
    data.window = parent;
    data.title = title;
    data.message = message.c_str();
    data.numbuttons = static_cast<int>(buttons.size());
    data.buttons = buttons.data();
    int button = -1;
    if (SDL_ShowMessageBox(&data, &button) != 0) {
        std::cerr << "Cannot show QoS dialog: " << SDL_GetError() << '\n';
        return -1;
    }
    return button;
}

void show_error(SDL_Window* parent, const std::string& message)
{
    std::cerr << "Endpoint QoS: " << message << '\n';
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Endpoint QoS", message.c_str(), parent);
}

bool list_profiles(std::vector<Profile>& profiles, std::string& error)
{
    auto* factory = DDSDomainParticipantFactory::get_instance();
    DDS_StringSeq libraries;
    DDS_ReturnCode_t ret = factory->get_qos_profile_libraries(libraries);
    for (DDS_Long i = 0; ret == DDS_RETCODE_OK && i < libraries.length(); ++i) {
        DDS_StringSeq names;
        ret = factory->get_qos_profiles(names, libraries[i]);
        for (DDS_Long j = 0; ret == DDS_RETCODE_OK && j < names.length(); ++j)
            profiles.push_back({libraries[i], names[j]});
    }
    if (ret != DDS_RETCODE_OK) {
        error = "Cannot enumerate QoS profiles (DDS code " + std::to_string(ret) + ").";
        return false;
    }
    return true;
}

int select_profile(SDL_Window* parent, const std::vector<Profile>& profiles, bool include_default)
{
    std::vector<std::string> labels;
    if (include_default) labels.emplace_back("DDS defaults");
    const size_t count = std::min<size_t>(profiles.size(), 6);
    for (size_t i = 0; i < count; ++i)
        labels.push_back(profiles[i].library + "::" + profiles[i].name);
    labels.emplace_back("Cancel");

    std::ostringstream message;
    message << "Select a QoS profile";
    if (profiles.size() > count)
        message << " (showing the first " << count << " of " << profiles.size() << ")";
    const int cancel_id = static_cast<int>(labels.size()) - 1;
    const int choice = show_choices(parent, "Endpoint QoS", message.str(), labels, cancel_id);
    if (choice < 0 || choice == cancel_id) return -1;
    if (include_default && choice == 0) return 0;
    return choice - (include_default ? 0 : -1);
}

#ifndef __linux__
int select_profile_with_current(SDL_Window* parent, const std::vector<Profile>& profiles)
{
    std::vector<std::string> labels{"Keep current", "DDS defaults"};
    const size_t count = std::min<size_t>(profiles.size(), 5);
    for (size_t i = 0; i < count; ++i)
        labels.push_back(profiles[i].library + "::" + profiles[i].name);
    labels.emplace_back("Cancel");
    const int cancel_id = static_cast<int>(labels.size()) - 1;
    const int choice = show_choices(parent, "Endpoint QoS", "Select the QoS basis for this endpoint.", labels, cancel_id);
    if (choice < 0 || choice == cancel_id) return -1;
    if (choice == 0) return -2;
    return choice - 1;
}
#endif

template<typename WriterQos>
bool edit_writer_fields(SDL_Window* parent, WriterQos& qos)
{
    while (true) {
        const char* reliability = qos.reliability.kind == DDS_RELIABLE_RELIABILITY_QOS
            ? "Reliable" : "Best effort";
        const int durability = static_cast<int>(qos.durability.kind);
        const char* durability_name[] = {"Volatile", "Transient local", "Transient", "Persistent"};
        const char* durability_text = durability >= 0 && durability < 4
            ? durability_name[durability] : "Unknown";
        const std::string history = qos.history.kind == DDS_KEEP_ALL_HISTORY_QOS
            ? "Keep all" : "Keep last, depth " + std::to_string(qos.history.depth);
        const std::vector<std::string> fields = {
            std::string("Reliability: ") + reliability,
            std::string("Durability: ") + durability_text,
            "History: " + history,
            "Deadline: " + (qos.deadline.period.sec == DDS_DURATION_INFINITE_SEC
                ? std::string("Infinite") : std::to_string(qos.deadline.period.sec) + " sec"),
            "Apply",
            "Cancel"
        };
        const int choice = show_choices(parent, "Endpoint QoS", "Choose a policy to edit or apply changes.", fields, 5);
        if (choice == 4) return true;
        if (choice == 5 || choice < 0) return false;

        if (choice == 0) {
            const int selected = show_choices(parent, "Reliability", "Select writer reliability.",
                {"Best effort", "Reliable", "Back"}, 2);
            if (selected == 0) qos.reliability.kind = DDS_BEST_EFFORT_RELIABILITY_QOS;
            if (selected == 1) qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
        } else if (choice == 1) {
            const int selected = show_choices(parent, "Durability", "Select writer durability.",
                {"Volatile", "Transient local", "Transient", "Persistent", "Back"}, 4);
            if (selected >= 0 && selected < 4)
                qos.durability.kind = static_cast<DDS_DurabilityQosPolicyKind>(selected);
        } else if (choice == 2) {
            const int selected = show_choices(parent, "History", "Select writer history.",
                {"Keep last, depth 1", "Keep last, depth 5", "Keep last, depth 32", "Keep all", "Back"}, 4);
            if (selected >= 0 && selected < 3) {
                qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
                qos.history.depth = selected == 0 ? 1 : (selected == 1 ? 5 : 32);
            } else if (selected == 3) {
                qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
            }
        } else if (choice == 3) {
            const int selected = show_choices(parent, "Deadline", "Select the maximum interval between samples.",
                {"Infinite", "1 second", "100 milliseconds", "Back"}, 3);
            if (selected == 0) qos.deadline.period = DDS_DURATION_INFINITE;
            if (selected == 1) qos.deadline.period = {1, 0};
            if (selected == 2) qos.deadline.period = {0, 100000000};
        }
    }
}

bool edit_reader_fields(SDL_Window* parent, effector_qos::ReaderSettings& settings)
{
    return edit_writer_fields(parent, settings.reader);
}

template<typename TargetQos, typename SourceQos>
void copy_shared_qos(TargetQos& target, const SourceQos& source)
{
    target.ownership.kind = source.ownership.kind;
    target.durability.kind = source.durability.kind;
    target.liveliness.kind = source.liveliness.kind;
    target.liveliness.lease_duration = source.liveliness.lease_duration;
    target.history.kind = source.history.kind;
    target.history.depth = source.history.depth;
    target.reliability.kind = source.reliability.kind;
    target.deadline.period = source.deadline.period;
}

bool load_writer_profile(DDSDomainParticipant* participant, DDSTopic* topic,
                         const Profile& profile, effector_qos::Settings& settings,
                         std::string& error)
{
    auto* factory = DDSDomainParticipantFactory::get_instance();
    auto ret = factory->get_publisher_qos_from_profile(settings.publisher,
        profile.library.c_str(), profile.name.c_str());
    if (ret == DDS_RETCODE_OK)
        ret = factory->get_datawriter_qos_from_profile_w_topic_name(settings.writer,
            profile.library.c_str(), profile.name.c_str(), topic->get_name());
    if (ret != DDS_RETCODE_OK) {
        error = "Cannot load profile " + profile.library + "::" + profile.name +
                " (DDS code " + std::to_string(ret) + ").";
        return false;
    }
    settings.library = profile.library;
    settings.profile = profile.name;
    return true;
}

bool load_reader_profile(DDSDomainParticipant* participant, const ReaderQosTarget& target,
                         const Profile& profile, effector_qos::ReaderSettings& settings,
                         std::string& error)
{
    auto* factory = DDSDomainParticipantFactory::get_instance();
    auto ret = factory->get_subscriber_qos_from_profile(settings.subscriber,
        profile.library.c_str(), profile.name.c_str());
    if (ret == DDS_RETCODE_OK)
        ret = factory->get_datareader_qos_from_profile_w_topic_name(settings.reader,
            profile.library.c_str(), profile.name.c_str(), target.topic_name.c_str());
    if (ret != DDS_RETCODE_OK) {
        error = "Cannot load reader profile " + profile.library + "::" + profile.name +
                " (DDS code " + std::to_string(ret) + ").";
        return false;
    }
    settings.library = profile.library;
    settings.profile = profile.name;
    return true;
}

#ifdef __linux__
bool configure_readers_form(SDL_Window* parent, GtkWindow* owner, DDSDomainParticipant* participant,
    const std::vector<ReaderQosTarget>& targets, const effector_qos::Settings& writer_settings,
    std::vector<ReaderChange>& changes, std::string& error);
#endif

bool configure_readers(SDL_Window* parent, DDSDomainParticipant* participant,
                       const std::vector<ReaderQosTarget>& targets,
                       const effector_qos::Settings& writer_settings,
                       std::vector<ReaderChange>& changes, std::string& error)
{
#ifdef __linux__
    return configure_readers_form(parent, nullptr, participant, targets, writer_settings, changes, error);
#else
    for (const auto& target : targets) {
        effector_qos::ReaderSettings current;
        if (!target.get || !target.get(current, error)) {
            if (error.empty()) error = "Cannot read QoS for " + target.name + ".";
            return false;
        }
        ReaderChange change{&target, current, current};
        copy_shared_qos(change.updated.reader, writer_settings.writer);
        change.updated.subscriber.partition = writer_settings.publisher.partition;

        std::string message = target.scope + "\n\nReader " + target.name;
        const int selected = show_choices(parent, "Reader QoS", message,
            {"Match publisher policies", "Edit reader policies", "Load QoS profile", "Leave unchanged"}, 3);
        if (selected == 3 || selected < 0) continue;
        if (selected == 1 && !edit_reader_fields(parent, change.updated)) continue;
        if (selected == 2) {
            std::vector<Profile> profiles;
            if (!list_profiles(profiles, error)) return false;
            const int profile_choice = select_profile(parent, profiles, true);
            if (profile_choice < 0) continue;
            if (profile_choice == 0) {
                if (participant->get_default_subscriber_qos(change.updated.subscriber) != DDS_RETCODE_OK ||
                    participant->get_default_datareader_qos(change.updated.reader) != DDS_RETCODE_OK) {
                    error = "Cannot read default reader QoS.";
                    return false;
                }
                change.updated.library.clear();
                change.updated.profile.clear();
            } else if (!load_reader_profile(participant, target, profiles[profile_choice - 1],
                                            change.updated, error)) {
                return false;
            }
            if (!edit_reader_fields(parent, change.updated)) continue;
        }
        changes.push_back(std::move(change));
    }
    return true;
#endif
}

bool apply_readers(const std::vector<ReaderChange>& changes, bool rollback,
                   SDL_Window* parent, std::string& error)
{
    for (const auto& change : changes) {
        if (!change.target->set ||
            !change.target->set(rollback ? change.original : change.updated, error)) {
            if (error.empty()) error = "Cannot update reader QoS for " + change.target->name + ".";
            show_error(parent, error);
            return false;
        }
    }
    return true;
}

std::unique_ptr<ThreatPublisher> create_threat_publisher(
    DDSDomainParticipant* participant, DDSTopic* topic, const effector_qos::Settings& settings,
    std::string& error)
{
    auto endpoint = std::make_unique<ThreatPublisher>();
    endpoint->participant = participant;
    endpoint->publisher = participant->create_publisher(settings.publisher, nullptr, DDS_STATUS_MASK_NONE);
    if (!endpoint->publisher) {
        error = "Cannot create a threat publisher with the selected QoS.";
        return nullptr;
    }
    auto writer_qos = settings.writer;
    writer_qos.protocol.rtps_object_id = DDS_RTPS_AUTO_ID;
    const DDS_GUID_t automatic_guid = {{0}};
    writer_qos.protocol.virtual_guid = automatic_guid;
    endpoint->writer = ship::ThreatDataWriter::narrow(endpoint->publisher->create_datawriter(
        topic, writer_qos, nullptr, DDS_STATUS_MASK_NONE));
    if (!endpoint->writer) {
        error = "Cannot create a threat writer with the selected QoS.";
        return nullptr;
    }
    return endpoint;
}

bool edit_publisher(SDL_Window* parent, DDSDomainParticipant* participant, DDSTopic* topic,
                    effector_qos::Settings& settings, std::string& error)
{
    std::vector<Profile> profiles;
    if (!list_profiles(profiles, error)) return false;
    const int profile_choice = select_profile(parent, profiles, true);
    if (profile_choice < 0) return false;
    if (profile_choice == 0) {
        if (participant->get_default_publisher_qos(settings.publisher) != DDS_RETCODE_OK ||
            participant->get_default_datawriter_qos(settings.writer) != DDS_RETCODE_OK) {
            error = "Cannot read DDS default writer QoS.";
            return false;
        }
        settings.library.clear();
        settings.profile.clear();
    } else {
        const size_t index = static_cast<size_t>(profile_choice - 1);
        if (index >= profiles.size() || !load_writer_profile(participant, topic, profiles[index], settings, error))
            return false;
    }
    return edit_writer_fields(parent, settings.writer);
}

#ifdef __linux__
std::string trim(const std::string& value)
{
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    return value.substr(start, value.find_last_not_of(" \t\r\n") - start + 1);
}

bool parse_integer(const std::string& text, DDS_Long& value)
{
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

std::string duration_text(const DDS_Duration_t& value)
{
    if (value.sec == DDS_DURATION_INFINITE_SEC) return "INF";
    const auto milliseconds = int64_t(value.sec) * 1000 + value.nanosec / 1000000;
    std::string result = std::to_string(milliseconds);
    if (value.nanosec % 1000000 != 0) {
        std::string fraction = std::to_string(1000000 + value.nanosec % 1000000).substr(1);
        while (fraction.back() == '0') fraction.pop_back();
        result += "." + fraction;
    }
    return result;
}

bool parse_duration(const std::string& input, DDS_Duration_t& value)
{
    const std::string text = trim(input);
    if (text == "INF" || text == "inf") {
        value = DDS_DURATION_INFINITE;
        return true;
    }
    const auto dot = text.find('.');
    const std::string whole = text.substr(0, dot);
    if (whole.empty() || whole.find_first_not_of("0123456789") != std::string::npos) return false;
    int64_t milliseconds = 0;
    const auto parsed = std::from_chars(whole.data(), whole.data() + whole.size(), milliseconds);
    const int64_t max_seconds = DDS_DURATION_INFINITE_SEC - 1;
    if (parsed.ec != std::errc{} || milliseconds / 1000 > max_seconds) return false;
    DDS_UnsignedLong fraction = 0;
    if (dot != std::string::npos) {
        std::string digits = text.substr(dot + 1);
        if (digits.empty() || digits.size() > 6 ||
            digits.find_first_not_of("0123456789") != std::string::npos) return false;
        digits.append(6 - digits.size(), '0');
        std::from_chars(digits.data(), digits.data() + digits.size(), fraction);
    }
    value.sec = static_cast<DDS_Long>(milliseconds / 1000);
    value.nanosec = static_cast<DDS_UnsignedLong>(milliseconds % 1000) * 1000000 + fraction;
    return true;
}

struct EffectorForm {
    GtkWidget* dialog;
    GtkWidget* profile;
    GtkWidget* partitions;
    GtkWidget* exclusive;
    GtkWidget* strength;
    GtkWidget* durability;
    GtkWidget* liveliness;
    GtkWidget* lease;
    GtkWidget* history;
    GtkWidget* reliable;
    GtkWidget* deadline;
    GtkWidget* lifespan;
    GtkWidget* error_label;
    GtkCssProvider* css;
    effector_qos::Settings original;
    effector_qos::Settings basis;
    DDSDomainParticipant* participant;
    DDSTopic* topic;
    const std::vector<Profile>& profiles;
    int selected_profile = 0;
    std::function<bool(int, effector_qos::Settings&, std::string&)> load_basis;

    static GtkWidget* entry(const char* name)
    {
        auto* widget = gtk_entry_new();
        gtk_entry_set_activates_default(GTK_ENTRY(widget), TRUE);
        atk_object_set_name(gtk_widget_get_accessible(widget), name);
        return widget;
    }

    static GtkWidget* combo(std::initializer_list<const char*> choices)
    {
        auto* widget = gtk_combo_box_text_new();
        for (auto* choice : choices) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), choice);
        return widget;
    }

    static GtkWidget* labelled(const char* text, GtkWidget* control)
    {
        auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        auto* label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), control);
        atk_object_set_name(gtk_widget_get_accessible(control), text);
        gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), control, FALSE, FALSE, 0);
        return box;
    }

    EffectorForm(SDL_Window* parent, const char* name, DDSDomainParticipant* participant,
                 DDSTopic* topic, const effector_qos::Settings& current,
                 const std::vector<Profile>& profiles, bool has_readers, bool reader_mode = false)
        : original(current), basis(current), participant(participant), topic(topic), profiles(profiles)
    {
        dialog = gtk_dialog_new();
        const auto title = std::string(name) + (reader_mode ? " - Reader QoS" : " - Publisher QoS");
        gtk_window_set_title(GTK_WINDOW(dialog), title.c_str());
        gtk_window_set_default_size(GTK_WINDOW(dialog), 650, 640);
        gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);
        gtk_style_context_add_class(gtk_widget_get_style_context(dialog), "effector-qos");
        css = gtk_css_provider_new();
        GError* css_error = nullptr;
        const char* style =
            ".effector-qos { background: #f0f0f0; color: #111; font: 12px Sans; }"
            ".effector-qos label { color: #111; }"
            ".effector-qos entry, .effector-qos combobox button {"
            " background: #fff; color: #111; min-height: 20px; padding: 2px 6px; }"
            ".effector-qos entry:disabled { background: #eee; color: #777; }"
            ".effector-qos button { min-height: 24px; padding: 2px 10px; }"
            ".effector-qos .error { color: #b00020; }";
        if (!gtk_css_provider_load_from_data(css, style, -1, &css_error)) {
            std::cerr << "QoS form style: " << css_error->message << '\n';
            g_error_free(css_error);
        }
        gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(dialog),
            GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        auto* grid = gtk_grid_new();
        gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
        gtk_grid_set_row_spacing(GTK_GRID(grid), 16);
        gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
        gtk_container_set_border_width(GTK_CONTAINER(grid), 8);
        gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, TRUE, TRUE, 0);
        auto attach = [grid](GtkWidget* widget, int x, int y, int width = 1) {
            gtk_widget_set_hexpand(widget, TRUE);
            gtk_widget_set_valign(widget, GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(grid), widget, x, y, width, 1);
        };
        profile = combo({reader_mode ? "Current reader QoS" : "Current effector QoS", "DDS defaults"});
        for (const auto& item : profiles) {
            const auto label = item.library + "::" + item.name;
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(profile), label.c_str());
        }
        // Long library/profile names must not enlarge the entire dialog.
        GList* cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(profile));
        if (cells) g_object_set(cells->data, "ellipsize", PANGO_ELLIPSIZE_END, "width-chars", 45, nullptr);
        g_list_free(cells);
        gtk_combo_box_set_active(GTK_COMBO_BOX(profile), 0);
        attach(labelled("Apply QoS from profile", profile), 0, 0, 2);
        partitions = entry("Partitions");
        attach(labelled("Partitions (semicolon-separated; blank = default)", partitions), 0, 1, 2);

        auto* ownership = gtk_frame_new("Ownership");
        auto* ownership_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        gtk_container_set_border_width(GTK_CONTAINER(ownership_box), 12);
        exclusive = gtk_check_button_new_with_label("Exclusive");
        gtk_box_pack_start(GTK_BOX(ownership_box), exclusive, FALSE, FALSE, 0);
        auto* strength_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_box_pack_start(GTK_BOX(strength_row), gtk_label_new("Strength"), FALSE, FALSE, 0);
        strength = entry("Strength");
        gtk_entry_set_width_chars(GTK_ENTRY(strength), 12);
        gtk_box_pack_end(GTK_BOX(strength_row), strength, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(ownership_box), strength_row, FALSE, FALSE, 0);
        if (reader_mode) {
            gtk_widget_set_no_show_all(strength_row, TRUE);
            gtk_widget_hide(strength_row);
        }
        gtk_container_add(GTK_CONTAINER(ownership), ownership_box);
        attach(ownership, 0, 2);

        auto* live_frame = gtk_frame_new("Liveliness");
        auto* live_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_container_set_border_width(GTK_CONTAINER(live_box), 12);
        liveliness = combo({"AUTOMATIC", "MANUAL_BY_PARTICIPANT", "MANUAL_BY_TOPIC"});
        atk_object_set_name(gtk_widget_get_accessible(liveliness), "Liveliness");
        gtk_box_pack_start(GTK_BOX(live_box), liveliness, FALSE, FALSE, 0);
        lease = entry("Lease duration (ms)");
        gtk_box_pack_start(GTK_BOX(live_box), labelled("Lease duration (ms)", lease), FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(live_frame), live_box);
        attach(live_frame, 1, 2);

        durability = combo({"VOLATILE", "TRANSIENT_LOCAL", "TRANSIENT", "PERSISTENT"});
        attach(labelled("Durability", durability), 0, 3);
        history = entry("History depth (or ALL)");
        attach(labelled("History depth (or ALL)", history), 1, 3);
        reliable = gtk_check_button_new_with_label("Reliability (reliable delivery)");
        attach(reliable, 0, 4);
        deadline = entry("Deadline (ms)");
        const char* duration_label = reader_mode ? "Minimum separation (ms)" : "Lifespan (ms)";
        lifespan = entry(duration_label);
        attach(labelled("Deadline (ms)", deadline), 0, 5);
        attach(labelled(duration_label, lifespan), 1, 5);

        auto* help = gtk_label_new(reader_mode
            ? "Durations: nonnegative milliseconds or INF. Minimum separation must not exceed the deadline. "
              "Reader changes are staged until the parent dialog is accepted."
            : "Durations: nonnegative milliseconds or INF. QoS/partition mismatches can prevent delivery. "
              "These settings affect only the selected publisher.");
        gtk_label_set_line_wrap(GTK_LABEL(help), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(help), 85);
        gtk_label_set_xalign(GTK_LABEL(help), 0);
        attach(help, 0, 6, 2);
        error_label = gtk_label_new("");
        gtk_label_set_line_wrap(GTK_LABEL(error_label), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(error_label), 85);
        gtk_label_set_xalign(GTK_LABEL(error_label), 0);
        gtk_style_context_add_class(gtk_widget_get_style_context(error_label), "error");
        attach(error_label, 0, 7, 2);

        auto* reader_button = gtk_button_new_with_label(reader_mode ? "Match publisher policies" : "Action Reader QoS...");
        gtk_widget_set_sensitive(reader_button, reader_mode || has_readers);
        attach(reader_button, 0, 8);
        g_signal_connect(reader_button, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            gtk_dialog_response(GTK_DIALOG(data), 1);
        }), dialog);
        auto* ok = gtk_dialog_add_button(GTK_DIALOG(dialog), "OK", GTK_RESPONSE_OK);
        auto* cancel = gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
        gtk_widget_set_size_request(ok, 96, 30);
        gtk_widget_set_size_request(cancel, 96, 30);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
        populate();
        g_signal_connect(exclusive, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
            gtk_widget_set_sensitive(GTK_WIDGET(data), gtk_toggle_button_get_active(button));
        }), strength);
        g_signal_connect(profile, "changed", G_CALLBACK(+[](GtkComboBox*, gpointer data) {
            static_cast<EffectorForm*>(data)->change_profile();
        }), this);

#if defined(GDK_WINDOWING_X11) && defined(SDL_VIDEO_DRIVER_X11)
        SDL_SysWMinfo info{};
        SDL_VERSION(&info.version);
        if (SDL_GetWindowWMInfo(parent, &info) && info.subsystem == SDL_SYSWM_X11 &&
            GDK_IS_X11_DISPLAY(gtk_widget_get_display(dialog))) {
            gtk_widget_realize(dialog);
            auto* owner = gdk_x11_window_foreign_new_for_display(gtk_widget_get_display(dialog), info.info.x11.window);
            if (owner) {
                gdk_window_set_transient_for(gtk_widget_get_window(dialog), owner);
                g_object_unref(owner);
            }
        }
#endif
        gtk_widget_show_all(dialog);
    }

    ~EffectorForm()
    {
        auto* display = gtk_widget_get_display(dialog);
        gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(dialog), GTK_STYLE_PROVIDER(css));
        g_object_unref(css);
        gtk_widget_destroy(dialog);
        // SDL does not flush GTK's separate Wayland/X11 connection.
        gdk_display_flush(display);
    }

    void error(const std::string& message)
    {
        gtk_label_set_text(GTK_LABEL(error_label), message.c_str());
        if (!message.empty()) std::cerr << "Endpoint QoS: " << message << '\n';
    }

    void populate()
    {
        std::string names;
        for (DDS_Long i = 0; i < basis.publisher.partition.name.length(); ++i) {
            if (i) names += ';';
            names += basis.publisher.partition.name[i];
        }
        gtk_entry_set_text(GTK_ENTRY(partitions), names.c_str());
        const auto& qos = basis.writer;
        const bool owned = qos.ownership.kind == DDS_EXCLUSIVE_OWNERSHIP_QOS;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(exclusive), owned);
        gtk_widget_set_sensitive(strength, owned);
        gtk_entry_set_text(GTK_ENTRY(strength), std::to_string(qos.ownership_strength.value).c_str());
        gtk_combo_box_set_active(GTK_COMBO_BOX(durability), static_cast<int>(qos.durability.kind));
        gtk_combo_box_set_active(GTK_COMBO_BOX(liveliness), static_cast<int>(qos.liveliness.kind));
        gtk_entry_set_text(GTK_ENTRY(lease), duration_text(qos.liveliness.lease_duration).c_str());
        gtk_entry_set_text(GTK_ENTRY(history), qos.history.kind == DDS_KEEP_ALL_HISTORY_QOS
            ? "ALL" : std::to_string(qos.history.depth).c_str());
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reliable), qos.reliability.kind == DDS_RELIABLE_RELIABILITY_QOS);
        gtk_entry_set_text(GTK_ENTRY(deadline), duration_text(qos.deadline.period).c_str());
        gtk_entry_set_text(GTK_ENTRY(lifespan), duration_text(qos.lifespan.duration).c_str());
    }

    void change_profile()
    {
        const int choice = gtk_combo_box_get_active(GTK_COMBO_BOX(profile));
        if (choice == selected_profile) return;
        auto candidate = original;
        std::string message;
        bool loaded = true;
        if (load_basis) {
            loaded = load_basis(choice, candidate, message);
        } else if (choice == 1) {
            loaded = participant->get_default_publisher_qos(candidate.publisher) == DDS_RETCODE_OK &&
                     participant->get_default_datawriter_qos(candidate.writer) == DDS_RETCODE_OK;
            candidate.library.clear();
            candidate.profile.clear();
            if (!loaded) message = "Cannot read default writer QoS.";
        } else if (choice >= 2 && static_cast<size_t>(choice - 2) < profiles.size()) {
            loaded = load_writer_profile(participant, topic, profiles[choice - 2], candidate, message);
        } else if (choice != 0) {
            loaded = false;
            message = "Invalid QoS profile selection.";
        }
        if (!loaded) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(profile), selected_profile);
            error(message);
            return;
        }
        selected_profile = choice;
        basis = candidate;
        populate();
        error("");
    }

    bool read(effector_qos::Settings& settings)
    {
        auto candidate = basis;
        auto& qos = candidate.writer;
        qos.ownership.kind = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(exclusive))
            ? DDS_EXCLUSIVE_OWNERSHIP_QOS : DDS_SHARED_OWNERSHIP_QOS;
        if (!parse_integer(trim(gtk_entry_get_text(GTK_ENTRY(strength))), qos.ownership_strength.value)) {
            error("Strength must be a signed 32-bit integer.");
            return false;
        }
        const int durability_index = gtk_combo_box_get_active(GTK_COMBO_BOX(durability));
        const int liveliness_index = gtk_combo_box_get_active(GTK_COMBO_BOX(liveliness));
        if (durability_index < 0 || durability_index > 3 || liveliness_index < 0 || liveliness_index > 2) {
            error("Select a durability and liveliness policy.");
            return false;
        }
        qos.durability.kind = static_cast<DDS_DurabilityQosPolicyKind>(durability_index);
        qos.liveliness.kind = static_cast<DDS_LivelinessQosPolicyKind>(liveliness_index);
        const auto depth = trim(gtk_entry_get_text(GTK_ENTRY(history)));
        if (depth == "ALL" || depth == "all") qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
        else if (parse_integer(depth, qos.history.depth) && qos.history.depth > 0)
            qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
        else {
            error("History depth must be a positive 32-bit integer or ALL.");
            return false;
        }
        qos.reliability.kind = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(reliable))
            ? DDS_RELIABLE_RELIABILITY_QOS : DDS_BEST_EFFORT_RELIABILITY_QOS;
        if (!parse_duration(gtk_entry_get_text(GTK_ENTRY(lease)), qos.liveliness.lease_duration) ||
            !parse_duration(gtk_entry_get_text(GTK_ENTRY(deadline)), qos.deadline.period) ||
            !parse_duration(gtk_entry_get_text(GTK_ENTRY(lifespan)), qos.lifespan.duration)) {
            error("Durations must be nonnegative milliseconds (up to 6 decimal places) or INF.");
            return false;
        }
        const auto partition_text = trim(gtk_entry_get_text(GTK_ENTRY(partitions)));
        std::vector<std::string> names;
        if (!partition_text.empty()) {
            size_t start = 0;
            do {
                const auto end = partition_text.find(';', start);
                names.push_back(trim(partition_text.substr(start, end - start)));
                if (end == std::string::npos) break;
                start = end + 1;
            } while (start <= partition_text.size());
        }
        if (names.size() > 256 ||
            !candidate.publisher.partition.name.ensure_length(static_cast<DDS_Long>(names.size()),
                                                               static_cast<DDS_Long>(names.size()))) {
            error("Cannot allocate partitions (maximum 256).");
            return false;
        }
        for (size_t i = 0; i < names.size(); ++i) {
            if (names[i].size() > 2048) {
                error("Each partition name must be at most 2048 bytes.");
                return false;
            }
            DDS_String_free(candidate.publisher.partition.name[static_cast<DDS_Long>(i)]);
            auto*& name = candidate.publisher.partition.name[static_cast<DDS_Long>(i)];
            name = DDS_String_dup(names[i].c_str());
            if (!name) { error("Cannot allocate partition name."); return false; }
        }
        settings = candidate;
        error("");
        return true;
    }
};

// Reuse the common policy widgets without replacing reader-only or unedited QoS.
effector_qos::Settings reader_fields(const effector_qos::ReaderSettings& reader)
{
    effector_qos::Settings fields;
    fields.publisher.partition = reader.subscriber.partition;
    copy_shared_qos(fields.writer, reader.reader);
    fields.writer.lifespan.duration = reader.reader.time_based_filter.minimum_separation;
    fields.library = reader.library;
    fields.profile = reader.profile;
    return fields;
}

bool configure_readers_form(SDL_Window* parent, GtkWindow* owner, DDSDomainParticipant* participant,
    const std::vector<ReaderQosTarget>& targets, const effector_qos::Settings& writer_settings,
    std::vector<ReaderChange>& changes, std::string& error)
{
    if (!gtk_init_check(nullptr, nullptr)) {
        error = "Cannot initialize the GTK reader QoS form. Check the desktop display connection.";
        return false;
    }
    std::vector<Profile> profiles;
    if (!list_profiles(profiles, error)) return false;
    auto pending = changes;
    for (const auto& target : targets) {
        auto existing = std::find_if(pending.begin(), pending.end(),
            [&target](const ReaderChange& change) { return change.target == &target; });
        effector_qos::ReaderSettings current;
        if (existing != pending.end()) current = existing->updated;
        else if (!target.get || !target.get(current, error)) {
            if (error.empty()) error = "Cannot read QoS for " + target.name + ".";
            return false;
        }
        auto basis = current;
        EffectorForm form(parent, target.name.c_str(), participant, nullptr,
                          reader_fields(current), profiles, false, true);
        if (owner) gtk_window_set_transient_for(GTK_WINDOW(form.dialog), owner);
        gtk_widget_set_tooltip_text(form.dialog, target.scope.c_str());
        form.load_basis = [&](int choice, effector_qos::Settings& fields, std::string& message) {
            auto candidate = current;
            if (choice == 1) {
                if (participant->get_default_subscriber_qos(candidate.subscriber) != DDS_RETCODE_OK ||
                    participant->get_default_datareader_qos(candidate.reader) != DDS_RETCODE_OK) {
                    message = "Cannot read default reader QoS.";
                    return false;
                }
                candidate.library.clear();
                candidate.profile.clear();
            } else if (choice >= 2 && static_cast<size_t>(choice - 2) < profiles.size()) {
                if (!load_reader_profile(participant, target, profiles[choice - 2], candidate, message))
                    return false;
            } else if (choice != 0) {
                message = "Invalid reader profile selection.";
                return false;
            }
            basis = candidate;
            fields = reader_fields(basis);
            return true;
        };
        while (true) {
            const int response = gtk_dialog_run(GTK_DIALOG(form.dialog));
            if (response != GTK_RESPONSE_OK && response != 1) return true;
            if (response == 1) {
                auto matched = reader_fields(basis);
                copy_shared_qos(matched.writer, writer_settings.writer);
                matched.publisher.partition = writer_settings.publisher.partition;
                form.basis = matched;
                form.populate();
                form.error("");
                continue;
            }
            effector_qos::Settings fields;
            if (!form.read(fields)) continue;
            auto updated = basis;
            copy_shared_qos(updated.reader, fields.writer);
            updated.subscriber.partition = fields.publisher.partition;
            updated.reader.time_based_filter.minimum_separation = fields.writer.lifespan.duration;
            const auto& separation = updated.reader.time_based_filter.minimum_separation;
            const auto& deadline = updated.reader.deadline.period;
            if (separation.sec > deadline.sec ||
                (separation.sec == deadline.sec && separation.nanosec > deadline.nanosec)) {
                form.error("Minimum separation must not exceed the deadline.");
                continue;
            }
            if (existing == pending.end()) pending.push_back({&target, current, updated});
            else existing->updated = updated;
            break;
        }
    }
    changes = std::move(pending);
    return true;
}

void edit_effector_form(SDL_Window* parent, DDSDomainParticipant* participant, DDSTopic* topic,
                        effector_qos::Channel& channel, int id, const char* name,
                        const std::vector<ReaderQosTarget>& readers,
                        const effector_qos::Settings& current, const std::vector<Profile>& profiles)
{
    if (!gtk_init_check(nullptr, nullptr)) {
        show_error(parent, "Cannot initialize the GTK QoS form. Check the desktop display connection.");
        return;
    }
    EffectorForm form(parent, name, participant, topic, current, profiles, !readers.empty());
    std::vector<ReaderChange> changes;
    while (true) {
        const int response = gtk_dialog_run(GTK_DIALOG(form.dialog));
        if (response != GTK_RESPONSE_OK && response != 1) return;
        effector_qos::Settings requested;
        if (!form.read(requested)) continue;
        std::string error;
        if (response == 1) {
            auto pending = changes;
            const bool configured = configure_readers_form(parent, GTK_WINDOW(form.dialog),
                participant, readers, requested, pending, error);
            if (!configured) form.error(error);
            else if (!pending.empty()) changes = std::move(pending);
            continue;
        }
        if (!apply_readers(changes, false, parent, error)) {
            std::string rollback_error;
            apply_readers(changes, true, parent, rollback_error);
            form.error(error + (rollback_error.empty() ? "" : "\nRollback: " + rollback_error));
            continue;
        }
        if (!channel.exchange("SET", id, requested, error)) {
            std::string rollback_error;
            apply_readers(changes, true, parent, rollback_error);
            form.error(error + (rollback_error.empty() ? "" : "\nRollback: " + rollback_error));
            continue;
        }
        return;
    }
}
#endif

} // namespace

void process_qos_dialog_events()
{
#ifdef __linux__
    if (!gdk_display_get_default()) return;
    // Bound the work so busy GTK sources cannot starve the simulation.
    for (int i = 0; i < 32 && g_main_context_pending(nullptr); ++i)
        g_main_context_iteration(nullptr, FALSE);
    gdk_display_flush(gdk_display_get_default());
#endif
}

ThreatPublisher::~ThreatPublisher()
{
    if (!publisher) return;
    DDS_ReturnCode_t ret = publisher->delete_contained_entities();
    if (ret == DDS_RETCODE_OK) ret = participant->delete_publisher(publisher);
    if (ret != DDS_RETCODE_OK)
        std::cerr << "Threat publisher cleanup failed: " << ret << '\n';
}

std::unique_ptr<ThreatPublisher> show_threat_qos_dialog(
    SDL_Window* parent, DDSDomainParticipant* participant, DDSTopic* topic,
    const std::vector<ReaderQosTarget>& readers)
{
    if (!participant || !topic) {
        show_error(parent, "DDS participant or threat topic is unavailable.");
        return nullptr;
    }

    effector_qos::Settings settings;
    std::string error;
    if (!edit_publisher(parent, participant, topic, settings, error)) {
        if (!error.empty()) show_error(parent, error);
        return nullptr;
    }

    std::vector<ReaderChange> changes;
    if (!configure_readers(parent, participant, readers, settings, changes, error)) {
        show_error(parent, error);
        return nullptr;
    }
    if (!apply_readers(changes, false, parent, error)) {
        apply_readers(changes, true, parent, error);
        return nullptr;
    }

    auto endpoint = create_threat_publisher(participant, topic, settings, error);
    if (!endpoint) {
        std::string rollback_error;
        apply_readers(changes, true, parent, rollback_error);
        show_error(parent, error);
        if (!rollback_error.empty()) show_error(parent, rollback_error);
    }
    return endpoint;
}

void show_effector_qos_dialog(SDL_Window* parent, DDSDomainParticipant* participant,
    DDSTopic* topic, effector_qos::Channel& channel, int effector_id, const char* name,
    const std::vector<ReaderQosTarget>& readers)
{
    if (!participant || !topic) {
        show_error(parent, "DDS participant or effector topic is unavailable.");
        return;
    }

    effector_qos::Settings settings;
    participant->get_default_publisher_qos(settings.publisher);
    participant->get_default_datawriter_qos(settings.writer);
    std::string error;
    if (!channel.exchange("GET", effector_id, settings, error)) {
        show_error(parent, error);
        return;
    }
    std::vector<Profile> profiles;
    if (!list_profiles(profiles, error)) {
        show_error(parent, error);
        return;
    }
#ifdef __linux__
    edit_effector_form(parent, participant, topic, channel, effector_id, name, readers, settings, profiles);
#else
    const int profile_choice = select_profile_with_current(parent, profiles);
    if (profile_choice == -1) return;
    if (profile_choice == 0) {
        if (participant->get_default_publisher_qos(settings.publisher) != DDS_RETCODE_OK ||
            participant->get_default_datawriter_qos(settings.writer) != DDS_RETCODE_OK) {
            show_error(parent, "Cannot read default writer QoS.");
            return;
        }
        settings.library.clear();
        settings.profile.clear();
    } else if (profile_choice > 0) {
        if (!load_writer_profile(participant, topic, profiles[profile_choice - 1], settings, error)) {
            show_error(parent, error);
            return;
        }
    }
    if (!edit_writer_fields(parent, settings.writer)) return;

    std::vector<ReaderChange> changes;
    if (!configure_readers(parent, participant, readers, settings, changes, error)) {
        show_error(parent, error);
        return;
    }
    if (!apply_readers(changes, false, parent, error)) {
        apply_readers(changes, true, parent, error);
        return;
    }
    if (!channel.exchange("SET", effector_id, settings, error)) {
        std::string rollback_error;
        apply_readers(changes, true, parent, rollback_error);
        show_error(parent, error);
        if (!rollback_error.empty()) show_error(parent, rollback_error);
        return;
    }
    const std::string message = std::string(name) + " publisher QoS updated.";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Endpoint QoS", message.c_str(), parent);
#endif
}
