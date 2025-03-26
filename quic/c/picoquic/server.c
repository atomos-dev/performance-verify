#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "common.h"
#define SERVER_PORT 24433

// Server callback function to handle incoming data
static int server_callback(picoquic_cnx_t* cnx,
    uint64_t stream_id, uint8_t* bytes, size_t length,
    picoquic_call_back_event_t fin_or_event, void* callback_ctx, void* v_stream_ctx)
{
    // Context is not used in this simple example
    (void)callback_ctx;
    (void)v_stream_ctx;

    switch (fin_or_event) {
    case picoquic_callback_stream_data:
    case picoquic_callback_stream_fin:
        break;
    default:
        // Ignore other events
        break;
    }

    return 0;
}

int main(int argc, char** argv)
{
    int port = SERVER_PORT;
    picoquic_quic_t* quic = NULL;
    uint64_t current_time = picoquic_current_time();

    // Create QUIC context without certificates (insecure)
    quic = picoquic_create(8, "server.crt", "server.key", NULL, "simple-protocol", NULL, NULL,
                          NULL, NULL, NULL, current_time, NULL, NULL, NULL, 0);

    if (quic == NULL) {
        fprintf(stderr, "Could not create QUIC context\n");
        return 1;
    }

    //picoquic_set_textlog(quic, "-");

    // Set the callback
    picoquic_set_default_callback(quic, server_callback, NULL);

    printf("Starting server on port %d\n", port);

    // Start the packet loop
    int ret = picoquic_packet_loop(quic, port, 0, 0, 0, 0, NULL, NULL);

    printf("Server stopped: %d\n", ret);

    // Clean up
    picoquic_free(quic);

    return ret;
}