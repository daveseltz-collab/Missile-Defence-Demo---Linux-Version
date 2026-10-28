#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <set>
#include <cmath>
#include <array>
#include <mutex>
#include <string>

#ifdef USE_CONNEXT
#include <ndds/ndds_cpp.h>
#include "ShipThreat.h"
#include "ShipThreatSupport.h"
#include "../effector_qos.h"
#endif

#ifndef USE_CONNEXT
namespace ship {
    struct Threat { int id; double x,y,heading,speed; int severity; };
    struct EffectorAction { int effector_id; int threat_id; bool destroyed; double x,y; };
}
#endif

using namespace std::chrono_literals;

#ifdef USE_CONNEXT
struct EffectorEndpoint {
    DDSDomainParticipant* participant = nullptr;
    DDSPublisher* publisher = nullptr;
    ship::EffectorActionDataWriter* writer = nullptr;
    effector_qos::Settings settings;
    ~EffectorEndpoint()
    {
        if (publisher) {
            publisher->delete_contained_entities();
            participant->delete_publisher(publisher);
        }
    }
};

static std::array<std::shared_ptr<EffectorEndpoint>, 5> g_effectors;
static std::mutex g_effectors_mutex;

static std::unique_ptr<EffectorEndpoint> create_endpoint(
    DDSDomainParticipant* participant, DDSTopic* topic, const effector_qos::Settings& settings)
{
    auto endpoint = std::make_unique<EffectorEndpoint>();
    endpoint->participant = participant;
    endpoint->settings = settings;
    endpoint->publisher = participant->create_publisher(settings.publisher, nullptr, DDS_STATUS_MASK_NONE);
    if (!endpoint->publisher) {
        std::cerr << "Effector publisher creation failed.\n";
        return nullptr;
    }
    auto writer_qos = settings.writer;
    writer_qos.protocol.rtps_object_id = DDS_RTPS_AUTO_ID;
    const DDS_GUID_t automatic_guid = {{0}};
    writer_qos.protocol.virtual_guid = automatic_guid;
    endpoint->writer = ship::EffectorActionDataWriter::narrow(endpoint->publisher->create_datawriter(
        topic, writer_qos, nullptr, DDS_STATUS_MASK_NONE));
    if (!endpoint->writer) {
        std::cerr << "Effector writer creation failed: history=" << settings.writer.history.depth
                  << ", max_samples=" << settings.writer.resource_limits.max_samples
                  << ", max_samples_per_instance=" << settings.writer.resource_limits.max_samples_per_instance
                  << std::endl;
        return nullptr;
    }
    endpoint->publisher->get_qos(endpoint->settings.publisher);
    endpoint->writer->get_qos(endpoint->settings.writer);
    return endpoint;
}

