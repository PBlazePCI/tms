/* (c) Copyright, Real-Time Innovations, 2012.  All rights reserved.
* RTI grants Licensee a license to use, modify, compile, and create derivative
* works of the software solely for use with RTI Connext DDS. Licensee may
* redistribute copies of the software provided that all such copies are subject
* to this license. The software is provided "as is", with no warranty of any
* type, including any warranty for fitness for any purpose. RTI is under no
* obligation to maintain or support the software. RTI shall not be liable for
* any incidental or consequential damages arising out of the use or inability
* to use the software.
*/

/*  This file is really an extension of the tmsCommsPattern.cxx and holds the
    Reader/Writer handlers that are dispatched from the comm patterns. As such 
    these functions are called with in the context of the thread processing a 
    particular pattern. 

    The signature and naming convention is:
        void TypeHandler_tms_TOPIC<TOPIC> (void);
        where type Handler is: Reader, Writer, PeriodicWriter, and OnChangeWriter.
 */

#include <stdio.h>
#include <iostream>
#include "tmsCommPatternTopicHndlrs.h"

// Global arrays of different pattern handlers - we need separate arrays in case
// the same topic is both a reader and writer (e.g., RequestResponse)
HandlerPtr reader_handler_ptrs[] = {
    GenericDefaultHandler,                                  // tms_TOPIC_ACTIVE_DIAGNOSTICS_ENUM,,
    GenericDefaultHandler,                                  // tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_OUTCOME_ENUM ,,
    GenericDefaultHandler,                                  // tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_RESPONSE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_CONFIG_RESERVATION_STATE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_COPY_CONFIG_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DC_DEVICE_POWER_MEASUREMENT_LIST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DC_LOAD_SHARING_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DC_LOAD_SHARING_STATUS_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DEVICE_ANNOUNCEMENT_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DEVICE_CLOCK_STATUS_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DEVICE_GROUNDING_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DEVICE_GROUNDING_STATUS_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DEVICE_PARAMETER_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DEVICE_PARAMETER_STATUS_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DEVICE_POWER_MEASUREMENT_LIST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DEVICE_POWER_PORT_LIST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DEVICE_POWER_STATUS_LIST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_DISCOVERED_CONNECTION_LIST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_ENGINE_STATE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_FINGERPRINT_NICKNAME_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_FINGERPRINT_NICKNAME_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_GET_CONFIG_CONTENTS_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_GET_CONFIG_DC_LOAD_SHARING_RESPONSE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_GET_CONFIG_DEVICE_PARAMETER_RESPONSE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_GET_CONFIG_GROUNDING_CIRCUIT_RESPONSE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_GET_CONFIG_LOAD_SHARING_RESPONSE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_GET_CONFIG_POWER_SWITCH_RESPONSE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_GET_CONFIG_SOURCE_TRANSITION_RESPONSE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_GET_CONFIG_STORAGE_CONTROL_RESPONSE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_GROUNDING_CIRCUIT_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_HEARTBEAT_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_LOAD_SHARING_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_LOAD_SHARING_STATUS_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_MICROGRID_CONNECTION_LIST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM,
    ReaderHandler_tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST,   // tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_OPERATOR_CONNECTION_LIST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_OPERATOR_INTENT_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_OPERATOR_INTENT_STATE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_POWER_SWITCH_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_RELEASE_CONFIG_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_REQUEST_RESPONSE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_RESERVE_CONFIG_REPLY_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_RESERVE_CONFIG_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_SOURCE_TRANSITION_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_SOURCE_TRANSITION_STATE_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_STANDARD_CONFIG_MASTER_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_STORAGE_CONTROL_REQUEST_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_STORAGE_CONTROL_STATUS_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_STORAGE_INFO_ENUM,
    GenericDefaultHandler,                                  // tms_TOPIC_STORAGE_STATE_ENUM,
    GenericDefaultHandler                                   // tms_TOPIC_LAST_SENTINEL_ENUM        
};

