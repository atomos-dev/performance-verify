#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>  // For time functions
#include "common.h"

#define SERVER_PORT 24433

const char* picoquic_event_to_string(picoquic_call_back_event_t event)
{
    switch (event) {
    case picoquic_callback_stream_data:
        return "STREAM_DATA";
    case picoquic_callback_stream_fin:
        return "STREAM_FIN";
    case picoquic_callback_stream_reset:
        return "STREAM_RESET";
    case picoquic_callback_stop_sending:
        return "STOP_SENDING";
    case picoquic_callback_stateless_reset:
        return "STATELESS_RESET";
    case picoquic_callback_close:
        return "CLOSE";
    case picoquic_callback_application_close:
        return "APPLICATION_CLOSE";
    case picoquic_callback_stream_gap:
        return "STREAM_GAP";
    case picoquic_callback_prepare_to_send:
        return "PREPARE_TO_SEND";
    case picoquic_callback_almost_ready:
        return "ALMOST_READY";
    case picoquic_callback_ready:
        return "READY";
    case picoquic_callback_datagram:
        return "DATAGRAM";
    case picoquic_callback_version_negotiation:
        return "VERSION_NEGOTIATION";
    case picoquic_callback_request_alpn_list:
        return "REQUEST_ALPN_LIST";
    case picoquic_callback_set_alpn:
        return "SET_ALPN";
    case picoquic_callback_pacing_changed:
        return "PACING_CHANGED";
    case picoquic_callback_prepare_datagram:
        return "PREPARE_DATAGRAM";
    case picoquic_callback_datagram_acked:
        return "DATAGRAM_ACKED";
    case picoquic_callback_datagram_lost:
        return "DATAGRAM_LOST";
    case picoquic_callback_datagram_spurious:
        return "DATAGRAM_SPURIOUS";
    case picoquic_callback_path_available:
        return "PATH_AVAILABLE";
    case picoquic_callback_path_suspended:
        return "PATH_SUSPENDED";
    case picoquic_callback_path_deleted:
        return "PATH_DELETED";
    case picoquic_callback_path_quality_changed:
        return "PATH_QUALITY_CHANGED";
    case picoquic_callback_path_address_observed:
        return "PATH_ADDRESS_OBSERVED";
    case picoquic_callback_app_wakeup:
        return "APP_WAKEUP";
    case picoquic_callback_next_path_allowed:
        return "NEXT_PATH_ALLOWED";
    default:
        return "UNKNOWN_EVENT";
    }
}

// Client context structure to track throughput test
typedef struct {
    size_t packet_size;       // Size of each packet in bytes
    uint64_t duration;        // Test duration in microseconds
    uint64_t start_time;      // Start time of the test
    uint64_t bytes_sent;      // Total bytes sent
    int test_running;         // Flag to indicate if test is running
    uint64_t stream_id;       // Stream ID used for the test
} client_context_t;