static void process_qos_requests(effector_qos::Channel& channel,
                                DDSDomainParticipant* participant, DDSTopic* topic,
                                std::unique_ptr<effector_qos::ReaderEndpoint>& threat_reader)
{
    auto* reader = channel.reader();
    if (!reader) return;
    DDS_StringSeq messages;
    DDS_SampleInfoSeq infos;
    if (reader->take(messages, infos, DDS_LENGTH_UNLIMITED, DDS_ANY_SAMPLE_STATE,
                     DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE) != DDS_RETCODE_OK) return;
    for (DDS_Long index = 0; index < messages.length(); ++index) {
        if (!infos[index].valid_data) continue;
        std::istringstream request(messages[index]);
        std::string operation, token, error;
        int id = 0;
        if (!(request >> operation >> id >> token)) continue;
        if (operation == "GET_READER" || operation == "SET_READER") {
            channel.send(effector_qos::reader_reply(operation, id, token, request, threat_reader));
            continue;
        }
        std::shared_ptr<EffectorEndpoint> current;
        {
            std::lock_guard<std::mutex> lock(g_effectors_mutex);
            if (id >= 1 && id <= int(g_effectors.size())) current = g_effectors[id - 1];
        }
        effector_qos::Settings settings;
        if (!current) {
            error = "Effector is unavailable.";
        } else if (operation == "GET") {
            settings = current->settings;
        } else if (operation == "SET") {
            effector_qos::Settings requested;
            if (!effector_qos::decode(request, requested)) {
                error = "Invalid QoS configuration message.";
            } else {
                settings = current->settings;
                if (!requested.profile.empty()) {
                    auto* factory = DDSDomainParticipantFactory::get_instance();
                    auto ret = factory->get_publisher_qos_from_profile(settings.publisher,
                        requested.library.c_str(), requested.profile.c_str());
                    if (ret == DDS_RETCODE_OK)
                        ret = factory->get_datawriter_qos_from_profile_w_topic_name(settings.writer,
                            requested.library.c_str(), requested.profile.c_str(), topic->get_name());
                    if (ret != DDS_RETCODE_OK)
                        error = "Cannot load the selected profile in the effector application (DDS code " +
                                std::to_string(ret) + ").";
                } else {
                    auto ret = participant->get_default_publisher_qos(settings.publisher);
                    if (ret == DDS_RETCODE_OK)
                        ret = participant->get_default_datawriter_qos(settings.writer);
                    if (ret != DDS_RETCODE_OK)
                        error = "Cannot load default publisher/writer QoS (DDS code " +
                                std::to_string(ret) + ").";
                }
                if (error.empty()) {
                    effector_qos::copy_editable(settings, requested);
                    settings.library = requested.library;
                    settings.profile = requested.profile;
                    auto replacement = create_endpoint(participant, topic, settings);
                    if (!replacement) {
                        error = "Cannot create the effector publisher/writer with these QoS values. "
                                "The previous configuration is still active; check DDS diagnostics.";
                    } else {
                        settings = replacement->settings;
                        {
                            std::lock_guard<std::mutex> lock(g_effectors_mutex);
                            g_effectors[id - 1] = std::move(replacement);
                        }
                        std::cout << "Updated QoS for effector " << id << std::endl;
                    }
                }
            }
        } else {
            error = "Unknown QoS request.";
        }
        std::ostringstream reply;
        reply << (error.empty() ? "OK" : "ERROR") << ' ' << id << ' ' << token << ' ';
        if (error.empty()) effector_qos::encode(reply, settings);
        else reply << std::quoted(error);
        channel.send(reply.str());
    }
    reader->return_loan(messages, infos);
}

// Aegis destroyer weapon systems with individual probability-of-kill (%)
struct EffectorDef { int id; const char* name; int pk; };
static const EffectorDef EFFECTOR_DEFS[] = {
    {1, "SM-2 MR",   75},  // Standard Missile 2 Medium Range (VLS)
    {2, "SM-6",      87},  // Standard Missile 6 extended range (VLS)
    {3, "ESSM",      68},  // Evolved Sea Sparrow Missile (VLS)
    {4, "CIWS",      52},  // Phalanx Close-In Weapon System
    {5, "MK 45/62",  36},  // 5-inch/62 cal gun (surface/low-alt threats)
};

