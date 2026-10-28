#pragma once

#include <ndds/ndds_cpp.h>
#include <ndds/dds_cpp/dds_cpp_builtintypes.h>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

namespace effector_qos {

struct Settings {
    DDS_PublisherQos publisher;
    DDS_DataWriterQos writer;
    std::string library;
    std::string profile;
};

struct ReaderSettings {
    DDS_SubscriberQos subscriber;
    DDS_DataReaderQos reader;
    std::string library;
    std::string profile;
};

inline void copy_editable(ReaderSettings& destination, const ReaderSettings& source)
{
    destination.subscriber.partition = source.subscriber.partition;
    destination.reader.ownership.kind = source.reader.ownership.kind;
    destination.reader.durability.kind = source.reader.durability.kind;
    destination.reader.liveliness.kind = source.reader.liveliness.kind;
    destination.reader.liveliness.lease_duration = source.reader.liveliness.lease_duration;
    destination.reader.history.kind = source.reader.history.kind;
    destination.reader.history.depth = source.reader.history.depth;
    destination.reader.reliability.kind = source.reader.reliability.kind;
    destination.reader.deadline.period = source.reader.deadline.period;
    destination.reader.time_based_filter.minimum_separation = source.reader.time_based_filter.minimum_separation;
}

inline void encode(std::ostream& output, const ReaderSettings& settings)
{
    const auto& qos = settings.reader;
    output << std::quoted(settings.library) << ' ' << std::quoted(settings.profile) << ' '
           << int(qos.ownership.kind) << ' ' << int(qos.durability.kind) << ' '
           << int(qos.liveliness.kind) << ' ' << qos.liveliness.lease_duration.sec << ' '
           << qos.liveliness.lease_duration.nanosec << ' ' << int(qos.history.kind) << ' '
           << qos.history.depth << ' ' << int(qos.reliability.kind) << ' '
           << qos.deadline.period.sec << ' ' << qos.deadline.period.nanosec << ' '
           << qos.time_based_filter.minimum_separation.sec << ' '
           << qos.time_based_filter.minimum_separation.nanosec << ' '
           << settings.subscriber.partition.name.length();
    for (DDS_Long index = 0; index < settings.subscriber.partition.name.length(); ++index)
        output << ' ' << std::quoted(settings.subscriber.partition.name[index]);
}

inline bool decode(std::istream& input, ReaderSettings& settings)
{
    auto& qos = settings.reader;
    int ownership, durability, liveliness, history, reliability, partition_count;
    if (!(input >> std::quoted(settings.library) >> std::quoted(settings.profile)
          >> ownership >> durability >> liveliness
          >> qos.liveliness.lease_duration.sec >> qos.liveliness.lease_duration.nanosec
          >> history >> qos.history.depth >> reliability
          >> qos.deadline.period.sec >> qos.deadline.period.nanosec
          >> qos.time_based_filter.minimum_separation.sec
          >> qos.time_based_filter.minimum_separation.nanosec >> partition_count)) return false;
    if (ownership < 0 || ownership > 1 || durability < 0 || durability > 3 ||
        liveliness < 0 || liveliness > 2 || history < 0 || history > 1 ||
        reliability < 0 || reliability > 1 || partition_count < 0 || partition_count > 256) return false;
    qos.ownership.kind = static_cast<DDS_OwnershipQosPolicyKind>(ownership);
    qos.durability.kind = static_cast<DDS_DurabilityQosPolicyKind>(durability);
    qos.liveliness.kind = static_cast<DDS_LivelinessQosPolicyKind>(liveliness);
    qos.history.kind = static_cast<DDS_HistoryQosPolicyKind>(history);
    qos.reliability.kind = static_cast<DDS_ReliabilityQosPolicyKind>(reliability);
    if (!settings.subscriber.partition.name.ensure_length(partition_count, partition_count)) return false;
    for (DDS_Long index = 0; index < partition_count; ++index) {
        std::string name;
        if (!(input >> std::quoted(name)) || name.size() > 2048) return false;
        DDS_String_free(settings.subscriber.partition.name[index]);
        settings.subscriber.partition.name[index] = DDS_String_dup(name.c_str());
        if (!settings.subscriber.partition.name[index]) return false;
    }
    return true;
}

struct ReaderEndpoint {
    DDSDomainParticipant* participant = nullptr;
    DDSTopicDescription* topic = nullptr;
    std::string qos_topic_name;
    DDSDataReaderListener* listener = nullptr;
    DDSSubscriber* subscriber = nullptr;
    DDSDataReader* reader = nullptr;
    ReaderSettings settings;
    ReaderEndpoint() = default;
    ReaderEndpoint(const ReaderEndpoint&) = delete;
    ReaderEndpoint& operator=(const ReaderEndpoint&) = delete;
    ~ReaderEndpoint()
    {
        if (subscriber) {
            subscriber->delete_contained_entities();
            participant->delete_subscriber(subscriber);
        }
    }
};

inline std::unique_ptr<ReaderEndpoint> create_reader(DDSDomainParticipant* participant,
    DDSTopicDescription* topic, DDSDataReaderListener* listener, const ReaderSettings& settings,
    const char* qos_topic_name = nullptr)
{
    auto endpoint = std::make_unique<ReaderEndpoint>();
    endpoint->participant = participant;
    endpoint->topic = topic;
    endpoint->qos_topic_name = qos_topic_name ? qos_topic_name : topic->get_name();
    endpoint->listener = listener;
    endpoint->settings = settings;
    endpoint->subscriber = participant->create_subscriber(settings.subscriber, nullptr, DDS_STATUS_MASK_NONE);
    if (!endpoint->subscriber) return nullptr;
    auto qos = settings.reader;
    qos.protocol.rtps_object_id = DDS_RTPS_AUTO_ID;
    const DDS_GUID_t automatic_guid = {{0}};
    qos.protocol.virtual_guid = automatic_guid;
    endpoint->reader = endpoint->subscriber->create_datareader(
        topic, qos, listener, listener ? DDS_DATA_AVAILABLE_STATUS : DDS_STATUS_MASK_NONE);
    if (!endpoint->reader) return nullptr;
    endpoint->subscriber->get_qos(endpoint->settings.subscriber);
    endpoint->reader->get_qos(endpoint->settings.reader);
    return endpoint;
}

inline bool replace_reader(std::unique_ptr<ReaderEndpoint>& endpoint,
                           const ReaderSettings& requested, std::string& error)
{
    if (!endpoint) { error = "Reader is unavailable."; return false; }
    auto settings = endpoint->settings;
    if (!requested.profile.empty()) {
        auto* factory = DDSDomainParticipantFactory::get_instance();
        auto ret = factory->get_subscriber_qos_from_profile(settings.subscriber,
            requested.library.c_str(), requested.profile.c_str());
        if (ret == DDS_RETCODE_OK)
            ret = factory->get_datareader_qos_from_profile_w_topic_name(settings.reader,
                requested.library.c_str(), requested.profile.c_str(), endpoint->qos_topic_name.c_str());
        if (ret != DDS_RETCODE_OK) {
            error = "Cannot load the reader profile in the receiving application (DDS code " +
                    std::to_string(ret) + ").";
            return false;
        }
    } else if (endpoint->participant->get_default_subscriber_qos(settings.subscriber) != DDS_RETCODE_OK ||
               endpoint->participant->get_default_datareader_qos(settings.reader) != DDS_RETCODE_OK) {
        error = "Cannot read default subscriber/reader QoS.";
        return false;
    }
    copy_editable(settings, requested);
    settings.library = requested.library;
    settings.profile = requested.profile;
    auto replacement = create_reader(endpoint->participant, endpoint->topic, endpoint->listener,
        settings, endpoint->qos_topic_name.c_str());
    if (!replacement) {
        error = "DDS rejected the subscriber/reader QoS. The previous reader remains active. "
                "Check resource limits, deadline, and time-based filter consistency.";
        return false;
    }
    endpoint = std::move(replacement);
    return true;
}

inline void copy_editable(Settings& destination, const Settings& source)
{
    destination.publisher.partition = source.publisher.partition;
    destination.writer.ownership.kind = source.writer.ownership.kind;
    destination.writer.ownership_strength.value = source.writer.ownership_strength.value;
    destination.writer.durability.kind = source.writer.durability.kind;
    destination.writer.liveliness.kind = source.writer.liveliness.kind;
    destination.writer.liveliness.lease_duration = source.writer.liveliness.lease_duration;
    destination.writer.history.kind = source.writer.history.kind;
    destination.writer.history.depth = source.writer.history.depth;
    destination.writer.reliability.kind = source.writer.reliability.kind;
    destination.writer.deadline.period = source.writer.deadline.period;
    destination.writer.lifespan.duration = source.writer.lifespan.duration;
}

inline void encode(std::ostream& output, const Settings& settings)
{
    const auto& qos = settings.writer;
    output << std::quoted(settings.library) << ' ' << std::quoted(settings.profile) << ' '
           << int(qos.ownership.kind) << ' ' << qos.ownership_strength.value << ' '
           << int(qos.durability.kind) << ' ' << int(qos.liveliness.kind) << ' '
           << qos.liveliness.lease_duration.sec << ' ' << qos.liveliness.lease_duration.nanosec << ' '
           << int(qos.history.kind) << ' ' << qos.history.depth << ' '
           << int(qos.reliability.kind) << ' '
           << qos.deadline.period.sec << ' ' << qos.deadline.period.nanosec << ' '
           << qos.lifespan.duration.sec << ' ' << qos.lifespan.duration.nanosec << ' '
           << settings.publisher.partition.name.length();
    for (DDS_Long index = 0; index < settings.publisher.partition.name.length(); ++index)
        output << ' ' << std::quoted(settings.publisher.partition.name[index]);
}

inline bool decode(std::istream& input, Settings& settings)
{
    auto& qos = settings.writer;
    int ownership, durability, liveliness, history, reliability, partition_count;
    if (!(input >> std::quoted(settings.library) >> std::quoted(settings.profile)
          >> ownership >> qos.ownership_strength.value >> durability >> liveliness
          >> qos.liveliness.lease_duration.sec >> qos.liveliness.lease_duration.nanosec
          >> history >> qos.history.depth >> reliability
          >> qos.deadline.period.sec >> qos.deadline.period.nanosec
          >> qos.lifespan.duration.sec >> qos.lifespan.duration.nanosec >> partition_count))
        return false;
    if (ownership < 0 || ownership > 1 || durability < 0 || durability > 3 ||
        liveliness < 0 || liveliness > 2 || history < 0 || history > 1 ||
        reliability < 0 || reliability > 1 || partition_count < 0 || partition_count > 256)
        return false;
    qos.ownership.kind = static_cast<DDS_OwnershipQosPolicyKind>(ownership);
    qos.durability.kind = static_cast<DDS_DurabilityQosPolicyKind>(durability);
    qos.liveliness.kind = static_cast<DDS_LivelinessQosPolicyKind>(liveliness);
    qos.history.kind = static_cast<DDS_HistoryQosPolicyKind>(history);
    qos.reliability.kind = static_cast<DDS_ReliabilityQosPolicyKind>(reliability);
    if (!settings.publisher.partition.name.ensure_length(partition_count, partition_count)) return false;
    for (DDS_Long index = 0; index < partition_count; ++index) {
        std::string name;
        if (!(input >> std::quoted(name)) || name.size() > 2048) return false;
        DDS_String_free(settings.publisher.partition.name[index]);
        settings.publisher.partition.name[index] = DDS_String_dup(name.c_str());
        if (!settings.publisher.partition.name[index]) return false;
    }
    return true;
}

class Channel {
    DDSDomainParticipant* participant_ = nullptr;
    DDSPublisher* publisher_ = nullptr;
    DDSSubscriber* subscriber_ = nullptr;
    DDSStringDataWriter* writer_ = nullptr;
    DDSStringDataReader* reader_ = nullptr;
public:
    Channel() = default;
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    ~Channel()
    {
        if (subscriber_) {
            subscriber_->delete_contained_entities();
            participant_->delete_subscriber(subscriber_);
        }
        if (publisher_) {
            publisher_->delete_contained_entities();
            participant_->delete_publisher(publisher_);
        }
    }
    bool initialize(DDSDomainParticipant* participant, bool server, const std::string& service = "Effector")
    {
        participant_ = participant;
        if (!participant_) return false;
        const auto outgoing = service + (server ? "QosReply" : "QosRequest");
        const auto incoming = service + (server ? "QosRequest" : "QosReply");
        if (DDSStringTypeSupport::register_type(participant_) != DDS_RETCODE_OK) return false;
        auto* output_topic = participant_->create_topic(outgoing.c_str(), DDSStringTypeSupport::get_type_name(),
            DDS_TOPIC_QOS_DEFAULT, nullptr, DDS_STATUS_MASK_NONE);
        auto* input_topic = participant_->create_topic(incoming.c_str(), DDSStringTypeSupport::get_type_name(),
            DDS_TOPIC_QOS_DEFAULT, nullptr, DDS_STATUS_MASK_NONE);
        publisher_ = participant_->create_publisher(DDS_PUBLISHER_QOS_DEFAULT, nullptr, DDS_STATUS_MASK_NONE);
        subscriber_ = participant_->create_subscriber(DDS_SUBSCRIBER_QOS_DEFAULT, nullptr, DDS_STATUS_MASK_NONE);
        if (!output_topic || !input_topic || !publisher_ || !subscriber_) return false;
        DDS_DataWriterQos writer_qos;
        DDS_DataReaderQos reader_qos;
        publisher_->get_default_datawriter_qos(writer_qos);
        subscriber_->get_default_datareader_qos(reader_qos);
        writer_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
        reader_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
        writer_qos.history.depth = 32;
        reader_qos.history.depth = 32;
        writer_ = DDSStringDataWriter::narrow(publisher_->create_datawriter(
            output_topic, writer_qos, nullptr, DDS_STATUS_MASK_NONE));
        reader_ = DDSStringDataReader::narrow(subscriber_->create_datareader(
            input_topic, reader_qos, nullptr, DDS_STATUS_MASK_NONE));
        return writer_ && reader_;
    }
    DDSStringDataReader* reader() const { return reader_; }
    bool send(const std::string& message)
    {
        return writer_ && writer_->write(message.c_str(), DDS_HANDLE_NIL) == DDS_RETCODE_OK;
    }
    template<typename QosSettings>
    bool exchange(const std::string& operation, int id, QosSettings& settings, std::string& error)
    {
        if (!writer_ || !reader_) {
            error = "Effector QoS control channel is unavailable.";
            return false;
        }
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        const auto token = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        bool sent = false;
        while (std::chrono::steady_clock::now() < until) {
            DDS_PublicationMatchedStatus matched;
            if (!sent && writer_->get_publication_matched_status(matched) == DDS_RETCODE_OK &&
                matched.current_count > 0) {
                std::ostringstream request;
                request << operation << ' ' << id << ' ' << token << ' ';
                if (operation.compare(0, 3, "SET") == 0) encode(request, settings);
                if (!send(request.str())) {
                    error = "Cannot send effector QoS request.";
                    return false;
                }
                sent = true;
            }
            DDS_StringSeq messages;
            DDS_SampleInfoSeq infos;
            const auto ret = reader_->take(messages, infos, DDS_LENGTH_UNLIMITED,
                DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
            bool received = false, success = false;
            if (ret == DDS_RETCODE_OK) {
                for (DDS_Long index = 0; index < messages.length(); ++index) {
                    if (!infos[index].valid_data) continue;
                    std::istringstream response(messages[index]);
                    std::string status, response_token;
                    int effector_id;
                    if (!(response >> status >> effector_id >> response_token) ||
                        response_token != token || effector_id != id) continue;
                    received = true;
                    if (status == "OK") {
                        success = decode(response, settings);
                        if (!success) error = "Invalid effector QoS response.";
                    } else {
                        if (!(response >> std::quoted(error))) error = "Effector rejected the QoS request.";
                    }
                }
                reader_->return_loan(messages, infos);
            }
            if (received) return success;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        error = operation.compare(0, 3, "SET") == 0
            ? "No acknowledgment from the application. Reopen the dialog to check whether the change was applied."
            : "No response from the application. Start the updated receiving application and try again.";
        return false;
    }
};

inline std::string reader_reply(const std::string& operation, int id, const std::string& token,
                               std::istream& request, std::unique_ptr<ReaderEndpoint>& endpoint)
{
    std::string error;
    if (!endpoint) error = "Reader is unavailable.";
    else if (operation == "SET_READER") {
        ReaderSettings requested;
        if (!decode(request, requested)) error = "Invalid reader QoS message.";
        else replace_reader(endpoint, requested, error);
    } else if (operation != "GET_READER") error = "Unknown reader request.";
    std::ostringstream reply;
    reply << (error.empty() ? "OK" : "ERROR") << ' ' << id << ' ' << token << ' ';
    if (error.empty()) encode(reply, endpoint->settings);
    else reply << std::quoted(error);
    return reply.str();
}

inline void process_reader_requests(Channel& channel, std::unique_ptr<ReaderEndpoint>& endpoint)
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
        std::string operation, token;
        int id;
        if (request >> operation >> id >> token)
            channel.send(reader_reply(operation, id, token, request, endpoint));
    }
    reader->return_loan(messages, infos);
}

}