#pragma once

#include <pb_common.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include "ambilight.pb.h"
#include "Timer.h"

#include <WiFiUdp.h>

// State machine
typedef enum state {
    START,
    IDLE,
    TO_DISCOVERY,
    DISCOVERY,
    TO_DATA,
    DATA,
} udp_client_state_t;

class UDPClient {
  public:
    // Constructor. Caller must provided a pointer to the message struct to be
    // used to encode and decode serialized data. Caller also must provide the
    // current WiFi connection's local IP and callback methods that will be
    // called when a packet is transmitted and received.
    UDPClient(Message *message, IPAddress local_ip, void (*tx_callback)(Message *message), void (*rx_callback)(Message *message));

    // Advance state of the UDPClient state machine and return the current state
    udp_client_state_t advance();

    // Get the current UDPClient state
    udp_client_state_t get_state();

  private:
    udp_client_state_t _state;     // current state
    Message *_message;  // pointer to caller-provided message struct

    // WiFi and network setup    
    WiFiUDP _udp;
    IPAddress _server_ip;
    uint16_t _server_port;
    IPAddress _local_ip;
    uint16_t _local_port;

    // For sending encoded messages
    uint8_t _encoded_message[MAX_MESSAGE_BYTES];
    size_t _message_length;
    uint32_t _sequence_number;

    // Timers for timeout
    Timer _timer;
    Timer _timer_ack;

    // Callback functions provided by the caller
    void (*_tx_callback)(Message *message);
    void (*_rx_callback)(Message *message);

    // Protobuf utilities
    bool _parse_pb(WiFiUDP *udp, int packet_size);
    bool _create_pb(MessageType type, uint8_t *buffer, size_t *message_length);
    bool _create_pb(MessageType type, uint8_t *buffer, size_t *message_length, IPAddress local_ip, uint16_t local_port);
};