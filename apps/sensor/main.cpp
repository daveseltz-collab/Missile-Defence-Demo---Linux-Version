#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <cmath>
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
    struct SensorDetection { int sensor_id; int threat_id; double x,y; int confidence; };
}
#endif

using namespace std::chrono_literals;

#ifdef USE_CONNEXT
static ship::SensorDetectionDataWriter *g_detection_writer = nullptr;

// Ship position (pixels) — must match command_control constants
static const float SHIP_X_PX = 400.0f;
static const float SHIP_Y_PX = 570.0f;
// Scale: 350 miles = 570 px  =>  1 nm = 1.1508 mi × (570/350) px/mi ≈ 1.875 px/nm

// Aegis destroyer sensor suite
// range_px: detection radius from ship in screen pixels
struct SensorDef { int id; const char* name; int conf_min, conf_max; float range_px; };
static const SensorDef SENSOR_DEFS[] = {
    {1, "AN/SPY-1D",  88, 100, 412.0f},  // 220 nm — primary Aegis phased-array radar
    {2, "AN/SPQ-9B",  72,  90,  75.0f},  //  40 nm — horizon search / gun FC radar
    {3, "AN/SPS-67",  55,  75,  47.0f},  //  25 nm — surface search radar
    {4, "AN/SLQ-32",  65,  85, 187.0f},  // 100 nm — electronic warfare / ESM
};

class ThreatListener : public DDSDataReaderListener {
public:
    std::mt19937 rng{(unsigned)std::chrono::system_clock::now().time_since_epoch().count()};
    std::mutex rng_mutex;
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
                // Each sensor in the suite reports a detection
                for (const auto& s : SENSOR_DEFS) {
                    // Only report when threat is within this sensor's detection range
                    float dx = float(seq[i].x) - SHIP_X_PX;
                    float dy = float(seq[i].y) - SHIP_Y_PX;
                    if (sqrtf(dx*dx + dy*dy) > s.range_px) continue;

                    std::uniform_int_distribution<int> conf_dist(s.conf_min, s.conf_max);
                    ship::SensorDetection d;
                    d.sensor_id  = s.id;
                    d.threat_id  = seq[i].id;
                    d.x          = seq[i].x;
                    d.y          = seq[i].y;
                    {
                        std::lock_guard<std::mutex> lock(rng_mutex);
                        d.confidence = conf_dist(rng);
                    }
                    if (g_detection_writer) g_detection_writer->write(d, DDS_HANDLE_NIL);
                }
            }
        }
        if (ret == DDS_RETCODE_OK)
            tr->return_loan(seq, infos);
    }
};
#endif

void publishDetection(const ship::SensorDetection &d)
{
#ifdef USE_CONNEXT
    if (g_detection_writer) {
        g_detection_writer->write(d, DDS_HANDLE_NIL);
        return;
    }
#endif
    // Placeholder for DDS publish
    std::cout << "[SENSOR] sensor=" << d.sensor_id << " detected threat=" << d.threat_id << " at (" << d.x << "," << d.y << ") conf=" << d.confidence << "\n";
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--check-dds") {
#ifdef USE_CONNEXT
        std::cout << "Sensor: DDS enabled (domain 0).\n";
        return 0;
#else
        std::cerr << "Sensor: DDS disabled; rebuild with -DUSE_CONNEXT=ON.\n";
        return 1;
#endif
    }
#ifdef USE_CONNEXT
    // DDS setup: subscribe to ThreatTopic and publish SensorDetectionTopic
    effector_qos::Channel qos_channel;
    std::unique_ptr<effector_qos::ReaderEndpoint> threat_reader;
    DDSDomainParticipant *participant = DDSDomainParticipantFactory::get_instance()->create_participant(
        0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (!participant) { std::cerr << "DDS create_participant failed" << std::endl; return 1; }
    else {
        ship::ThreatTypeSupport::register_type(participant, ship::ThreatTypeSupport::get_type_name());
        ship::SensorDetectionTypeSupport::register_type(participant, ship::SensorDetectionTypeSupport::get_type_name());

        DDSTopic *threat_topic = participant->create_topic("ThreatTopic", ship::ThreatTypeSupport::get_type_name(), DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        DDSTopic *sensor_topic = participant->create_topic("SensorDetectionTopic", ship::SensorDetectionTypeSupport::get_type_name(), DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        if (!threat_topic || !sensor_topic) { std::cerr << "Cannot create sensor DDS topics.\n"; return 1; }

        DDSPublisher *publisher = participant->create_publisher(DDS_PUBLISHER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        if (!publisher) { std::cerr << "Cannot create sensor publisher.\n"; return 1; }
        DDSDataWriter *dw = publisher->create_datawriter(sensor_topic, DDS_DATAWRITER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
        g_detection_writer = ship::SensorDetectionDataWriter::narrow(dw);
        if (!g_detection_writer) { std::cerr << "Cannot create sensor detection writer.\n"; return 1; }

        static ThreatListener tl;
        effector_qos::ReaderSettings defaults;
        participant->get_default_subscriber_qos(defaults.subscriber);
        participant->get_default_datareader_qos(defaults.reader);
        threat_reader = effector_qos::create_reader(participant, threat_topic, &tl, defaults);
        if (!threat_reader || !qos_channel.initialize(participant, true, "Sensor")) {
            std::cerr << "Cannot initialize sensor reader QoS service.\n";
            return 1;
        }
    }
    std::cout << "Sensor: DDS ready on domain 0." << std::endl;
#else
    std::cout << "Sensor app starting (DDS disabled)." << std::endl;
#endif

    std::mt19937 rng((unsigned)std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_real_distribution<double> pos(-1000.0,1000.0);

    int sensor_id = 1;
    while (true) {
        // Simulate periodic detection broadcasts when no DDS
#ifndef USE_CONNEXT
        ship::SensorDetection d;
        d.sensor_id = sensor_id;
        d.threat_id = (rng() % 20) + 1;
        d.x = pos(rng);
        d.y = pos(rng);
        d.confidence = 50 + (rng() % 50);
        publishDetection(d);
        std::this_thread::sleep_for(2s);
#else
    effector_qos::process_reader_requests(qos_channel, threat_reader);
    std::this_thread::sleep_for(20ms);
#endif
    }
    return 0;
}
