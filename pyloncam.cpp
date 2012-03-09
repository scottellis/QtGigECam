
#include <opencv2/imgproc/imgproc_c.h>

#include "pyloncam.h"

PylonCam::PylonCam()
{
	m_hDev = 0;
	m_hGrabber = 0;
	m_hWait = 0;
	m_payloadSize = 0;
	m_imgWidth = 0;
	m_imgHeight = 0;
	//m_milBayerImageBuf = 0;
	//m_milWBCoefficients = 0;
	//m_bayerConversionType = M_BAYER_BG;
	m_copyBuffIndex = -1;
	m_hNodeMap = 0;
	m_hGainNode = 0;
    m_hExposureNode = 0;

	for (int i = 0; i < NUM_BUFFERS; i++) {
		m_buff[i] = NULL;
		m_hBuff[i] = 0;
	}

#ifdef _DEBUG
	// to prevent connection resets when debugging
	_putenv("PYLON_GIGE_HEARTBEAT=300000");
#endif

	PylonInitialize();
}

PylonCam::~PylonCam()
{
	close();
	
	PylonTerminate();
}

bool PylonCam::isError(GENAPIC_RESULT result)
{
	return (result != GENAPI_E_OK);
}

bool PylonCam::open()
{
	bool success = false;

	if (isOpen())
		return true;

	if (!openDevice(0))
		return false;

	if (!setPixelFormat(true))
		goto open_done;

	if (!setTriggerMode())
		goto open_done;

	if (!setAcquisitionMode())
		goto open_done;

	if (!setPacketSize(8192))
		goto open_done;

	if (!getPayloadSize())
		goto open_done;

	if (!getImageDimensions())
		goto open_done;

	//if (!allocateBayerBuffers())
	//	return false;

	if (!allocateBuffers(8))
		goto open_done;

	adjustSettings();

	success = true;

open_done:

	if (!success)
		close();

	return success;
}

void PylonCam::close()
{
	closeStreamGrabber();

	if (m_hDev) {
		PylonDeviceClose(m_hDev);
		PylonDestroyDevice(m_hDev);
		m_hDev = 0;
	}

	freeBuffers();
	//freeBayerBuffers();

	m_imgWidth = 0;
	m_imgHeight = 0;
}

bool PylonCam::isOpen()
{
	return (m_hDev != 0);
}

QSize PylonCam::getImageSize()
{
	QSize sz(m_imgWidth, m_imgHeight);

	return sz;
}

bool PylonCam::startCapture()
{
	GENAPIC_RESULT result;

	if (!openStreamGrabber())
		return false;

	if (!prepareBuffers())
		return false;

	result = PylonDeviceExecuteCommandFeature(m_hDev, "AcquisitionStart");
	if (isError(result))
		return false;

	m_copyBuffIndex = -1;
	m_stopThread = false;

	start();

	return true;
}

bool PylonCam::stopCapture()
{
	GENAPIC_RESULT result;

	m_stopThread = true;

	while (!isFinished())
		wait(100);

	result = PylonDeviceExecuteCommandFeature(m_hDev, "AcquisitionStop");
	if (isError(result))
		return false;

	releaseBuffers();

	closeStreamGrabber();

	return true;
}

bool PylonCam::getNextFrame(Mat *frame)
{
	GENAPIC_RESULT result;
	CvSize sz = { m_imgWidth, m_imgHeight };
	int index = -1;
	bool ret = false;

	m_copyMutex.lock(); 
	if (m_copyBuffIndex >= 0) {
		index = m_copyBuffIndex;
		m_copyBuffIndex = -1;
	}
	m_copyMutex.unlock();

	if (index >= 0) {
		//MbufPut(m_milBayerImageBuf, m_buff[index]);
		//MbufBayer(m_milBayerImageBuf, buf_id, m_milWBCoefficients, m_bayerConversionType);
		IplImage *bayer = cvCreateImage(sz, IPL_DEPTH_8U, 1);

		if (bayer) {
			bayer->imageData = (char *)m_buff[index];

			IplImage *planar = cvCreateImage(sz, IPL_DEPTH_8U, 3);

			if (planar) {
				//cvCvtColor(bayer, planar, CV_BayerGB2BGR); // gives mosaic pattern
				cvCvtColor(bayer, planar, CV_BayerBG2BGR);   // good, RB correct for Qt
				//cvCvtColor(bayer, planar, CV_BayerBG2RGB); // no mosaic, but RB swapped for Qt
				*frame = Mat(planar, true);
				cvReleaseImage(&planar);
				ret = true;
			}

			cvReleaseImage(&bayer);
		}
	}

	// requeue the grab buffer
	if (index >= 0)
		PylonStreamGrabberQueueBuffer(m_hGrabber, m_hBuff[index], (void *) index);

	return ret;
}