class ThreatListener : public DDSDataReaderListener {
public:
    std::mt19937 rng{(unsigned)std::chrono::system_clock::now().time_since_epoch().count()};
    std::uniform_int_distribution<int> chance{1, 100};
    std::set<int> engaged;   // threat IDs already engaged — skip republished updates
    std::mutex state_mutex;
    void on_data_available(DDSDataReader* reader) override {
        ship::ThreatDataReader *tr = ship::ThreatDataReader::narrow(reader);
        if (!tr) return;
        ship::ThreatSeq seq;
        DDS_SampleInfoSeq infos;
        DDS_ReturnCode_t ret = tr->take(seq, infos, DDS_LENGTH_UNLIMITED,
                                        DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
        if (ret == DDS_RETCODE_OK) {
            for (DDS_Long i = 0; i < seq.length(); ++i) {
                if (!infos[i].valid_data) continue;
                // Only engage when threat has entered SPY-1D detection range
                // (same 412 px radius used by the sensor — detect first, then shoot)
                {
                    float dx = float(seq[i].x) - 400.0f;  // SHIP_X
                    float dy = float(seq[i].y) - 570.0f;  // SHIP_Y
                    if (sqrtf(dx*dx + dy*dy) > 380.0f) continue;  // not yet in range
                }
                {
                    std::lock_guard<std::mutex> lock(state_mutex);
                    if (!engaged.insert(seq[i].id).second) continue;
                }
                // Layered defense: all weapons engage; MK45 only for surface threats
                for (const auto& e : EFFECTOR_DEFS) {
                    if (e.id == 5 && seq[i].severity < 3) continue; // MK45 for surface threats only
                    ship::EffectorAction a;
                    a.effector_id = e.id;
                    a.threat_id   = seq[i].id;
                    {
                        std::lock_guard<std::mutex> lock(state_mutex);
                        a.destroyed = (chance(rng) <= e.pk);
                    }
                    a.x           = seq[i].x;
                    a.y           = seq[i].y;
                    std::shared_ptr<EffectorEndpoint> endpoint;
                    {
                        std::lock_guard<std::mutex> lock(g_effectors_mutex);
                        endpoint = g_effectors[e.id - 1];
                    }
                    if (endpoint) endpoint->writer->write(a, DDS_HANDLE_NIL);
                }
            }
        }
        if (ret == DDS_RETCODE_OK)
            tr->return_loan(seq, infos);
    }
};
#endif

void publishAction(const ship::EffectorAction &a)
{
#ifdef USE_CONNEXT
    std::shared_ptr<EffectorEndpoint> endpoint;
    {
        std::lock_guard<std::mutex> lock(g_effectors_mutex);
        if (a.effector_id >= 1 && a.effector_id <= int(g_effectors.size()))
            endpoint = g_effectors[a.effector_id - 1];
    }
    if (endpoint) {
        endpoint->writer->write(a, DDS_HANDLE_NIL);
        return;
    }
#endif
    // Placeholder for DDS publish
    std::cout << "[EFFECTOR] effector=" << a.effector_id << " action on threat=" << a.threat_id << " destroyed=" << a.destroyed << "\n";
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--check-dds") {
#ifdef USE_CONNEXT
        std::cout << "Effector: DDS enabled (domain 0).\n";
        return 0;
#else
        std::cerr << "Effector: DDS disabled; rebuild with -DUSE_CONNEXT=ON.\n";
        return 1;
#endif
    }
#ifdef USE_CONNEXT
    // DDS setup: subscribe to ThreatTopic and publish EffectorActionTopic
    effector_qos::Channel qos_channel;
    std::unique_ptr<effector_qos::ReaderEndpoint> threat_reader;
    DDSTopic* effector_topic = nullptr;
    DDSDomainParticipant *participant = DDSDomainParticipantFactory::get_instance()->create_participant(
        0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (!participant) { std::cerr << "DDS create_participant failed" << std::endl; return 1; }
    else {
        ship::ThreatTypeSupport::register_type(participant, ship::ThreatTypeSupport::get_type_name());
        ship::EffectorActionTypeSupport::register_type(participant, ship::EffectorActionTypeSupport::get_type_name());

        DDSTopic *threat_topic = participant->create_topic("ThreatTopic", ship::ThreatTypeSupport::get_type_name(), DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        effector_topic = participant->create_topic("EffectorActionTopic", ship::EffectorActionTypeSupport::get_type_name(), DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        if (!threat_topic || !effector_topic) { std::cerr << "Cannot create effector DDS topics.\n"; return 1; }

        effector_qos::Settings defaults;
        participant->get_default_publisher_qos(defaults.publisher);
        participant->get_default_datawriter_qos(defaults.writer);
        for (auto& endpoint : g_effectors) {
            endpoint = create_endpoint(participant, effector_topic, defaults);
            if (!endpoint) { std::cerr << "Cannot create effector endpoint.\n"; return 1; }
        }
        if (!qos_channel.initialize(participant, true)) {
            std::cerr << "Cannot initialize effector QoS control channel.\n";
            return 1;
        }

        static ThreatListener tl;
        effector_qos::ReaderSettings reader_defaults;
        participant->get_default_subscriber_qos(reader_defaults.subscriber);
        participant->get_default_datareader_qos(reader_defaults.reader);
        threat_reader = effector_qos::create_reader(participant, threat_topic, &tl, reader_defaults);
        if (!threat_reader) { std::cerr << "Cannot create effector threat reader.\n"; return 1; }
    }
    std::cout << "Effector: DDS ready on domain 0." << std::endl;
#else
    std::cout << "Effector app starting (DDS disabled)." << std::endl;
#endif

    std::mt19937 rng((unsigned)std::chrono::system_clock::now().time_since_epoch().count());
    int effector_id = 1;
    while (true) {
#ifndef USE_CONNEXT
        ship::EffectorAction a;
        a.effector_id = effector_id;
        a.threat_id = (rng() % 20) + 1;
        a.destroyed = (rng() % 2) == 0;
        a.x = 0.0; a.y = 0.0;
        publishAction(a);
        std::this_thread::sleep_for(3s);
#else
        process_qos_requests(qos_channel, participant, effector_topic, threat_reader);
        std::this_thread::sleep_for(20ms);
#endif
    }
    return 0;
}
