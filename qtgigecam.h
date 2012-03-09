#ifndef QTGIGECAM_H
#define QTGIGECAM_H

//#include <mil.h>
#include <QtGui>
#include "ui_qtgigecam.h"
#include "camera.h"
#include "capturethread.h"


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
	void newImage(Mat frame);

protected:
	void timerEvent(QTimerEvent *event);

private:
	void showImage(Mat *frame);
	bool createCamera();

	Ui::QtGigECamClass ui;

	Camera *m_camera;
	CaptureThread *m_captureThread;
	
	QMutex m_frameQMutex;
	QQueue <Mat> m_frameQueue;

	int m_frameRateTimer;
	int m_frameRefreshTimer;
	QLabel *m_status;
	int m_frameCount;
	int m_imgWidth;
	int m_imgHeight;
	bool m_scaling;
};

#endif // QTGIGECAM_H