void PylonCam::run()
{
	GENAPIC_RESULT result;
	bool isReady;
	PylonGrabResult_t grabResult;
	int index;

	while (!m_stopThread) {
		result = PylonWaitObjectWait(m_hWait, 1000, &isReady);
		if (isError(result))
			break;

		if (!isReady)
			continue;

		result = PylonStreamGrabberRetrieveResult(m_hGrabber, &grabResult, &isReady);
		if (isError(result))
			break;

		if (!isReady)
			continue; // should never happen

		index = (int) grabResult.Context;

		if (index < 0 || index >= NUM_BUFFERS) 
			break;

		if (m_copyMutex.tryLock()) {
			if (m_copyBuffIndex < 0)
				m_copyBuffIndex = index;
			else
				PylonStreamGrabberQueueBuffer(m_hGrabber, m_hBuff[index], (void *) index);

			m_copyMutex.unlock();
		}
		else {
			PylonStreamGrabberQueueBuffer(m_hGrabber, m_hBuff[index], (void *) index);
		}
	}
}

bool PylonCam::openDevice(int device)
{
	GENAPIC_RESULT result;
	size_t num_devices;

	result = PylonEnumerateDevices(&num_devices);
	if (isError(result))
		return false;

	if (num_devices < device + 1)
		return false;

	result = PylonCreateDeviceByIndex(device, &m_hDev);
	if (isError(result))
		return false;

	 result = PylonDeviceOpen(m_hDev, PYLONC_ACCESS_MODE_CONTROL | PYLONC_ACCESS_MODE_STREAM);
	 if (isError(result)) {
		 PylonDestroyDevice(m_hDev);
		 m_hDev = 0;
		 return false;
	 }

	 return true;
}

bool PylonCam::setPixelFormat(bool color)
{
	GENAPIC_RESULT result;

	if (color) {
		 if (!PylonDeviceFeatureIsAvailable(m_hDev, "EnumEntry_PixelFormat_BayerBG8"))
			return false;

		 result = PylonDeviceFeatureFromString(m_hDev, "PixelFormat", "BayerBG8");
		 if (isError(result))
			return false;
	}
	else {
		return false;
	}

	return true;
}

bool PylonCam::setTriggerMode()
{
	GENAPIC_RESULT result;

	if (PylonDeviceFeatureIsAvailable(m_hDev, "EnumEntry_TriggerControlImplementation_Standard")) {
		result = PylonDeviceFeatureFromString(m_hDev, "TriggerControlImplementation", "Standard");
		if (isError(result))
			return false;
	}

	if (PylonDeviceFeatureIsAvailable(m_hDev, "EnumEntry_TriggerSelector_AcquisitionStart")) {
		result = PylonDeviceFeatureFromString(m_hDev, "TriggerSelector", "AcquisitionStart");
		if (isError(result))
			return false;

		result = PylonDeviceFeatureFromString(m_hDev, "TriggerMode", "Off");
		if (isError(result))
			return false;
	}

	if (PylonDeviceFeatureIsAvailable(m_hDev, "EnumEntry_TriggerSelector_FrameStart")) {
        result = PylonDeviceFeatureFromString(m_hDev, "TriggerSelector", "FrameStart");
		if (isError(result))
			return false;

        result = PylonDeviceFeatureFromString(m_hDev, "TriggerMode", "Off");
        if (isError(result))
			return false;
    }

	if (PylonDeviceFeatureIsAvailable(m_hDev, "EnumEntry_TriggerSource_Line1")) {
		result = PylonDeviceFeatureFromString(m_hDev, "TriggerSource", "Line1");
		if (isError(result))
			return false;
	}

	if (PylonDeviceFeatureIsAvailable(m_hDev, "EnumEntry_TriggerActivation_FallingEdge")) {
		result = PylonDeviceFeatureFromString(m_hDev, "TriggerActivation", "FallingEdge");
		if (isError(result))
			return false;
	}

	// choices are Timed (Exposure setting) or TriggerWidth (dsp controlled)
	if (PylonDeviceFeatureIsAvailable(m_hDev, "EnumEntry_ExposureMode_Timed")) {
		result = PylonDeviceFeatureFromString(m_hDev, "ExposureMode", "Timed");
		if (isError(result))
			return false;
	}

	return true;
}

bool PylonCam::setAcquisitionMode()
{
	GENAPIC_RESULT result;

	result = PylonDeviceFeatureFromString(m_hDev, "AcquisitionMode", "Continuous" );
	if (isError(result))
		return false;

	return true;
}