// Client callback function
static int client_callback(picoquic_cnx_t* cnx,
    uint64_t stream_id, uint8_t* bytes, size_t length,
    picoquic_call_back_event_t fin_or_event, void* callback_ctx, void* v_stream_ctx)
{
    client_context_t* ctx = (client_context_t*)callback_ctx;
    uint64_t current_time = picoquic_current_time();

    switch (fin_or_event) {
    case picoquic_callback_ready:
        // Connection is ready, start the throughput test
        ctx->stream_id = picoquic_get_next_local_stream_id(cnx, 0);
        ctx->start_time = current_time;
        ctx->bytes_sent = 0;
        ctx->test_running = 1;

        // Print UTC time
        {
            time_t now = time(NULL);
            struct tm *tm_info = gmtime(&now);
            char time_str[26];
            strftime(time_str, 26, "%Y-%m-%d %H:%M:%S UTC", tm_info);

            printf("Test started at: %s\n", time_str);
            printf("Connection ready, starting throughput test on stream %" PRIu64 "\n", ctx->stream_id);
            printf("Packet size: %zu bytes, Duration: %.1f seconds\n",
                   ctx->packet_size, (double)ctx->duration / 1000000.0);
        }

        // Mark the stream as active to trigger prepare_to_send events
        picoquic_mark_active_stream(cnx, ctx->stream_id, 1, NULL);
        break;

    case picoquic_callback_prepare_to_send:
        // QUIC stack is ready to send data
        if (ctx->test_running && stream_id == ctx->stream_id) {
            uint64_t test_elapsed = current_time - ctx->start_time;

            if (test_elapsed < ctx->duration) {
                // Test is still running, send data
                size_t available = length;
                size_t to_send = (ctx->packet_size < available) ? ctx->packet_size : available;
                int is_fin = 0; // Not the final data

                // Get the actual buffer to write data to
                uint8_t* buffer = picoquic_provide_stream_data_buffer(bytes, to_send, is_fin, 1);
                if (buffer != NULL) {
                    ctx->bytes_sent += to_send;
                } else {
                    fprintf(stderr, "Error: could not get data buffer\n");
                    return -1;
                }
            } else if (ctx->test_running) {
                // Test duration has elapsed, end the test
                ctx->test_running = 0;

                // Print UTC time
                {
                    time_t now = time(NULL);
                    struct tm *tm_info = gmtime(&now);
                    char time_str[26];
                    strftime(time_str, 26, "%Y-%m-%d %H:%M:%S UTC", tm_info);

                    printf("\nTest completed at: %s\n", time_str);
                }

                // Calculate and display throughput
                double duration_seconds = (double)test_elapsed / 1000000.0;
                double mbps = ((double)ctx->bytes_sent * 8.0) / (duration_seconds * 1000000.0);

                printf("Throughput test completed:\n");
                printf("  Total bytes sent: %zu bytes\n", ctx->bytes_sent);
                printf("  Duration: %.2f seconds\n", duration_seconds);
                printf("  Throughput: %.2f Mbps\n", mbps);

                // Send FIN to close the stream
                uint8_t* buffer = picoquic_provide_stream_data_buffer(bytes, 0, 1, 1);
                if (buffer == NULL) {
                    fprintf(stderr, "Error: could not get data buffer for FIN\n");
                }

                // Close the connection
                picoquic_close(cnx, 0);
            }
        }
        return 0;

    case picoquic_callback_stream_data:
    case picoquic_callback_stream_fin:
        // Handle server response if needed
        if (length > 0) {
            // For debugging, print any server response
            printf("Received %zu bytes from server on stream %" PRIu64 "\n", length, stream_id);
        }
        break;

    default:
        // Ignore other events
        break;
    }

    return 0;
}

int main(int argc, char** argv)
{
    const char* server_name = "localhost";
    int server_port = SERVER_PORT;
    size_t packet_size = 1200;  // Default packet size: 4KB
    int duration_seconds = 10;  // Default test duration: 10 seconds

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            server_name = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            server_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            packet_size = (size_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            duration_seconds = atoi(argv[++i]);
        }
    }

    // Create client context
    client_context_t client_ctx;
    memset(&client_ctx, 0, sizeof(client_context_t));
    client_ctx.packet_size = packet_size;
    client_ctx.duration = (uint64_t)duration_seconds * 1000000; // Convert to microseconds

    // Create QUIC context
    picoquic_quic_t* quic = NULL;
    uint64_t current_time = picoquic_current_time();

    quic = picoquic_create(8, NULL, NULL, NULL, "simple-protocol", NULL, NULL,
                          NULL, NULL, NULL, current_time, NULL, NULL, NULL, 0);

    if (quic == NULL) {
        fprintf(stderr, "Could not create QUIC context\n");
        return 1;
    }

    // Create connection to server
    struct sockaddr_storage server_address;
    int is_name = 0;

    if (picoquic_get_server_address(server_name, server_port, &server_address, &is_name) != 0) {
        fprintf(stderr, "Could not resolve server address\n");
        picoquic_free(quic);
        return 1;
    }

    // Create a connection
    picoquic_cnx_t* cnx = picoquic_create_cnx(quic, picoquic_null_connection_id, picoquic_null_connection_id,
        (struct sockaddr*)&server_address, current_time, 0, server_name, "simple-protocol", 1);
    picoquic_set_verify_certificate_callback(quic, NULL, NULL);

    if (cnx == NULL) {
        fprintf(stderr, "Could not create connection\n");
        picoquic_free(quic);
        return 1;
    }

    // Set the callback with our context
    picoquic_set_callback(cnx, client_callback, &client_ctx);

    // Start the connection
    if (picoquic_start_client_cnx(cnx) != 0) {
        fprintf(stderr, "Could not start connection\n");
        picoquic_delete_cnx(cnx);
        picoquic_free(quic);
        return 1;
    }

    printf("Starting client connection to %s:%d\n", server_name, server_port);

    // Start the packet loop
    int ret = picoquic_packet_loop(quic, 0, server_address.ss_family, 0, 0, 0, NULL, NULL);

    // Clean up
    picoquic_free(quic);

    return ret;
}