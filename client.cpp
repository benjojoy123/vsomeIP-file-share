#include "client.h"

Client::Client() : app(vsomeip::runtime::get()->create_application("Hello")) {}

void Client::init() {
    app->init();
}

void Client::registerHandlers() {
    app->register_availability_handler(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, [this](vsomeip::service_t, vsomeip::instance_t, bool available) {
        onAvailability(available);
    });

    app->register_message_handler(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_METHOD_ID, [this](const std::shared_ptr<vsomeip::message>& response) {
        onMessage(response);
    });
}

void Client::requestService() {
    app->request_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
}

void Client::start() {
    std::thread sender(&Client::run, this);
    app->start();
    sender.join();
}

void Client::run() {
    std::unique_lock<std::mutex> its_lock(mutex);
    condition.wait(its_lock, [this] {
        return service_available;
    });

    if (service_available) {
        std::shared_ptr<vsomeip::message> request;
        request = vsomeip::runtime::get()->create_request();
        request->set_service(SAMPLE_SERVICE_ID);
        request->set_instance(SAMPLE_INSTANCE_ID);
        request->set_method(SAMPLE_METHOD_ID);

        app->send(request);
        std::cout << "Text file request sent" << std::endl;
    } else {
        std::cout << "Service is not available. Cannot send text file request." << std::endl;
    }
}

void Client::onMessage(const std::shared_ptr<vsomeip::message>& response) {

    std::cout<<"------------onMessage-----------"<<std::endl;
    std::shared_ptr<vsomeip::payload> its_payload = response->get_payload();
    vsomeip::length_t l = its_payload->get_length();

    // Get payload
    std::vector<vsomeip::byte_t> binary_data(its_payload->get_data(), its_payload->get_data() + l);

    CustomFilePacket packet;
    deserializeCustomFilePacket(packet, binary_data);

    // Extract filename from the payload
    std::string received_filename = packet.filename;

    // Generate a timestamp for unique file names
    auto timestamp = std::chrono::system_clock::now();
    auto time_point = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_point), "%Y%m%d%H%M%S");

    // Construct the full file path
    std::string file_name = "/boot" + received_filename + "_" + ss.str() + ".jpg";

    // Open the output file in append mode if not opened yet or if the filename has changed
    if (!output_file.is_open() || received_filename != packet.filename) {
        if (output_file.is_open()) {
            output_file.close();
            std::cout << "CLIENT: Closed previous file." << std::endl;
        }

        output_file.open(file_name, std::ios::binary | std::ios::app);
        if (output_file.is_open()) {
            std::cout << "CLIENT: Opened new file: " << file_name << std::endl;
        } else {
            std::cerr << "CLIENT: Failed to open the output file at path: " << file_name << std::endl;
        }
    }

    if (output_file.is_open()) {
        // Append the payload data to the file
        output_file.write(reinterpret_cast<const char*>(packet.payload_data.data()), packet.payload_length);
        std::cout << "CLIENT: Appended file content to " << file_name << std::endl;

        // Update total data received
        total_data_received += packet.payload_length;
        std::cout << "CLIENT: Total data received: " <<std::dec<< total_data_received << " bytes" << std::endl;
    } else {
        std::cerr << "CLIENT: Failed to open the output file." << std::endl;
    }

    // Notify the main thread that the content has been received
    condition.notify_one();
}

void Client::onAvailability(bool available) {
    std::cout << "CLIENT: Service ["
              << std::setw(4) << std::setfill('0') << std::hex << SAMPLE_SERVICE_ID << "." << SAMPLE_INSTANCE_ID
              << "] is "
              << (available ? "available." : "NOT available.")
              << std::endl;

    service_available = available;
    condition.notify_one();
}

void Client::deserializeCustomFilePacket(CustomFilePacket& packet, const std::vector<vsomeip::byte_t>& serialized_data) {
    size_t offset = 0;
    // Deserialize binary data into the CustomFilePacket structure
    std::memcpy(&packet.sequence_number, serialized_data.data() + offset, sizeof(packet.sequence_number));
    offset += sizeof(packet.sequence_number);

    std::memcpy(&packet.timestamp, serialized_data.data() + offset, sizeof(packet.timestamp));
    offset += sizeof(packet.timestamp);

    std::memcpy(&packet.filename_length, serialized_data.data() + offset, sizeof(packet.filename_length));
    offset += sizeof(packet.filename_length);

    char filename_buffer[packet.filename_length];
    std::memcpy(filename_buffer, serialized_data.data() + offset, packet.filename_length);
    packet.filename.assign(filename_buffer, packet.filename_length);
    offset += packet.filename_length;

    std::memcpy(&packet.payload_length, serialized_data.data() + offset, sizeof(packet.payload_length));
    offset += sizeof(packet.payload_length);

    packet.payload_data.resize(packet.payload_length);
    std::memcpy(packet.payload_data.data(), serialized_data.data() + offset, packet.payload_length);
}

bool Client::isAllPacketsReceived() const {
    // Check if all packets with sequence numbers from 1 to N are received
    for (uint32_t i = 1; i <= received_packets.size(); ++i) {
        if (received_packets.find(i) == received_packets.end()) {
            return false;
        }
    }
    return true;
}

int main() {
    // Establish connection with the server
    Client client;
    client.init();
    client.registerHandlers();
    client.requestService();
    client.start();  // Start the client to establish the connection

    return 0;
}