bool PylonCam::setPacketSize(int size)
{
	GENAPIC_RESULT result;

	if (PylonDeviceFeatureIsWritable(m_hDev, "GevSCPSPacketSize")) {
        result = PylonDeviceSetIntegerFeature(m_hDev, "GevSCPSPacketSize", size);
        if (isError(result))
			return false;
    }

	return true;
}

bool PylonCam::getPayloadSize()
{
	GENAPIC_RESULT result;

	result = PylonDeviceGetIntegerFeatureInt32(m_hDev, "PayloadSize", &m_payloadSize);
	if (isError(result))
		return false;

	return true;
}

bool PylonCam::getImageDimensions()
{
	GENAPIC_RESULT result;

	if (!PylonDeviceFeatureIsReadable(m_hDev, "Width"))
		return false;

	if (!PylonDeviceFeatureIsReadable(m_hDev, "Height"))
		return false;

	result = PylonDeviceGetIntegerFeatureInt32(m_hDev, "Width", &m_imgWidth);
	if (isError(result))
		return false;

	result = PylonDeviceGetIntegerFeatureInt32(m_hDev, "Height", &m_imgHeight);
	if (isError(result))
		return false;

	return true;
}

bool PylonCam::openStreamGrabber()
{
	GENAPIC_RESULT result;
	size_t num_streams;

	result = PylonDeviceGetNumStreamGrabberChannels(m_hDev, &num_streams);
    if (isError(result))
		return false;

	if (num_streams < 1)
		return false;

    result = PylonDeviceGetStreamGrabber(m_hDev, 0, &m_hGrabber);
    if (isError(result))
		return false;

    result = PylonStreamGrabberOpen(m_hGrabber);
    if (isError(result))
		return false;

    result = PylonStreamGrabberGetWaitObject(m_hGrabber, &m_hWait);
    if (isError(result))
		return false;

	return true;
}

void PylonCam::closeStreamGrabber()
{
	if (m_hGrabber) {
		PylonStreamGrabberClose(m_hGrabber);
		m_hGrabber = 0;
	}
}

bool PylonCam::prepareBuffers()
{
	GENAPIC_RESULT result;

    result = PylonStreamGrabberSetMaxNumBuffer(m_hGrabber, NUM_BUFFERS);
    if (isError(result))
		return false;

    result = PylonStreamGrabberSetMaxBufferSize(m_hGrabber, m_payloadSize);
    if (isError(result))
		return false;

    result = PylonStreamGrabberPrepareGrab(m_hGrabber);
    if (isError(result))
		return false;

	for (int i = 0; i < NUM_BUFFERS; i++) {
		result = PylonStreamGrabberRegisterBuffer(m_hGrabber, m_buff[i], m_payloadSize, &m_hBuff[i]);
        if (isError(result))
			return false;
    }

	for (int i = 0; i < NUM_BUFFERS; i++) {
		result = PylonStreamGrabberQueueBuffer(m_hGrabber, m_hBuff[i], (void *) i);
		if (isError(result))
			return false;
	}

	return true;
}

bool PylonCam::releaseBuffers()
{
	GENAPIC_RESULT result;
	PylonGrabResult_t grabResult;
	bool isReady = true;

	result = PylonStreamGrabberCancelGrab(m_hGrabber);
	if (isError(result))
		return false;

	while (isReady) {
		result = PylonStreamGrabberRetrieveResult(m_hGrabber, &grabResult, &isReady);
		if (isError(result))
			break;
	}

	for (int i = 0; i < NUM_BUFFERS; i++) {
		result = PylonStreamGrabberDeregisterBuffer(m_hGrabber, m_hBuff[i]);
		if (isError(result))
			break;
	}

	return true;
}

bool PylonCam::allocateBuffers(int num_buffers)
{
	freeBuffers();

	if (m_payloadSize < 1)
		return false;

	for (int i = 0; i < NUM_BUFFERS; i++) {
		m_buff[i] = new unsigned char [m_payloadSize];

		if (!m_buff[i]) {
			freeBuffers();
			return false;
		}
	}

	return true;
}

