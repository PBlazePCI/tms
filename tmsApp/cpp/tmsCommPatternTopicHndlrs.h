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

/*  This file is really an extension of the tmsCommsPattern.cxx and hold the
    Reader/Writer handlers that are dispatched from the comm patterns. As such 
    these functions are called with in the context of the thread processing a 
    particular pattern. 

    The signature and naming convention is:
        void TypeHandler_tms_TOPIC<TOPIC_NAME> (void);
        where Type Handler is: Reader, <Writer>, PeriodicWriter, and <OnChangeWriter>.

        Notes: 
        <Writer> typically will have no topics as it just sits or generic
        issues/events common to all non-periodic or non-OnChange writers. But if
        there was a reason to process those events specific to a topic then you would
        want to add handlers for generic Writers.

        <OnChangeWriter> seems generic enough not to need Topic specific handling.
        It simpley sends the topic pointed to and resets the trigger pointed to.
 */

#ifndef tmsCommsPatternTopicHndlrs_h
#define tmsCommsPatternTopicHndlrs_h

#include "tmsCommon.h"
#include "tmsCommPatternTopicHndlrs.h"
#include "tmsCommPatterns.h"


void GenericDefaultHandler(void * InfoBlck); // This handler will get called if no handler is installed
void ReaderHandler_tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST (void * InfoBlck);
void ReaderHandler_tms_TOPIC_REQUEST_RESPONSE (void * InfoBlck);
void ReaderHandler_tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME (void * InfoBlck);

void PeriodicWriterHandler_tms_TOPIC_HEARTBEAT (void * InfoBlck);

#endif