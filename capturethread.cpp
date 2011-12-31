#include "capturethread.h"
#include "qtgigecam.h"


bool CaptureThread::startCapture(Camera *camera, MIL_ID buff_id)
{
	if (isRunning())
		return false;

	if (!camera || !camera->open())
		return false;

	m_camera = camera;
	m_buff = buff_id;
	m_stop = false;

	if (!m_camera->startCapture())
		return false;

	start(QThread::TimeCriticalPriority);

	return true;
}

void CaptureThread::stopCapture()
{    
	m_stop = true;
}

void CaptureThread::run()
{
    while (!m_stop) {
		if (!m_camera->getNextFrame(m_buff)) {
			msleep(10);
			continue;
		}

		emit newImage(m_buff);
    }

	m_camera->stopCapture();
}
