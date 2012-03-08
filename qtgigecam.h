#ifndef QTGIGECAM_H
#define QTGIGECAM_H

#include <mil.h>
#include <QtGui>
#include "ui_qtgigecam.h"


class CaptureThread;
class Camera;

enum CameraType { ctWebCam, ctPylonCam };


class QtGigECam : public QMainWindow
{
	Q_OBJECT

public:
	QtGigECam(QWidget *parent = 0, Qt::WFlags flags = 0);
	~QtGigECam();

public slots:
	void startVideo();
	void stopVideo();
	void toggleScaling();
	void newImage(MIL_ID id);

protected:
	void timerEvent(QTimerEvent *event);

private:
	void showImage(MIL_ID buf_id);
	bool prepareBuffers(int numBuffers);
	void freeBuffers();
	void prepareQueues();
	bool createCamera();
	MIL_ID getFreeBuffer();

	Ui::QtGigECamClass ui;

	CaptureThread *m_captureThread;
	
	QMutex m_frameQMutex;
	QQueue <MIL_ID> m_frameQueue;
	QMutex m_freeQMutex;
	QQueue <MIL_ID> m_freeQueue;

	int m_frameRateTimer;
	int m_frameRefreshTimer;
	QLabel *m_pStatus;
	int m_frameCount;
	int m_imgWidth;
	int m_imgHeight;
	int m_imgPitch;

	enum CameraType m_cameraType;
	Camera *m_camera;
	bool m_scaling;
	QLabel *m_cameraView;

	MIL_ID m_milApp;
	MIL_ID m_camera_buff;
};

#endif // QTGIGECAM_H
