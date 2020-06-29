// Copyright 2020 Maris Melbardis
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*++

Module Name:

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    User-mode Driver Framework 2

--*/

#include "driver.h"
#include "queue.tmh"

NTSTATUS
EmuControllerQueueInitialize(
    _In_ WDFDEVICE Device,
	_Out_ WDFQUEUE *Queue
    )
/*++

Routine Description:

     The I/O dispatch callbacks for the frameworks device object
     are configured in this function.

     A single default I/O Queue is configured for parallel request
     processing, and a driver context memory allocation is created
     to hold our structure QUEUE_CONTEXT.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    VOID

--*/
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;
	WDF_OBJECT_ATTRIBUTES queueAttributes;
	PQUEUE_CONTEXT          queueContext;


    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
         &queueConfig,
        WdfIoQueueDispatchParallel
        );

    queueConfig.EvtIoDeviceControl = EmuControllerEvtIoDeviceControl;


	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
		&queueAttributes,
		QUEUE_CONTEXT);



    status = WdfIoQueueCreate(
                 Device,
                 &queueConfig,
                 &queueAttributes,
                 &queue
                 );

    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, 
			"WdfIoQueueCreate failed %!STATUS!", 
			status);
        return status;
    }

	queueContext = QueueGetContext(queue);
	queueContext->Queue = queue;
	queueContext->DeviceContext = DeviceGetContext(Device);


	*Queue = queue;
    return status;
}


NTSTATUS
EmuControllerManualQueueInitialize(
	_In_  WDFDEVICE         Device,
	_Out_ WDFQUEUE			*Queue
)
/*++
Routine Description:
	This function creates a manual I/O queue to receive IOCTL_HID_READ_REPORT
	forwarded from the device's default queue handler.
	It also creates a periodic timer to check the queue and complete any pending
	request with data from the device. Here timer expiring is used to simulate
	a hardware event that new data is ready.
	The workflow is like this:
	- Hidclass.sys sends an ioctl to the miniport to read input report.
	- The request reaches the driver's default queue. As data may not be avaiable
	  yet, the request is forwarded to a second manual queue temporarily.
	- Later when data is ready (as simulated by timer expiring), the driver
	  checks for any pending request in the manual queue, and then completes it.
	- Hidclass gets notified for the read request completion and return data to
	  the caller.
	On the other hand, for IOCTL_HID_WRITE_REPORT request, the driver simply
	sends the request to the hardware (as simulated by storing the data at
	DeviceContext->DeviceData) and completes the request immediately. There is
	no need to use another queue for write operation.
Arguments:
	Device - Handle to a framework device object.
	Queue - Output pointer to a framework I/O queue handle, on success.
Return Value:
	NTSTATUS
--*/
{
	NTSTATUS                status;
	WDF_IO_QUEUE_CONFIG     queueConfig;
	WDF_OBJECT_ATTRIBUTES   queueAttributes;
	WDFQUEUE                queue;
	PQUEUE_CONTEXT			queueContext;

	WDF_IO_QUEUE_CONFIG_INIT(
		&queueConfig,
		WdfIoQueueDispatchManual);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
		&queueAttributes,
		QUEUE_CONTEXT);


	status = WdfIoQueueCreate(
		Device,
		&queueConfig,
		&queueAttributes,
		&queue);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, 
			"WdfIoManualQueueCreate failed %!STATUS!", 
			status);
		return status;
	}

	queueContext = QueueGetContext(queue);
	queueContext->Queue = queue;
	queueContext->DeviceContext = DeviceGetContext(Device);


	*Queue = queue;

	return status;
}

VOID
EmuControllerEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    OutputBufferLength - Size of the output buffer in bytes

    InputBufferLength - Size of the input buffer in bytes

    IoControlCode - I/O control code.

Return Value:

    VOID

