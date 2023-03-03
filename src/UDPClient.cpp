#include "Constants.h"
#include "UDPClient.h"
#include "Utils.h"

UDPClient::UDPClient(Message *message, IPAddress local_ip, void (*tx_callback)(Message *message), void (*rx_callback)(Message *message)) {
    _message = message;
    _state = START;
    _sequence_number = 0;
    _local_ip = local_ip;
    _local_port = UDP_BROADCAST_PORT;
    _timer = Timer();
    _timer_ack = Timer();
    _tx_callback = tx_callback;
    _rx_callback = rx_callback;
}

udp_client_state_t UDPClient::advance() {
    switch (_state) {
        /*
            State: START
            Behavior: Setup UDP port to receive broadcast messages.
            Transition: Once setup, transition to IDLE.
        */
        case START: {
            _udp.stop();
            _local_port = UDP_BROADCAST_PORT;   // reset to listen to broadcast
            print("Starting UDP at %s:%d\n", _local_ip.toString().c_str(), _local_port);
            if (!_udp.begin(_local_port)) {
                print("Failed to begin UDP!\n");
            }
            print("Transition: START ==> IDLE\n");
            _state = IDLE;
            break;
        }
        /*
            State: IDLE
            Behavior: Listen for discovery message.
            
            Transition: On receipt of discovery message, store server's IP/port, 
            start transition to TO_DISCOVERY state.
        */
        case IDLE: {
            int packet_size = _udp.parsePacket();
            if (packet_size) {
                print("%d bytes from %s:%d\n", packet_size, _udp.remoteIP().toString().c_str(), _udp.remotePort());

                if (_parse_pb(&_udp, packet_size)) {
                    if (_message->type == MessageType_DISCOVERY) {
                        print("Received discovery message!\n");

                        _server_ip = _udp.remoteIP();
                        _server_port = _udp.remotePort();
                        print("Setting server as %s:%d\n", _server_ip.toString().c_str(), _server_port);
                        
                        // Comment out the code below to keep using the UDP_BROADCAST_PORT 
                        // instead of randomizing it.
                        // In a real-world use case we probably want to randomize the port to
                        // prevent server spoofing attacks. Continuing to use UDP_BROADCAST_PORT
                        // allows us to keep listening for discovery packets 
                        // once we start collecting data.

                        _udp.stop();
                        _local_port = (rand() % (MAX_UDP_PORT - MIN_UDP_PORT)) + MIN_UDP_PORT;
                        print("Restarting UDP service on port %d\n", _local_port);
                        _udp.begin(_local_port);

                        print("Transition: IDLE ==> TO_DISCOVERY\n");
                        _state = TO_DISCOVERY;
                    }
                } else {
                    print("Failed to parse packet!\n");
                }
            }
            break;
        }
        /*
            State: TO_DISCOVERY

            Behavior: Set discovery timeout timer.

            Transition: Immediately transition to DISCOVERY.
        */
        case TO_DISCOVERY: {
            _timer.set_timeout_ms(DISCOVERY_TIMEOUT_MS);
            print("Transition: TO_DISCOVERY ==> DISCOVERY\n");
            _state = DISCOVERY;
            break;
        }
        /*
            State: DISCOVERY

            Behavior: Send config packets every DISCOVERY_CONFIG_MS.

            Transition: On receipt of ACK, transition to TO_DATA state. If no ACK is
            received before timeout, transition to START state.
        */
        case DISCOVERY: {
            // Send config packet to server
            if (!_udp.beginPacket(_server_ip, _server_port)) {
                print("Failed to begin packet!\n");
            }
            if (_create_pb(MessageType_CONFIG, _encoded_message, &_message_length, _local_ip, _local_port)) {
                print("Sending config message of length %d\n", _message_length);
                size_t written = _udp.write(_encoded_message, _message_length);
                if (written != _message_length) {
                    print("Only wrote %d instead of %d bytes!\n", written, _message_length);
                }
            }
            if (!_udp.endPacket()) {
                print("Failed to end packet!\n");
            }
            
            // Check for ack packet from server
            int packet_size = _udp.parsePacket();
            if (packet_size) {
                print("%d bytes from %s:%d\n", packet_size, _udp.remoteIP().toString().c_str(), _udp.remotePort());

                if (_parse_pb(&_udp, packet_size)) {
                    if (_message->type == MessageType_ACK_DISCOVERY) {
                        print("Received discovery ack!\n");
                        print("Transition: DISCOVERY ==> TO_DATA\n");
                        _state = TO_DATA;
                    }
                } else {
                    print("Failed to parse packet!\n");
                }
            }

            if (_timer.has_elapsed()) {
                print("Timed out\n");
                print("Transition: DISCOVERY ==> START\n");
                _state = START;
            }
            vTaskDelay(DISCOVERY_CONFIG_MS / portTICK_PERIOD_MS);

            break;
        }
        /*
            State: TO_DATA

            Behavior: Set wifi power saving off. Set heartbeat timers.
            
            Transition: Immediately transition to DATA.
        */
        case TO_DATA: {
            _timer.set_timeout_ms(HEARTBEAT_MS);
            _timer_ack.set_timeout_ms(HEARTBEAT_ACK_MS);
            print("Transition: TO_DATA ==> DATA\n");
            _state = DATA;
            break;
        }
        /*
            State: DATA

            Behavior: Listen for data messages and parse. If the sequence number is larger 
            than the last received, immediately send to LEDs and update the last seen
            sequence number.
            
            Transition: On receipt of discovery message, transition back to IDLE state in
            order to re-establish connection.
        */
        case DATA: {
            // Check for data from server
            int packet_size = _udp.parsePacket();
            if (packet_size) {
                print("%d bytes from %s:%d\n", packet_size, _udp.remoteIP().toString().c_str(), _udp.remotePort());

                if (_parse_pb(&_udp, packet_size)) {
                    _rx_callback(_message);
                    if (_message->type == MessageType_DATA) {
                        print("Received data!\n");
                    }
                    if (_message->type == MessageType_ACK_HEARTBEAT) {
                        print("Received heartbeat ack!\n");
                        _timer_ack.reset();
                    }
                    // if (decoded_message.type == MessageType_DISCOVERY) {
                    //     print("Received unexpected discovery message! Resetting\n");
                    //     print("Transition: DATA ==> IDLE\n");
                    //     state = IDLE;
                    // }
                } else {
                    print("Failed to parse packet!\n");
                }
            }
            // Send heartbeat
            if (_timer.has_elapsed(true)) {
                if (!_udp.beginPacket(_server_ip, _server_port)) {
                    print("Failed to begin packet!\n");
                }
                _create_pb(MessageType_HEARTBEAT, _encoded_message, &_message_length);
                print("Sending heartbeat message of length %d\n", _message_length);
                size_t written = _udp.write(_encoded_message, _message_length);
                if (written != _message_length) {
                    print("Only wrote %d instead of %d bytes!\n", written, _message_length);
                }
                if (!_udp.endPacket()) {
                    print("Failed to end packet!\n");
                }
            }
            // Check that we have received an ack from the server
            if (_timer_ack.has_elapsed()) {
                print("Timed out waiting for heartbeat ack from server!\n");
                print("Transition: DATA ==> START\n");
                _state = START;
            }
            break;
        }
        default: {
            print("Client state %d not recognized!\n", _state);
            break;
        }
    }

    return _state;
}

