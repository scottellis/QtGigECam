#include "capturethread.h"
#include "qtgigecam.h"

#include "webcam.h"
#include "pyloncam.h"


CaptureThread::CaptureThread(QtGigECam *parent, CameraType type) 
	: QThread(), m_parent(parent)
{
    m_stopped = true;
	m_cameraType = type;

	switch (type) {
	case ctWebCam:
		m_camera = new WebCam();
		break;

	case ctPylonCam:
		m_camera = new PylonCam();
		break;

	default:
		m_camera = NULL;
	}
}

CaptureThread::~CaptureThread()
{
	disconnectCamera();

	if (m_camera) {
		delete m_camera;
		m_camera = NULL;
	}
}

bool CaptureThread::startCapture()
{
	if (!m_stopped)
		return false;

	if (!m_camera)
		return false;

	if (!m_camera->open())
		return false;

	m_stopped = false;

	if (!m_camera->startCapture())
		return false;

	start(QThread::TimeCriticalPriority);

	return true;
}

void CaptureThread::stopCapture()
{    
	m_stopped = true;
/*
	QMutex timeoutMutex;
	QWaitCondition waitTemp;

	while (!isFinished()) {
		timeoutMutex.lock();
		waitTemp.wait(&timeoutMutex, 200);
	}
*/
}

void CaptureThread::run()
{
	Mat frame;
	MIL_ID buf_id = 0;
	
    while (!m_stopped) {
		if (!buf_id)
			buf_id = m_parent->getFreeBuffer();

		if (!buf_id) {
			msleep(10);
			continue;
		}

		if (!m_camera->getNextFrame(buf_id)) {
			msleep(10);
			continue;
		}

		if (m_parent->setGrabFrame(buf_id))
			buf_id = 0;
    }

	m_camera->stopCapture();
	
	if (buf_id)
		MbufFree(buf_id);
}

bool CaptureThread::connectCamera(int device)
{
	if (!m_camera)
		return false;

	return m_camera->open();
}

void CaptureThread::disconnectCamera()
{
	if (m_camera)
		m_camera->close();
}

bool CaptureThread::isConnected()
{
	if (!m_camera)
		return false;

    return m_camera->isOpen();
}
