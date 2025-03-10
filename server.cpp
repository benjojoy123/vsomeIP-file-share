#include "server.h"

Server::Server() : app(vsomeip::runtime::get()->create_application("World")) {}
void Server::init() {
    app->init();
}

void Server::registerHandlers() {
    app->register_message_handler(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_METHOD_ID, [this](const std::shared_ptr<vsomeip::message>& request) {
        onMessage(request);
    });
}

void Server::offerService() {
    app->offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
}

void Server::start() {
    app->start();
}

void Server::onMessage(const std::shared_ptr<vsomeip::message>& request) {
    std::cout << "SERVICE: Received a file request from the client." << std::endl;
    // Providing the path of the image file
    std::ifstream file("/boot/images.jpg", std::ios::binary);
    if (file.is_open()) {
        const size_t fragment_size = 1350; // Setting the fragment size to be less than the maximum message size
        // Reading the content of the image file
        std::vector<vsomeip::byte_t> file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        // Calculate the number of fragments
        size_t num_fragments = (file_content.size() + fragment_size - 1) / fragment_size;
        std::cout<<" number of fragments "<<num_fragments<<std::endl;
        // Send each fragment as a separate packet
        for (size_t i = 0; i < num_fragments; ++i) {
            CustomPacket custom_packet;
            custom_packet.sequence_number = i + 1; // Sequence number starts from 1
            custom_packet.timestamp = 0; // Set your timestamp logic
            custom_packet.filename = "images.jpg"; // Set your filename logic
            // Calculate the range for the current fragment
            size_t start_index = i * fragment_size;
            size_t end_index = std::min((i + 1) * fragment_size, file_content.size());
            // Extract the payload data for the current fragment
            custom_packet.payload_data.assign(file_content.begin() + start_index, file_content.begin() + end_index);
            custom_packet.payload_length = static_cast<uint32_t>(custom_packet.payload_data.size());
            // Serialize the custom packet
            std::ostringstream oss;
            oss << custom_packet;
            // Create response
            std::shared_ptr<vsomeip::message> its_response = vsomeip::runtime::get()->create_response(request);
            std::shared_ptr<vsomeip::payload> its_payload = vsomeip::runtime::get()->create_payload();
            // Set serialized data to the payload
            its_payload->set_data(reinterpret_cast<const vsomeip::byte_t*>(oss.str().data()), oss.str().size());
            its_response->set_payload(its_payload);
            app->send(its_response);
            std::cout << "SERVICE: Sent fragment " << (i + 1) << " of " << num_fragments << std::endl;
            // std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        std::cout << "SERVICE: Sent the fragments of the image file to the client." << std::endl;
        file.close();
    } else {
        std::cerr << "SERVICE: Failed to open the image file." << std::endl;
    }
}
int main() {
    Server server;
    server.init();
    server.registerHandlers();
    server.offerService();
    server.start();
    return 0;
}

