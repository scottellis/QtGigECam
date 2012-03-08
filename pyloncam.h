#ifndef PYLONCAM_H
#define PYLONCAM_H

#include <QMutex>

#include "camera.h"
#include <pylonc/PylonC.h>

#define NUM_BUFFERS 8

class PylonCam : public Camera
{
public:
	PylonCam();
	virtual ~PylonCam();

	virtual bool open();
	virtual void close();
	virtual bool isOpen();
	virtual bool startCapture();
	virtual bool stopCapture();

	virtual bool getNextFrame(MIL_ID buf_id);

	virtual QSize getImageSize();

protected:
	void run();

private:
	bool isError(GENAPIC_RESULT result);
	bool openDevice(int device = 0);
	bool setPixelFormat(bool color);
	bool setTriggerMode();
	bool setAcquisitionMode();
	bool setPacketSize(int size);
	bool getPayloadSize();
	bool openStreamGrabber();
	void closeStreamGrabber();
	bool allocateBuffers(int num_buffers);
	void freeBuffers();
	bool prepareBuffers();
	bool releaseBuffers();
	bool getImageDimensions();
	bool allocateBayerBuffers();
	void freeBayerBuffers();

	bool getNodeMap();
	bool getGainNode();
	bool getGainValue(long *current, long *min, long *max);
	bool setGainValue(long val);
	bool getExposureNode();
	bool getExposureValue(long *current, long *min, long *max);
	bool setExposureValue(long val);
	void adjustSettings();

	PYLON_DEVICE_HANDLE m_hDev;
	PYLON_STREAMGRABBER_HANDLE m_hGrabber;
	PYLON_WAITOBJECT_HANDLE m_hWait;
	int m_payloadSize;
	unsigned char *m_buff[NUM_BUFFERS];
	PYLON_STREAMBUFFER_HANDLE m_hBuff[NUM_BUFFERS];
	int m_imgWidth;
	int m_imgHeight;
	MIL_ID m_milBayerImageBuf;
	MIL_ID m_milWBCoefficients;
	long m_bayerConversionType;
	QMutex m_copyMutex;
	int m_copyBuffIndex;
	volatile bool m_stopThread;

	NODEMAP_HANDLE m_hNodeMap;
	NODE_HANDLE m_hGainNode;
    NODE_HANDLE m_hExposureNode;  
};

#endif // PYLONCAM_H
