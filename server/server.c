#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/types.h>
#include <open62541/client_subscriptions.h>

#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

static void
dataChangeCallback(UA_Server* server, UA_UInt32 indexID,
                   void* indexContext, const UA_NodeId* nodeID,
                   void* nodeContext, UA_UInt32 attributeID,
                   const UA_DataValue* value) {}

static void
addMonitoredItem(UA_Server* server) {
    /*
     * Fetch the node and add monitoring.
     */
    UA_NodeId indexNodeID = UA_NODEID_STRING(1, "index");
    UA_MonitoredItemCreateRequest indexRequest =
        UA_MonitoredItemCreateRequest_default(indexNodeID);
    indexRequest.requestedParameters.samplingInterval = 1000.0;
    UA_Server_createDataChangeMonitoredItem(server, UA_TIMESTAMPSTORETURN_SOURCE,
                                            indexRequest, NULL, dataChangeCallback);
}

static void addVariable(UA_Server* server) {
    /*
     * Parameter attribute definition.
     */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_Double index = 0.0;
    UA_Variant_setScalar(&attr.value, &index, &UA_TYPES[UA_TYPES_DOUBLE]);

    attr.description = UA_LOCALIZEDTEXT("en-US", "Random index");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Index");
    attr.dataType = UA_TYPES[UA_TYPES_DOUBLE].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    /*
     * Adding the variable node to the information model.
     */
    UA_NodeId indexNodeId = UA_NODEID_STRING(1, "index");
    UA_QualifiedName indexName = UA_QUALIFIEDNAME(1, "index");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addVariableNode(server, indexNodeId, parentNodeId, parentReferenceNodeId, indexName,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL,
                              NULL);
}

static void writeVariable(UA_Server* server) {
    UA_NodeId indexNodeId = UA_NODEID_STRING(1, "index");

    /*
     * Generate new random value.
     */
    UA_Double index = (double) rand() / (double) (RAND_MAX / 4096);
    UA_Variant variable;
    UA_Variant_init(&variable);
    UA_Variant_setScalar(&variable, &index, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, indexNodeId, variable);
}

static volatile UA_Boolean running = true;

static void stopHandler(int s) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "Received Ctrl-C");
    running = false;
}

int main() {
    /*
     * Initialize random seed, set termination handlers.
     */
    srand(time(NULL));

    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Server* server = UA_Server_new();
    UA_ServerConfig_setDefault(UA_Server_getConfig(server));

    addVariable(server);
    addMonitoredItem(server);

    UA_StatusCode sc = UA_Server_run_startup(server);
    if (sc != UA_STATUSCODE_GOOD) {
        goto cleanup;
    }

    /*
     * Main loop.
     */
    while (running) {
        UA_Server_run_iterate(server, false);
        writeVariable(server);

        struct timespec ts;
        ts.tv_nsec = 100 * 1000;
        ts.tv_sec = 0;
        nanosleep(&ts, NULL);
    }

cleanup:
    UA_Server_delete(server);
    return sc == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