void PylonCam::freeBuffers()
{
	for (int i = 0; i < NUM_BUFFERS; i++) {
		if (m_buff[i]) {
			delete [] m_buff[i];
			m_buff[i] = NULL;
		}
	}
}
/*
bool PylonCam::allocateBayerBuffers()
{
	uint32_t width, height;
	float coeff[3];

	if (!m_milBayerImageBuf) {
		m_milBayerImageBuf = MbufAllocColor(M_DEFAULT_HOST, 1, m_imgWidth, m_imgHeight, 8L + M_UNSIGNED, M_IMAGE + M_PROC, M_NULL); 

		if (!m_milBayerImageBuf)
			return false;
	}

	if (!m_milWBCoefficients) {
		m_milWBCoefficients = MbufAlloc1d(M_DEFAULT_HOST, 3, 32 + M_FLOAT, M_ARRAY, M_NULL);

		if (!m_milWBCoefficients)
			return false;
	
		coeff[0] = 1.0;
		coeff[1] = 1.0;
		coeff[2] = 1.45;

		MbufPut(m_milWBCoefficients, (void *)coeff);
	}

	return true;
}

void PylonCam::freeBayerBuffers()
{
	if (m_milBayerImageBuf) {
		MbufFree(m_milBayerImageBuf);
		m_milBayerImageBuf = 0;
	}

	if (m_milWBCoefficients) {
		MbufFree(m_milWBCoefficients);
		m_milWBCoefficients = 0;
	}
}
*/
void PylonCam::adjustSettings()
{
	long gainCurrent, gainMin, gainMax;
	long exposureCurrent, exposureMin, exposureMax;

	getGainValue(&gainCurrent, &gainMin, &gainMax);

	getExposureValue(&exposureCurrent, &exposureMin, &exposureMax);

	setExposureValue(50);
}

#define NODE_NAME_GAIN          "GainRaw"
#define NODE_NAME_EXPOSURE      "ExposureTimeRaw"

bool PylonCam::getNodeMap()
{
	GENAPIC_RESULT result;

	if (m_hNodeMap)
		return true;

	result = PylonDeviceGetNodeMap(m_hDev, &m_hNodeMap);
    if (isError(result))
		return false;

	return true;
}

bool PylonCam::getGainNode()
{
	GENAPIC_RESULT result;

	if (m_hGainNode)
		return true;

	if (!getNodeMap())
		return false;

	result = GenApiNodeMapGetNode(m_hNodeMap, NODE_NAME_GAIN, &m_hGainNode);
	if (isError(result))
		return false;

	if (GENAPIC_INVALID_HANDLE == m_hGainNode) {
		m_hGainNode = 0;
		return false;
	}

	return true;
}

bool PylonCam::getGainValue(long *current, long *min, long *max)
{
	GENAPIC_RESULT result;
	int64_t val;

	if (!getGainNode())
		return false;

	if (current) {
		result = GenApiIntegerGetValue(m_hGainNode, &val); 
		if (isError(result))
			return false;

		*current = (long) val;
	}

	if (min) {
		result = GenApiIntegerGetMin(m_hGainNode, &val);
		if (isError(result))
			return false;

		*min = (long) val;
	}

	if (max) {
		result = GenApiIntegerGetMax(m_hGainNode, &val);
		if (isError(result))
			return false;

		*max = (long) val;
	}

	return true;
}

bool PylonCam::setGainValue(long val)
{
	GENAPIC_RESULT result;
	int64_t val64 = val;

	if (!getGainNode())
		return false;
	
	result = GenApiIntegerSetValue(m_hGainNode, val64);
		
	if (isError(result))
		return false;

	return true;
}

bool PylonCam::getExposureNode()
{
	GENAPIC_RESULT result;

	if (m_hExposureNode)
		return true;

	if (!getNodeMap())
		return false;

	result = GenApiNodeMapGetNode(m_hNodeMap, NODE_NAME_EXPOSURE, &m_hExposureNode);
	if (isError(result))
		return false;

	if (GENAPIC_INVALID_HANDLE == m_hExposureNode) {
		m_hExposureNode = 0;
		return false;
	}

	return true;
}

bool PylonCam::getExposureValue(long *current, long *min, long *max)
{
	GENAPIC_RESULT result;
	int64_t val;

	if (!getExposureNode())
		return false;

	if (current) {
		result = GenApiIntegerGetValue(m_hExposureNode, &val); 
		if (isError(result))
			return false;

		*current = (long) val;
	}

	if (min) {
		result = GenApiIntegerGetMin(m_hExposureNode, &val);
		if (isError(result))
			return false;

		*min = (long) val;
	}

	if (max) {
		result = GenApiIntegerGetMax(m_hExposureNode, &val);
		if (isError(result))
			return false;

		*max = (long) val;
	}

	return true;
}

bool PylonCam::setExposureValue(long val)
{
	GENAPIC_RESULT result;
	int64_t val64 = val;

	if (!getExposureNode())
		return false;

	result = GenApiIntegerSetValue(m_hExposureNode, val64);
		
	if (isError(result))
		return false;

	return true;
}