udp_client_state_t UDPClient::get_state() {
    return this->_state;
}

/*
    Creates a serialized message, stores it in the buffer pointer and updates 
    the message_length variable.
*/
bool UDPClient::_create_pb(MessageType type, uint8_t *buffer, size_t *message_length) {
    return _create_pb(type, buffer, message_length, IPAddress(), 0);
}

/*
    Creates a serialized message, stores it in the buffer pointer and updates 
    the message_length variable.
*/
bool UDPClient::_create_pb(MessageType type, uint8_t *buffer, size_t *message_length, IPAddress local_ip, uint16_t local_port) {
    bool status;

    // Initialize the message memory
    *_message = (Message)Message_init_zero;

    // Create a stream that will write to our buffer
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, MAX_MESSAGE_BYTES);

    // Fill the message. IMPORTANT: the has_ fields must be explicitly set to
    // true, otherwise they will not be encoded.

    _message->has_type = true;
    _message->type = type;

    switch (type) {
        case MessageType_CONFIG: {
            _message->has_config = true;
            _message->config.has_ipv4 = true;
            strlcpy(_message->config.ipv4, _local_ip.toString().c_str(), sizeof(_message->config.ipv4) - 1);

            _message->config.has_port = true;
            _message->config.port = local_port;
            break;
        }
        case MessageType_HEARTBEAT: {
            break;
        }
        default: {
            print("pb type not recognized!\n");
            return false;
        }
    }

    _tx_callback(_message); // caller will populate sender field and any config-specific information

    // Encode the message
    status = pb_encode(&stream, Message_fields, _message);
    *message_length = stream.bytes_written;

    if (!status) {
        print("Encoding failed: %s\n", PB_GET_ERROR(&stream));
    }

    return status;
}

/*
    Parses a protobuf packet received via UDP and stores the contents in memory
    at the passed in message pointer. Returns true is successful, false
    otherwise.
*/
bool UDPClient::_parse_pb(WiFiUDP *udp, int packet_size) {
    bool status;
    uint8_t buffer[MAX_MESSAGE_BYTES];
    int len = udp->read(buffer, MAX_MESSAGE_BYTES - 1);

    // Null-terminate
    if (len > 0) {
        buffer[len] = 0;
    }

    // Create a stream that reads from the buffer
    pb_istream_t stream = pb_istream_from_buffer(buffer, packet_size);

    *_message = (Message)Message_init_zero;    // reset the message

    // Now decode the message
    status = pb_decode(&stream, Message_fields, _message);

    if (!status) {
        printf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
    }

    return status;
}
