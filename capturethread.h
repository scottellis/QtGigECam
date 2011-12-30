#ifndef CAPTURETHREAD_H
#define CAPTURETHREAD_H

#include <QThread>
#include <QtGui>

#include "camera.h"

enum CameraType { ctWebCam, ctPylonCam };

class QtGigECam;

class CaptureThread : public QThread
{
    Q_OBJECT

public:
    CaptureThread(QtGigECam *parent, CameraType type = ctWebCam);
	virtual ~CaptureThread();

	bool connectCamera(int device);
    void disconnectCamera();
	bool isConnected();

	bool startCapture();
    void stopCapture();
    bool isStopped() { return m_stopped; }

protected:
    void run();

private:
	QtGigECam *m_parent;
	CameraType m_cameraType;
	Camera *m_camera;    
    volatile bool m_stopped;

};

#endif // CAPTURETHREAD_H
