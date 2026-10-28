#pragma once

#include <SDL.h>
#include <ndds/ndds_cpp.h>
#include "ShipThreatSupport.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "../effector_qos.h"

struct ThreatPublisher {
    DDSDomainParticipant* participant = nullptr;
    DDSPublisher* publisher = nullptr;
    ship::ThreatDataWriter* writer = nullptr;

    ThreatPublisher() = default;
    ThreatPublisher(const ThreatPublisher&) = delete;
    ThreatPublisher& operator=(const ThreatPublisher&) = delete;
    ~ThreatPublisher();
};

struct ReaderQosTarget {
    std::string name;
    std::string scope;
    std::string topic_name;
    std::function<bool(effector_qos::ReaderSettings&, std::string&)> get;
    std::function<bool(const effector_qos::ReaderSettings&, std::string&)> set;
};

// Service native dialog events while the application is running its SDL loop.
void process_qos_dialog_events();

std::unique_ptr<ThreatPublisher> show_threat_qos_dialog(
    SDL_Window* parent, DDSDomainParticipant* participant, DDSTopic* topic,
    const std::vector<ReaderQosTarget>& readers = {});

void show_effector_qos_dialog(SDL_Window* parent, DDSDomainParticipant* participant,
    DDSTopic* topic, effector_qos::Channel& channel, int effector_id, const char* name,
    const std::vector<ReaderQosTarget>& readers = {});