--*/
{
	NTSTATUS                status = STATUS_NOT_IMPLEMENTED;
	WDFDEVICE               device;
	PDEVICE_CONTEXT         deviceContext;
	PQUEUE_CONTEXT          queueContext;
	BOOLEAN                 completeRequest = TRUE;

	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);

	device = WdfIoQueueGetDevice(Queue);
	queueContext = QueueGetContext(Queue);
	deviceContext = DeviceGetContext(device);

	if (deviceContext->HidDescriptor.bLength == 0 || 
		deviceContext->HidDescriptor.DescriptorList[0].wReportLength == 0)
	{
		WdfRequestComplete(Request, status);
		return;
	}


	switch (IoControlCode)
	{
	case IOCTL_HID_GET_DEVICE_DESCRIPTOR:   // METHOD_NEITHER
		//
		// Retrieves the device's HID descriptor.
		//
	    status = RequestCopyFromBuffer(Request,
				&deviceContext->HidDescriptor,
				deviceContext->HidDescriptor.bLength);
		
		break;

	case IOCTL_HID_GET_DEVICE_ATTRIBUTES:   // METHOD_NEITHER
		//
		//Retrieves a device's attributes in a HID_DEVICE_ATTRIBUTES structure.
		//
		status = RequestCopyFromBuffer(Request,
			&queueContext->DeviceContext->HidDeviceAttributes,
			sizeof(HID_DEVICE_ATTRIBUTES));
		break;

	case IOCTL_HID_GET_REPORT_DESCRIPTOR:   // METHOD_NEITHER
		//
		//Obtains the report descriptor for the HID device.
		//
		status = RequestCopyFromBuffer(Request,
			deviceContext->ReportDescriptor,
			deviceContext->HidDescriptor.DescriptorList[0].wReportLength);
		break;

	case IOCTL_HID_READ_REPORT:             // METHOD_NEITHER
		//
		// Returns a report from the device into a class driver-supplied
		// buffer.
		//
		status = ReadReport(queueContext, Request, &completeRequest);
		break;

	case IOCTL_HID_WRITE_REPORT:            // METHOD_NEITHER
		//
		// Transmits a class driver-supplied report to the device.
		//
		status = WriteReport(queueContext, Request);
		break;
	
	//
	// HID minidriver IOCTL uses HID_XFER_PACKET which contains an embedded pointer.
	//
	//   typedef struct _HID_XFER_PACKET {
	//     PUCHAR reportBuffer;
	//     ULONG  reportBufferLen;
	//     UCHAR  reportId;
	//   } HID_XFER_PACKET, *PHID_XFER_PACKET;
	//
	// UMDF cannot handle embedded pointers when marshalling buffers between processes.
	// Therefore a special driver mshidumdf.sys is introduced to convert such IRPs to
	// new IRPs (with new IOCTL name like IOCTL_UMDF_HID_Xxxx) where:
	//
	//   reportBuffer - passed as one buffer inside the IRP
	//   reportId     - passed as a second buffer inside the IRP
	//
	// The new IRP is then passed to UMDF host and driver for further processing.
	//

	case IOCTL_UMDF_HID_GET_FEATURE:        // METHOD_NEITHER

		status = GetFeature(queueContext, Request);
		break;

	case IOCTL_UMDF_HID_SET_FEATURE:        // METHOD_NEITHER

		status = SetFeature(queueContext, Request);
		break;

	case IOCTL_UMDF_HID_GET_INPUT_REPORT:  // METHOD_NEITHER

		status = GetInputReport(queueContext, Request);
		break;

	case IOCTL_UMDF_HID_SET_OUTPUT_REPORT: // METHOD_NEITHER

		status = SetOutputReport(queueContext, Request);
		break;

	case IOCTL_HID_GET_STRING:                      // METHOD_NEITHER

		status = GetString(Request);
		break;

	case IOCTL_HID_GET_INDEXED_STRING:              // METHOD_OUT_DIRECT

		status = GetIndexedString(Request);
		break;

	case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:  // METHOD_NEITHER
	case IOCTL_HID_ACTIVATE_DEVICE:                 // METHOD_NEITHER
	case IOCTL_HID_DEACTIVATE_DEVICE:               // METHOD_NEITHER
	case IOCTL_GET_PHYSICAL_DESCRIPTOR:             // METHOD_OUT_DIRECT
	default:
		status = STATUS_NOT_IMPLEMENTED;
		break;
	
	}

	if (completeRequest)
	{
		WdfRequestComplete(Request, status);
	}

    return;
}