HandlerPtr periodic_handler_ptrs[] = { 
    GenericDefaultHandler, // tms_TOPIC_ACTIVE_DIAGNOSTICS_ENUM,
    GenericDefaultHandler, // tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_OUTCOME_ENUM,
    GenericDefaultHandler, // tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_RESPONSE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_CONFIG_RESERVATION_STATE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_COPY_CONFIG_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DC_DEVICE_POWER_MEASUREMENT_LIST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DC_LOAD_SHARING_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DC_LOAD_SHARING_STATUS_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DEVICE_ANNOUNCEMENT_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DEVICE_CLOCK_STATUS_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DEVICE_GROUNDING_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DEVICE_GROUNDING_STATUS_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DEVICE_PARAMETER_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DEVICE_PARAMETER_STATUS_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DEVICE_POWER_MEASUREMENT_LIST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DEVICE_POWER_PORT_LIST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DEVICE_POWER_STATUS_LIST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_DISCOVERED_CONNECTION_LIST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_ENGINE_STATE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_FINGERPRINT_NICKNAME_ENUM,
    GenericDefaultHandler, // tms_TOPIC_FINGERPRINT_NICKNAME_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_GET_CONFIG_CONTENTS_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_GET_CONFIG_DC_LOAD_SHARING_RESPONSE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_GET_CONFIG_DEVICE_PARAMETER_RESPONSE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_GET_CONFIG_GROUNDING_CIRCUIT_RESPONSE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_GET_CONFIG_LOAD_SHARING_RESPONSE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_GET_CONFIG_POWER_SWITCH_RESPONSE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_GET_CONFIG_SOURCE_TRANSITION_RESPONSE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_GET_CONFIG_STORAGE_CONTROL_RESPONSE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_GROUNDING_CIRCUIT_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_HEARTBEAT_ENUM,
    GenericDefaultHandler, // tms_TOPIC_LOAD_SHARING_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_LOAD_SHARING_STATUS_ENUM,
    GenericDefaultHandler, // tms_TOPIC_MICROGRID_CONNECTION_LIST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM,
    GenericDefaultHandler, // tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_OPERATOR_CONNECTION_LIST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_OPERATOR_INTENT_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_OPERATOR_INTENT_STATE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_POWER_SWITCH_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_RELEASE_CONFIG_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_REQUEST_RESPONSE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_RESERVE_CONFIG_REPLY_ENUM,
    GenericDefaultHandler, // tms_TOPIC_RESERVE_CONFIG_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_SOURCE_TRANSITION_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_SOURCE_TRANSITION_STATE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_STANDARD_CONFIG_MASTER_ENUM,
    GenericDefaultHandler, // tms_TOPIC_STORAGE_CONTROL_REQUEST_ENUM,
    GenericDefaultHandler, // tms_TOPIC_STORAGE_CONTROL_STATUS_ENUM,
    GenericDefaultHandler, // tms_TOPIC_STORAGE_INFO_ENUM,
    GenericDefaultHandler, // tms_TOPIC_STORAGE_STATE_ENUM,
    GenericDefaultHandler, // tms_TOPIC_LAST_SENTINEL_ENUM        
};



void GenericDefaultHandler(void * infoBlck) {
    std::cout << "No Handler attatched to this topic" << std::endl;
    return;
}

void ReaderHandler_tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST (void * infoBlck) {
    // To Do - can the cast be avoided i.e. pass in a  ReaderThreadInfo *  vs void *
    ReaderThreadInfo * myReaderThreadInfo = (ReaderThreadInfo *) infoBlck; 
    std::cout <<  myReaderThreadInfo->me() << " (should check for MM_JOIN/LEAVE) " << std::endl;

    // ****** PUT WHAT YOU NEED TO DO SPECIFICALLY FOR 

    // if we responded tms_REPLY_OK then we should set the internal variable as MMR_COMPLETE
    // the mail_loop of the MSM should now see a difference between the internal state and the tms_state 
    // causing an On Change tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME to get triggered
    internal_membership_result = MMR_COMPLETE;
    
   end_reader_hndlr:
   return;
}


                           /*
                            switch  (myReaderThreadInfo->topic_enum()) {  

                                

                                case  tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM: // MSM Sim receives this from Device
                                    
                                case  tms_TOPIC_REQUEST_RESPONSE_ENUM:
                                    std::cout << "Recieved" << tms_TOPIC_REQUEST_RESPONSE_NAME << std::endl;
                                    break;
                                case  tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM: // Device receives from MSM
                                    std::cout << "Received " << tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_NAME << std::endl;
                                    break;
                                default: 
                                    std::cout << "Received unhandled Topic - default topic fall through" << std::endl;
                                    break;
                            } 
                            */  
    